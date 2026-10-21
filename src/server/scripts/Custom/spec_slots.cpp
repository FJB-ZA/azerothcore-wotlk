/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <array>
#include <string>

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GossipDef.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpecSlotsPolicy.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Trainer.h"
#include "WorldPacket.h"
#include "WorldSession.h"

// Spec slots: up to four talent groups per character (MAX_TALENT_SPECS in the core fork). Slot N is talent
// group N - 1. Slots 2-4 are bought from the player's class trainer; switching casts one of the custom spells
// 990010-990013, started from the trainer or from the SpecSlots addon (patches/client/subclasses).
namespace
{
// Gossip sender for our trainer options; the trainer's own options use sender 0.
constexpr uint32 GossipSender = 0x534C;
// Gossip action for buying slot n is BuyAction + n; switching to slot n is n.
constexpr uint32 BuyAction = 100;

// Slots 2-4 in copper, read once per config load.
std::array<uint32, 3> prices = {SpecSlots::DefaultPrice[0], SpecSlots::DefaultPrice[1], SpecSlots::DefaultPrice[2]};

// Bot sessions have no client socket, and the core names them "headless".
bool IsBot(Player* player) { return player->GetSession()->GetRemoteAddress() == "headless"; }

uint8 Owned(Player* player) { return player->GetSpecsCount(); }
uint8 Active(Player* player) { return player->GetActiveSpec() + 1; }

// The price of the next slot, or 0 when all four are owned.
uint32 NextPrice(Player* player)
{
    uint8 const owned = Owned(player);
    return owned >= 1 && owned < SpecSlots::MaxSlots ? prices[owned - 1] : 0;
}

std::string Gold(uint32 copper) { return std::to_string(copper / GOLD) + " gold"; }

void Say(Player* player, std::string const& text) { ChatHandler(player->GetSession()).SendSysMessage(text); }

// A trainer of the player's own class; the core's dual spec used the same test via Creature::CanResetTalents.
bool IsClassTrainerFor(Creature* creature, Player* player)
{
    Trainer::Trainer const* trainer = sObjectMgr->GetTrainer(creature->GetEntry());
    return trainer && trainer->GetTrainerType() == Trainer::Type::Class && trainer->IsTrainerValidForPlayer(player);
}

// Talent points spent in each tree of a talent group
SpecSlots::TreePoints PointsIn(Player* player, uint8 group)
{
    uint32 points[3] = {0, 0, 0};
    for (auto const& [spellId, talent] : player->GetTalentMap())
    {
        if (talent->State == PLAYERSPELL_REMOVED || !talent->IsInSpec(group))
            continue;
        TalentSpellPos const* pos = GetTalentSpellPos(spellId);
        TalentEntry const* entry = pos ? sTalentStore.LookupEntry(pos->talent_id) : nullptr;
        TalentTabEntry const* tab = entry ? sTalentTabStore.LookupEntry(entry->TalentTab) : nullptr;
        if (tab && tab->tabpage < 3)
            points[tab->tabpage] += pos->rank + 1;
    }
    return {uint8(points[0]), uint8(points[1]), uint8(points[2])};
}

std::string Spread(SpecSlots::TreePoints points)
{
    return std::to_string(points.a) + '/' + std::to_string(points.b) + '/' + std::to_string(points.c);
}

// Hidden addon message to the player's own SpecSlots addon
void SendState(Player* player)
{
    if (IsBot(player))
        return;

    std::array<SpecSlots::TreePoints, SpecSlots::MaxSlots> points{};
    for (uint8 group = 0; group < Owned(player) && group < SpecSlots::MaxSlots; ++group)
        points[group] = PointsIn(player, group);

    WorldPacket data;
    std::string const message = std::string(SpecSlots::AddonPrefix) + '\t' +
                                SpecSlots::FormatState(Active(player), Owned(player), NextPrice(player), points);
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
    player->SendDirectMessage(&data);
}

SpecSlots::SwapCheck CheckSwap(Player* player, uint8 slot)
{
    return SpecSlots::CanSwap(slot, Owned(player), Active(player), player->IsInCombat(), player->InArena());
}

// Starts the switch cast, or says why it can't start.
void TrySwap(Player* player, uint8 slot)
{
    std::string const n = std::to_string(slot);
    switch (CheckSwap(player, slot))
    {
        case SpecSlots::SwapCheck::Ok:
            player->CastSpell(player, SpecSlots::SwapSpell(slot), false);
            break;
        case SpecSlots::SwapCheck::NotOwned:
            Say(player, "You haven't bought spec slot " + n + " yet. Any trainer of your class sells it.");
            break;
        case SpecSlots::SwapCheck::AlreadyActive:
            Say(player, "Spec slot " + n + " is already active.");
            break;
        case SpecSlots::SwapCheck::InCombat:
            Say(player, "You can't switch spec slots in combat.");
            break;
        case SpecSlots::SwapCheck::InArena:
            Say(player, "You can't switch spec slots in an arena.");
            break;
    }
}

void TryBuy(Player* player, uint8 slot)
{
    uint8 const owned = Owned(player);
    uint32 const price = NextPrice(player);
    std::string const n = std::to_string(slot);

    // A stale gossip window (double click, or an old menu) can only ever buy the next slot.
    if (slot != owned + 1 && owned < SpecSlots::MaxSlots)
        return;

    switch (SpecSlots::CanBuy(player->GetLevel(), owned, player->GetMoney(), price))
    {
        case SpecSlots::BuyCheck::Ok:
            player->ModifyMoney(-int32(price));
            player->UpdateSpecCount(owned + 1);
            player->SaveToDB(false, false);
            Say(player, "Spec slot " + n + " is yours. Switch to it here, or type /slots.");
            SendState(player);
            break;
        case SpecSlots::BuyCheck::TooLow:
            Say(player, "Spec slots open up at level 10.");
            break;
        case SpecSlots::BuyCheck::AllOwned:
            Say(player, "You already have all four spec slots.");
            break;
        case SpecSlots::BuyCheck::NoMoney:
            Say(player, "Spec slot " + n + " costs " + Gold(price) + ".");
            break;
    }
}
}  // namespace

