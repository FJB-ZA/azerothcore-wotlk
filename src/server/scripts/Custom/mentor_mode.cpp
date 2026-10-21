/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>

#include "Chat.h"
#include "Config.h"
#include "CreatureData.h"
#include "Group.h"
#include "MentorModePolicy.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellInfo.h"

// Mentor mode: while grouped with members at least MentorMode::MinGap levels lower on the same map, a
// player fights as the lowest member's level + MentorMode::Margin. The core reads the mentor level for
// level-dependent combat rules (hit, glancing, resists, skill caps, aggro, kill XP); this script scales
// health, damage and healing, sets the armour scale and switches the mode on and off.
namespace
{
// Read once per config load; MentorMode.Enable is optional in worldserver.conf.
bool enabled = true;

bool Enabled() { return enabled; }

CreatureBaseStats const* BaseStats(uint8 level) { return sObjectMgr->GetCreatureBaseStats(level, CLASS_WARRIOR); }

float BaseHealth(uint8 level)
{
    CreatureBaseStats const* stats = BaseStats(level);
    return stats ? float(stats->BaseHealth[MentorMode::StatColumn(level)]) : 0.0f;
}

float BaseArmor(uint8 level)
{
    CreatureBaseStats const* stats = BaseStats(level);
    return stats ? stats->BaseArmor : 0.0f;
}

// Health, damage and healing scale for a mentoring player (1 when not mentoring).
float PowerScale(Player const* player)
{
    uint8 const mentor = player->GetMentorLevel();
    return mentor ? MentorMode::Scale(BaseHealth(mentor), BaseHealth(player->GetLevel())) : 1.0f;
}

// The mentoring player behind a unit (the player itself, or the owner of its pet or totem).
Player* MentorOf(Unit* unit)
{
    Player* player = unit ? unit->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    return player && player->GetMentorLevel() ? player : nullptr;
}

uint8 DesiredMentorLevel(Player* player)
{
    if (!Enabled() || player->InBattleground() || player->InArena())
        return 0;
    Group* group = player->GetGroup();
    if (!group)
        return 0;
    uint8 lowest = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource())
            if (member != player && member->IsInWorld() && member->GetMapId() == player->GetMapId() &&
                (!lowest || member->GetLevel() < lowest))
                lowest = member->GetLevel();
    return MentorMode::TargetLevel(player->GetLevel(), lowest);
}

void ApplyMentorLevel(Player* player, uint8 level)
{
    if (player->GetMentorLevel() == level)
        return;
    float const healthPct = player->GetHealthPct();
    float const armorScale = level ? MentorMode::Scale(BaseArmor(level), BaseArmor(player->GetLevel())) : 1.0f;
    player->SetMentorLevel(level, armorScale);
    player->UpdateArmor();
    player->UpdateMaxHealth();
    if (player->IsAlive())
        player->SetHealth(std::max<uint32>(1, uint32(player->GetMaxHealth() * healthPct / 100.0f)));
    // Bots mentor too (they join lower players' groups), but have no screen to tell.
    if (player->GetSession()->IsSocketClosed())
        return;
    std::string const message = level ? "Mentor mode: you now fight as level " + std::to_string(level) +
                                            " alongside your lower-level friends."
                                      : "Mentor mode ended: you're back to full strength.";
    ChatHandler(player->GetSession()).SendSysMessage(message);
    player->GetSession()->SendAreaTriggerMessage(message);
}

// Keeps the Mentoring debuff in step with the mentor level, so it comes back after anything strips it and a
// saved one is dropped at login once mentoring is over. Without the spell data loaded, AddAura does nothing.
void SyncAura(Player* player)
{
    uint8 const level = player->GetMentorLevel();
    if (!level)
    {
        player->RemoveAurasDueToSpell(MentorMode::AuraSpell);
        return;
    }
    Aura* aura = player->GetAura(MentorMode::AuraSpell);
    if (!aura)
        aura = player->AddAura(MentorMode::AuraSpell, player);
    if (aura && aura->GetStackAmount() != level)
        aura->SetStackAmount(level);
}

void Update(Player* player)
{
    ApplyMentorLevel(player, DesiredMentorLevel(player));
    SyncAura(player);
}
}  // namespace

class MentorModeWorldScript : public WorldScript
{
public:
    MentorModeWorldScript()
        : WorldScript("MentorModeWorldScript", {WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_AFTER_CONFIG_LOAD})
    {
    }

    // Scripts load after the first config read, so startup reads it too.
    void OnStartup() override { Load(); }
    void OnAfterConfigLoad(bool /*reload*/) override { Load(); }

private:
    static void Load() { enabled = sConfigMgr->GetOption<bool>("MentorMode.Enable", true, false); }
};

class MentorModePlayerScript : public PlayerScript
{
public:
    MentorModePlayerScript()
        : PlayerScript("MentorModePlayerScript",
                       {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_AFTER_UPDATE, PLAYERHOOK_ON_AFTER_UPDATE_MAX_HEALTH,
                        PLAYERHOOK_ON_BEFORE_GET_LEVEL_FOR_XP_GAIN, PLAYERHOOK_ON_GIVE_EXP})
    {
    }

    void OnPlayerLogin(Player* player) override { Update(player); }

    // Re-checked every 2 seconds: group changes, level-ups and map changes all settle within that.
    void OnPlayerAfterUpdate(Player* player, uint32 diff) override
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            uint32& timer = _timers[player->GetGUID().GetCounter()];
            timer += diff;
            if (timer < MentorMode::CheckIntervalMs)
                return;
            timer = 0;
        }
        Update(player);
    }

    void OnPlayerAfterUpdateMaxHealth(Player* player, float& value) override
    {
        if (player->GetMentorLevel())
            value *= PowerScale(player);
    }

    // Group kill XP counts the mentor at the mentored level, so lower members earn as if grouped with equals.
    void OnPlayerBeforeGetLevelForXPGain(Player const* player, uint8& level) override
    {
        if (player->GetMentorLevel())
            level = player->GetMentorLevel();
    }

    // The mentor is there to help: the mentored level would otherwise pay out full XP for grey content.
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 /*xpSource*/) override
    {
        if (player->GetMentorLevel())
            amount = 0;
    }

private:
    std::mutex _mutex;
    std::unordered_map<uint32, uint32> _timers;
};

class MentorModeUnitScript : public UnitScript
{
public:
    MentorModeUnitScript()
        : UnitScript("MentorModeUnitScript", true,
                     {UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
                      UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_HEAL_RECEIVED})
    {
    }

    void ModifyMeleeDamage(Unit* /*target*/, Unit* attacker, uint32& damage) override
    {
        if (Player* mentor = MentorOf(attacker))
            damage = uint32(damage * PowerScale(mentor));
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        if (Player* mentor = MentorOf(attacker))
            damage = int32(damage * PowerScale(mentor));
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage,
                                       SpellInfo const* /*spellInfo*/) override
    {
        if (Player* mentor = MentorOf(attacker))
            damage = uint32(damage * PowerScale(mentor));
    }

    void ModifyHealReceived(Unit* /*target*/, Unit* healer, uint32& heal, SpellInfo const* /*spellInfo*/) override
    {
        if (Player* mentor = MentorOf(healer))
            heal = uint32(heal * PowerScale(mentor));
    }
};

void AddSC_mentor_mode()
{
    new MentorModeWorldScript();
    new MentorModePlayerScript();
    new MentorModeUnitScript();
}
