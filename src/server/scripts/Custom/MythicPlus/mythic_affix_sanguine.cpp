/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Creature.h"
#include "MythicMechanicPolicy.h"
#include "MythicPlus.h"
#include "ScriptMgr.h"

// Sanguine: a non-boss enemy leaves a pool of ichor where it dies, healing its allies and hurting players for
// 20 seconds, so tanks drag enemies out of it.
using namespace MythicPlus;

class MythicSanguineUnitScript : public UnitScript
{
public:
    MythicSanguineUnitScript() : UnitScript("MythicSanguineUnitScript", true, {UNITHOOK_ON_UNIT_DEATH}) { }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        Creature* dead = unit->ToCreature();
        MythicRun* run = dead ? ActiveRun(dead->GetMap()) : nullptr;
        if (!run || !run->Has(Affix::Sanguine) || IsBoss(dead) || !IsEnemyKind(dead))
            return;
        CastGround(dead, dead->GetPosition(), SpellSanguineIchor, ScaledDamage(SanguineTickDamage, run->level), PoolMs);
    }
};

void AddSC_mythic_affix_sanguine()
{
    new MythicSanguineUnitScript();
}