class SpecSlotsWorldScript : public WorldScript
{
public:
    SpecSlotsWorldScript()
        : WorldScript("SpecSlotsWorldScript", {WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_AFTER_CONFIG_LOAD})
    {
    }

    // Scripts load after the first config read, so startup reads it too.
    void OnStartup() override { Load(); }
    void OnAfterConfigLoad(bool /*reload*/) override { Load(); }

private:
    static void Load()
    {
        for (uint8 i = 0; i < prices.size(); ++i)
            prices[i] = sConfigMgr->GetOption<uint32>("SpecSlots.Price.Slot" + std::to_string(i + 2), SpecSlots::DefaultPrice[i], false);
    }
};

class SpecSlotsPlayerScript : public PlayerScript
{
public:
    SpecSlotsPlayerScript()
        : PlayerScript("SpecSlotsPlayerScript",
                       {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT, PLAYERHOOK_ON_AFTER_SPEC_SLOT_CHANGED})
    {
    }

    void OnPlayerLogin(Player* player) override { SendState(player); }

    // The addon whispers its commands to the player themself: "SoloWowSlots\tQ" or "SoloWowSlots\tW <slot>".
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        std::string const prefix = std::string(SpecSlots::AddonPrefix) + '\t';
        if (language != LANG_ADDON || type != CHAT_MSG_WHISPER || receiver != player || msg.rfind(prefix, 0) != 0)
            return true;

        if (auto const command = SpecSlots::ParseCommand(std::string_view(msg).substr(prefix.size())))
        {
            if (command->kind == SpecSlots::Command::Kind::Query)
                SendState(player);
            else
                TrySwap(player, command->slot);
        }
        return false;
    }

    // The switch spell finished. Commit the slot now, so a crash can't bring back the old one.
    void OnPlayerAfterSpecSlotChanged(Player* player, uint8 group) override
    {
        CharacterDatabase.Execute("UPDATE characters SET activeTalentGroup = {}, talentGroupsCount = {} WHERE guid = {}",
                                  uint32(group), uint32(player->GetSpecsCount()), player->GetGUID().GetCounter());
        if (IsBot(player))
            return;
        Say(player, "You're now using spec slot " + std::to_string(group + 1) + ".");
        SendState(player);
    }
};

class SpecSlotsCreatureScript : public AllCreatureScript
{
public:
    SpecSlotsCreatureScript() : AllCreatureScript("SpecSlotsCreatureScript") { }

    bool CanCreatureGossipHello(Player* player, Creature* creature) override
    {
        // Scripted trainers keep their own gossip.
        if (IsBot(player) || creature->GetScriptId() || !IsClassTrainerFor(creature, player))
            return false;

        player->PrepareGossipMenu(creature, creature->GetGossipMenuId(), true);
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        uint8 const owned = Owned(player);
        if (owned < SpecSlots::MaxSlots)
        {
            std::string const n = std::to_string(owned + 1);
            menu.AddMenuItem(-1, GOSSIP_ICON_TRAINER, "Buy spec slot " + n + ".", GossipSender, BuyAction + owned + 1,
                             "Spec slot " + n + " keeps its own talents, glyphs and action bars. Buy it?", NextPrice(player));
        }

        for (uint8 slot = 1; slot <= owned && slot <= SpecSlots::MaxSlots; ++slot)
        {
            if (slot == Active(player))
                continue;
            SpecSlots::TreePoints const points = PointsIn(player, slot - 1);
            std::string const what = points.a + points.b + points.c ? Spread(points) : "no talents yet";
            menu.AddMenuItem(-1, GOSSIP_ICON_TRAINER, "Switch to spec slot " + std::to_string(slot) + " (" + what + ").",
                             GossipSender, slot, "", 0);
        }

        player->SendPreparedGossip(creature);
        return true;
    }

    bool CanCreatureGossipSelect(Player* player, Creature* /*creature*/, uint32 sender, uint32 action) override
    {
        if (sender != GossipSender)
            return false;

        // Close first, so a second click on the same window finds nothing to buy.
        player->PlayerTalkClass->SendCloseGossip();
        if (action > BuyAction && action <= BuyAction + SpecSlots::MaxSlots)
            TryBuy(player, uint8(action - BuyAction));
        else if (action >= 1 && action <= SpecSlots::MaxSlots)
            TrySwap(player, uint8(action));
        return true;
    }
};

class SpecSlotsSpellScript : public AllSpellScript
{
public:
    SpecSlotsSpellScript() : AllSpellScript("SpecSlotsSpellScript", {ALLSPELLHOOK_CAN_SELECT_SPEC_TALENT}) { }

    // However the switch spell was started, it only goes through when the rules allow it.
    bool CanSelectSpecTalent(Spell* spell) override
    {
        uint8 const slot = SpecSlots::SlotOfSwapSpell(spell->GetSpellInfo()->Id);
        if (!slot)
            return true;
        Player* player = spell->GetCaster() ? spell->GetCaster()->ToPlayer() : nullptr;
        return player && CheckSwap(player, slot) == SpecSlots::SwapCheck::Ok;
    }
};

void AddSC_spec_slots()
{
    new SpecSlotsWorldScript();
    new SpecSlotsPlayerScript();
    new SpecSlotsCreatureScript();
    new SpecSlotsSpellScript();
}
