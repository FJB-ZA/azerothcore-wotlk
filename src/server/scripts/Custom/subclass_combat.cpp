/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SubclassPaths.h"
#include "UnitScript.h"

// Sub-class path tuning in combat: each path's damage and healing modifiers, split separately for PvE and PvP, and the
// melee share of a pet split into several bodies.
namespace
{
// Melee is the only attack every body makes; the pet's own spells stay whole.
float DamageFactor(Unit const* attacker, Unit const* target, bool melee)
{
    if (!attacker || !target)
        return 1.0f;
    Player const* owner = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!owner)
        return 1.0f;
    SubclassPaths::Path const* path = SubclassPaths::ActivePath(owner);
    uint8 const bodies = melee && attacker != owner ? SubclassPaths::MeleeBodies(attacker) : 1;
    if (!path && bodies == 1)
        return 1.0f;
    return SubclassPaths::DamageFactor(path ? path->modifiers : SubclassPaths::Modifiers{},
                                       target->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr, bodies);
}

float HealingFactor(Unit const* healer)
{
    Player const* owner = healer ? healer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    SubclassPaths::Path const* path = owner ? SubclassPaths::ActivePath(owner) : nullptr;
    if (!path)
        return 1.0f;
    return SubclassPaths::HealingFactor(path->modifiers, owner->InBattleground() || owner->InArena());
}

bool IsPeriodicHeal(SpellInfo const* spellInfo)
{
    return spellInfo && (spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL) || spellInfo->HasAura(SPELL_AURA_OBS_MOD_HEALTH));
}

// A leech heals for the damage it did, which the damage modifier already scaled.
bool IsLeech(SpellInfo const* spellInfo)
{
    return spellInfo && (spellInfo->HasAura(SPELL_AURA_PERIODIC_LEECH) || spellInfo->HasEffect(SPELL_EFFECT_HEALTH_LEECH));
}
}  // namespace

class SubclassCombatUnitScript : public UnitScript
{
public:
    SubclassCombatUnitScript()
        : UnitScript("SubclassCombatUnitScript", true,
                     {UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
                      UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_HEAL_RECEIVED})
    {
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (SubclassPaths::AffectsCombat())
            damage = uint32(damage * DamageFactor(attacker, target, true));
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        if (SubclassPaths::AffectsCombat())
            damage = int32(damage * DamageFactor(attacker, target, false));
    }

    // Also called for periodic heal ticks, which ModifyHealReceived scales instead.
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        if (SubclassPaths::AffectsCombat() && !IsPeriodicHeal(spellInfo))
            damage = uint32(damage * DamageFactor(attacker, target, false));
    }

    void ModifyHealReceived(Unit* /*target*/, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        if (SubclassPaths::AffectsCombat() && !IsLeech(spellInfo))
            heal = uint32(heal * HealingFactor(healer));
    }
};

void AddSC_subclass_combat()
{
    new SubclassCombatUnitScript();
}
