/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <set>
#include <string>
#include <vector>

#include "Bag.h"
#include "Chat.h"
#include "Creature.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "Item.h"
#include "MapMgr.h"
#include "MythicItemPolicy.h"
#include "MythicPlus.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"

// npc_mythic_plus, the Keystone Master in the Dalaran emblem rooms: teleports the group to a dungeon one of them,
// bots included, holds a key for, hands out a first key, opens last week's vault and runs the Crest Forge.
namespace
{
using namespace MythicPlus;

constexpr uint32 MainSender = GOSSIP_SENDER_MAIN + 43;
constexpr uint32 ForgeSender = GOSSIP_SENDER_MAIN + 44;
enum MainAction : uint32
{
    ActionStarterKey = 1,
    ActionVault = 2,
    ActionForge = 3,
    ActionAffixes = 4,
    ActionCrests = 5,
    // Plus the dungeon's map id
    ActionTeleport = 1000
};
constexpr float BringRange = 30.0f;
constexpr uint32 MaxForgeOptions = GOSSIP_MAX_MENU_ITEMS - 1;

std::string DungeonName(uint16 map)
{
    DungeonInfo const* dungeon = FindDungeon(map);
    return dungeon ? dungeon->name : "an unknown dungeon";
}

std::vector<Player*> GroupNear(Player* player, Creature* master)
{
    std::vector<Player*> members{player};
    Group* group = player->GetGroup();
    if (!group)
        return members;
    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (member && member != player && member->IsInWorld() && member->IsWithinDistInMap(master, BringRange))
            members.push_back(member);
    }
    return members;
}

void AddTeleports(Player* player, Creature* master)
{
    std::set<uint16> offered;
    auto const offer = [&](Player* holder)
    {
        Keystone const key = GetKeystone(holder);
        if (!key.map || !offered.insert(key.map).second)
            return;
        std::string const whose = holder == player ? "your" : holder->GetName() + "'s";
        AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Take us to " + DungeonName(key.map) + " (" + whose + " keystone +" +
                                                       std::to_string(key.level) + ").",
                         MainSender, ActionTeleport + key.map);
    };
    for (Player* member : GroupNear(player, master))
        offer(member);
}

// A finished or abandoned heroic lockout would send the group back into a spent dungeon; `player` may be offline
void ClearEmptyLockout(ObjectGuid guid, uint16 map, Player* player)
{
    InstancePlayerBind* bind = sInstanceSaveMgr->PlayerGetBoundInstance(guid, map, DUNGEON_DIFFICULTY_HEROIC);
    if (!bind || !bind->save)
        return;
    Map* instance = sMapMgr->FindMap(map, bind->save->GetInstanceId());
    if (!instance || !instance->GetPlayersCountExceptGMs())
        sInstanceSaveMgr->PlayerUnbindInstance(guid, map, DUNGEON_DIFFICULTY_HEROIC, true, player);
}

void Teleport(Player* player, Creature* master, uint16 map)
{
    ChatHandler handler(player->GetSession());
    DungeonInfo const* dungeon = FindDungeon(map);
    if (!dungeon)
        return;
    if (player->GetLevel() < HeroicLevel)
    {
        handler.SendNotification("Mythic+ dungeons need level " + std::to_string(HeroicLevel) + ".");
        return;
    }
    if (Group* group = player->GetGroup())
    {
        if (group->GetDungeonDifficulty() != DUNGEON_DIFFICULTY_HEROIC)
        {
            if (!group->IsLeader(player->GetGUID()))
            {
                handler.SendNotification("Ask your group leader to set the dungeon difficulty to heroic.");
                return;
            }
            group->SetDungeonDifficulty(DUNGEON_DIFFICULTY_HEROIC);
        }
    }
    else if (player->GetDungeonDifficulty() != DUNGEON_DIFFICULTY_HEROIC)
    {
        player->SetDungeonDifficulty(DUNGEON_DIFFICULTY_HEROIC);
        player->SendDungeonDifficulty(false);
    }
    // The group follows its leader's bind, and the leader may be anywhere
    if (Group* group = player->GetGroup())
        ClearEmptyLockout(group->GetLeaderGUID(), map, ObjectAccessor::FindConnectedPlayer(group->GetLeaderGUID()));
    for (Player* member : GroupNear(player, master))
    {
        if (!member->IsAlive() || member->IsInCombat() || member->GetLevel() < HeroicLevel)
            continue;
        ClearEmptyLockout(member->GetGUID(), map, member);
        member->TeleportTo(dungeon->map, dungeon->x, dungeon->y, dungeon->z, dungeon->o);
    }
}

