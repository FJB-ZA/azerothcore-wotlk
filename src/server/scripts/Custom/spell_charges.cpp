/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <string>
#include <unordered_map>

#include "Chat.h"
#include "DataMap.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellChargesPolicy.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"

// Spell charges (SpellChargesPolicy.h) for the spells in solo_wow_spell_charges, whose DBC cooldown must be 0.
namespace
{
struct Rule
{
    uint8 max = 0;
    uint32 rechargeMs = 0;
};

std::unordered_map<uint32, Rule> rules;

// Each player's charges, kept with the player until logout
class PlayerCharges : public DataMap::Base
{
public:
    std::unordered_map<uint32, SpellCharges::State> states;
};

// Bot sessions have no client socket, and the core names them "headless".
bool IsBot(Player* player) { return player->GetSession()->GetRemoteAddress() == "headless"; }

uint64 Now() { return GameTime::GetGameTimeMS().count(); }

// The rule's recharge time after the player's cooldown modifiers (glyphs, talents, set bonuses)
uint32 RechargeMs(Player* player, uint32 spellId, Rule const& rule)
{
    int32 recharge = int32(rule.rechargeMs);
    player->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, recharge);
    return recharge > 0 ? uint32(recharge) : 0;
}

SpellCharges::State& CurrentState(Player* player, uint32 spellId, Rule const& rule, uint64 now)
{
    auto& states = player->CustomData.GetDefault<PlayerCharges>("SpellCharges")->states;
    auto found = states.find(spellId);
    if (found == states.end())
    {
        uint32 const cooldown = player->GetSpellCooldownDelay(spellId);
        found = states.emplace(spellId, SpellCharges::FromCooldown(rule.max, now, cooldown)).first;
    }
    found->second = SpellCharges::Refresh(found->second, rule.max, RechargeMs(player, spellId, rule), now);
    // An expired cooldown stays in the map until removed or saved, so a missing entry means something cleared it.
    bool const cooldownKept = player->GetSpellCooldownMap().count(spellId) != 0;
    found->second = SpellCharges::Reconcile(found->second, rule.max, cooldownKept, now);
    return found->second;
}

void SendState(Player* player, uint32 spellId, Rule const& rule)
{
    if (IsBot(player))
        return;
    uint64 const now = Now();
    SpellCharges::State const state = CurrentState(player, spellId, rule, now);
    std::string const body = SpellCharges::Format(spellId, state, rule.max, RechargeMs(player, spellId, rule), now);
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
                                 std::string(SpellCharges::AddonPrefix) + '\t' + body);
    player->SendDirectMessage(&data);
}

void SendKnown(Player* player)
{
    for (auto const& [spellId, rule] : rules)
        if (player->HasSpell(spellId))
            SendState(player, spellId, rule);
}

void Load()
{
    rules.clear();
    QueryResult result = WorldDatabase.Query("SELECT spell, max_charges, recharge_ms FROM solo_wow_spell_charges");
    if (!result)
        return;
    do
    {
        Field* fields = result->Fetch();
        uint32 const spellId = fields[0].Get<uint32>();
        Rule const rule{fields[1].Get<uint8>(), fields[2].Get<uint32>()};
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || !rule.max)
        {
            LOG_ERROR("sql.sql", "solo_wow_spell_charges: spell {} is missing or has no charges; skipped.", spellId);
            continue;
        }
        if (spellInfo->RecoveryTime || spellInfo->CategoryRecoveryTime)
            LOG_ERROR("sql.sql", "solo_wow_spell_charges: spell {} still has a DBC cooldown, so the client puts it on "
                      "cooldown after every charge.", spellId);
        rules[spellId] = rule;
    } while (result->NextRow());
    LOG_INFO("server.loading", ">> Loaded {} spells with charges", rules.size());
}
}

class SpellChargesWorldScript : public WorldScript
{
public:
    SpellChargesWorldScript() : WorldScript("SpellChargesWorldScript", {WORLDHOOK_ON_STARTUP}) { }

    void OnStartup() override { Load(); }
};

class SpellChargesSpellScript : public AllSpellScript
{
public:
    SpellChargesSpellScript() : AllSpellScript("SpellChargesSpellScript", {ALLSPELLHOOK_ON_CAST}) { }

    // After a successful cast: spend a charge, and put the spell on cooldown when that was the last one.
    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player || spell->IsTriggered() || player->GetCommandStatus(CHEAT_COOLDOWN))
            return;
        auto const rule = rules.find(spellInfo->Id);
        if (rule == rules.end())
            return;

        uint64 const now = Now();
        SpellCharges::State& state = CurrentState(player, spellInfo->Id, rule->second, now);
        state = SpellCharges::Spend(state, rule->second.max, RechargeMs(player, spellInfo->Id, rule->second), now);
        if (uint32 const cooldown = SpellCharges::CooldownMs(state, now))
        {
            player->AddSpellCooldown(spellInfo->Id, 0, cooldown, true);
            WorldPacket data;
            player->BuildCooldownPacket(data, SPELL_COOLDOWN_FLAG_NONE, spellInfo->Id, cooldown);
            player->SendDirectMessage(&data);
        }
        SendState(player, spellInfo->Id, rule->second);
    }
};

class SpellChargesPlayerScript : public PlayerScript
{
public:
    SpellChargesPlayerScript()
        : PlayerScript("SpellChargesPlayerScript",
                       {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEARN_SPELL, PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT})
    {
    }

    void OnPlayerLogin(Player* player) override { SendKnown(player); }

    void OnPlayerLearnSpell(Player* player, uint32 spellId) override
    {
        auto const rule = rules.find(spellId);
        if (rule != rules.end())
            SendState(player, spellId, rule->second);
    }

    // The client asks again (after a UI reload) with a hidden whisper to the player themself: "SoloWowCharges\tQ".
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        std::string const prefix = std::string(SpellCharges::AddonPrefix) + '\t';
        if (language != LANG_ADDON || type != CHAT_MSG_WHISPER || receiver != player || msg.rfind(prefix, 0) != 0)
            return true;

        if (SpellCharges::IsQuery(std::string_view(msg).substr(prefix.size())))
            SendKnown(player);
        return false;
    }
};

void AddSC_spell_charges()
{
    new SpellChargesWorldScript();
    new SpellChargesSpellScript();
    new SpellChargesPlayerScript();
}
