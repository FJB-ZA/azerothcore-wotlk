/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Creature.h"
#include "MythicPlus.h"
#include "ScriptMgr.h"

// Bolstering: when a non-boss enemy dies, every enemy near it gains a stack of 20% health and damage.
using namespace MythicPlus;

class MythicBolsteringUnitScript : public UnitScript
{
public:
    MythicBolsteringUnitScript() : UnitScript("MythicBolsteringUnitScript", true, {UNITHOOK_ON_UNIT_DEATH}) { }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        Creature* dead = unit->ToCreature();
        MythicRun* run = dead ? ActiveRun(dead->GetMap()) : nullptr;
        if (!run || !run->Has(Affix::Bolstering) || IsBoss(dead) || !IsEnemyKind(dead))
            return;
        for (Creature* ally : EnemiesNear(dead, DeathEffectRadius))
            if (!IsBoss(ally))
                ally->AddAura(SpellBolstered, ally);
    }
};

void AddSC_mythic_affix_bolstering()
{
    new MythicBolsteringUnitScript();
}