void GiveStarterKey(Player* player)
{
    ChatHandler handler(player->GetSession());
    if (player->GetLevel() < HeroicLevel)
    {
        handler.SendNotification("Come back at level " + std::to_string(HeroicLevel) + ".");
        return;
    }
    if (GetKeystone(player).map)
        return;
    uint16 const map = Dungeons[urand(0, uint32(Dungeons.size() - 1))].map;
    SetKeystone(player, map, MinKeyLevel);
    handler.SendSysMessage("Your first keystone: " + DungeonName(map) + " +" + std::to_string(MinKeyLevel) + ".");
}

void ShowAffixes(Player* player)
{
    uint32 const week = WeekIndex(uint64(GameTime::GetGameTime().count()));
    WeekAffixes const affixes = AffixesForWeek(week);
    ChatHandler(player->GetSession())
        .SendSysMessage(std::string("This week's affixes: +4 ") + AffixName(affixes.plus4) + ", +7 " +
                        AffixName(affixes.plus7) + ", +10 " + AffixName(affixes.plus10) + ".");
}

void ShowCrests(Player* player)
{
    CrestWallet const owned = Crests(player);
    CrestWallet const earned = EarnedThisWeek(player);
    std::string text = "Your crests:";
    for (uint8 tier = 1; tier <= CrestTiers; ++tier)
        text += std::string(" ") + CrestName(Crest(tier)) + " " + std::to_string(owned[tier - 1]) + " (" +
                std::to_string(earned[tier - 1]) + "/" + std::to_string(WeeklyCrestCap) + " this week)" +
                (tier < CrestTiers ? "," : ".");
    ChatHandler(player->GetSession()).SendSysMessage(text);
}

uint32 ForgeAction(uint8 bag, uint8 slot)
{
    return (uint32(bag) << 8) | slot;
}

void AddForgeItem(Player* player, Item* item, uint8 bag, uint8 slot, uint32& offered)
{
    if (!item || offered >= MaxForgeOptions || !IsMythicItem(item->GetEntry()))
        return;
    std::string const cost = UpgradeCostText(item->GetEntry());
    if (cost.empty())
        return;
    std::optional<RankRef> const ref = DecodeRankId(item->GetEntry());
    std::string const name = item->GetTemplate()->Name1 + " (Mythic " + std::to_string(ref->rank) + ")";
    AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, name + " to " + cost, ForgeSender, ForgeAction(bag, slot),
                     "Upgrade " + name + " to " + cost + "?", 0, false);
    ++offered;
}

void ShowForge(Player* player, Creature* master)
{
    ClearGossipMenuFor(player);
    uint32 offered = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        AddForgeItem(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot), INVENTORY_SLOT_BAG_0, slot, offered);
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        AddForgeItem(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot), INVENTORY_SLOT_BAG_0, slot, offered);
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        if (Bag* bag = player->GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                AddForgeItem(player, bag->GetItemByPos(uint8(slot)), bagSlot, uint8(slot), offered);
    if (!offered)
        ChatHandler(player->GetSession()).SendNotification("You carry no Mythic+ item that can go higher.");
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, master->GetGUID());
}
}

class npc_mythic_plus : public CreatureScript
{
public:
    npc_mythic_plus() : CreatureScript("npc_mythic_plus") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        AddTeleports(player, creature);
        if (!GetKeystone(player).map)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "I need a keystone.", MainSender, ActionStarterKey);
        if (std::optional<VaultReward> const reward = PendingVault(player))
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Open the Great Vault (last week's best: +" +
                                                                std::to_string(reward->bestLevel) + ").",
                             MainSender, ActionVault);
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "Upgrade my Mythic+ gear with crests.", MainSender,
                         ActionForge);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "What are this week's affixes?", MainSender, ActionAffixes);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "How many crests do I have?", MainSender, ActionCrests);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (sender == ForgeSender)
        {
            std::string const reason = UpgradeItem(player, uint8(action >> 8), uint8(action & 0xFF));
            if (!reason.empty())
                ChatHandler(player->GetSession()).SendNotification(reason);
            ShowForge(player, creature);
            return true;
        }
        if (sender != MainSender)
        {
            CloseGossipMenuFor(player);
            return true;
        }
        switch (action)
        {
            case ActionForge:
                ShowForge(player, creature);
                return true;
            case ActionStarterKey:
                GiveStarterKey(player);
                break;
            case ActionVault:
                if (std::string const reason = OpenVault(player); !reason.empty())
                    ChatHandler(player->GetSession()).SendNotification(reason);
                break;
            case ActionAffixes:
                ShowAffixes(player);
                break;
            case ActionCrests:
                ShowCrests(player);
                break;
            default:
                if (action > ActionTeleport)
                {
                    CloseGossipMenuFor(player);
                    Teleport(player, creature, uint16(action - ActionTeleport));
                    return true;
                }
                break;
        }
        OnGossipHello(player, creature);
        return true;
    }
};

void AddSC_mythic_npc()
{
    new npc_mythic_plus();
}
