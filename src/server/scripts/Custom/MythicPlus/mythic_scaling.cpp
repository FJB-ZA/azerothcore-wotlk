/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Creature.h"
#include "Map.h"
#include "MythicPlus.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"

// Mythic+ scaling: enemy health and damage grow with the key level over heroic, with Fortified on trash and
// Tyrannical on bosses. Damage grows only against players and their pets.
namespace MythicPlus
{
namespace
{
constexpr char const* ScaleKey = "MythicPlusScale";

struct CreatureScale : public DataMap::Base
{
    float damage = 1.0f;
};

// Friendly NPCs with gossip, pets, triggers and the run's own helpers stay as they are
bool Scalable(Creature* creature)
{
    if (creature->IsPet() || creature->IsTotem() || creature->IsTrigger() || creature->IsCritter() ||
        creature->IsCivilian() || creature->GetCharmerOrOwnerGUID().IsPlayer() || creature->GetNpcFlags())
        return false;
    uint32 const entry = creature->GetEntry();
    return entry != ExplosiveOrbEntry && entry != RuneEntry && entry != KeystoneMasterEntry;
}

bool Scaling(MythicRun const* run)
{
    return run && (run->state == RunState::Countdown || run->Active());
}

float DamageMultiplier(Unit* target, Unit* attacker)
{
    Creature* creature = attacker ? attacker->ToCreature() : nullptr;
    if (!creature || !target || !target->GetCharmerOrOwnerPlayerOrPlayerItself())
        return 1.0f;
    CreatureScale const* scale = creature->CustomData.Get<CreatureScale>(ScaleKey);
    return scale ? scale->damage : 1.0f;
}
}

void ScaleCreature(Creature* creature, MythicRun const& run)
{
    if (!Scalable(creature) || creature->CustomData.Get<CreatureScale>(ScaleKey))
        return;
    bool const boss = IsBoss(creature);
    double const level = ScalingMultiplier(run.level);
    double health = level;
    double damage = level;
    if (boss && run.Has(Affix::Tyrannical))
    {
        health *= TyrannicalHealth;
        damage *= TyrannicalDamage;
    }
    else if (!boss && run.Has(Affix::Fortified))
    {
        health *= FortifiedHealth;
        damage *= FortifiedDamage;
    }
    creature->CustomData.GetDefault<CreatureScale>(ScaleKey)->damage = float(damage);
    creature->ApplyStatPctModifier(UNIT_MOD_HEALTH, TOTAL_PCT, float((health - 1.0) * 100.0));
    if (creature->IsAlive())
        creature->SetFullHealth();
}

void ScaleMap(Map* map, MythicRun const& run)
{
    for (auto const& [spawnId, creature] : map->GetCreatureBySpawnIdStore())
        ScaleCreature(creature, run);
}
}

using namespace MythicPlus;

class MythicScalingCreatureScript : public AllCreatureScript
{
public:
    MythicScalingCreatureScript() : AllCreatureScript("MythicScalingCreatureScript") { }

    void OnCreatureAddWorld(Creature* creature) override
    {
        MythicRun* run = FindRun(creature->GetMap());
        if (Scaling(run))
            ScaleCreature(creature, *run);
    }
};

class MythicScalingUnitScript : public UnitScript
{
public:
    MythicScalingUnitScript()
        : UnitScript("MythicScalingUnitScript", true,
                     {UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
                      UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK})
    {
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        damage = uint32(damage * DamageMultiplier(target, attacker));
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        damage = int32(damage * DamageMultiplier(target, attacker));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        // The same hook runs for periodic heals
        if (spellInfo && !spellInfo->IsPositive())
            damage = uint32(damage * DamageMultiplier(target, attacker));
    }
};

void AddSC_mythic_scaling()
{
    new MythicScalingCreatureScript();
    new MythicScalingUnitScript();
}
