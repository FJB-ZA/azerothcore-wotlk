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
#include "Player.h"
#include "ScriptMgr.h"

// Bursting: when a non-boss enemy dies, every player near it takes a stack of Burst, shadow damage each second for
// 4 seconds per stack, so a big pull killed at once hurts.
using namespace MythicPlus;

class MythicBurstingUnitScript : public UnitScript
{
public:
    MythicBurstingUnitScript() : UnitScript("MythicBurstingUnitScript", true, {UNITHOOK_ON_UNIT_DEATH}) { }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        Creature* dead = unit->ToCreature();
        MythicRun* run = dead ? ActiveRun(dead->GetMap()) : nullptr;
        if (!run || !run->Has(Affix::Bursting) || IsBoss(dead) || !IsEnemyKind(dead))
            return;
        // The spell adds its one die side to the base points
        int32 const basePoints = int32(ScaledDamage(BurstingTickDamage, run->level)) - 1;
        for (Player* player : RunPlayers(dead->GetMap(), false))
            if (player->IsWithinDistInMap(dead, DeathEffectRadius))
                player->CastCustomSpell(SpellBurst, SPELLVALUE_BASE_POINT0, basePoints, player, TRIGGERED_FULL_MASK);
    }
};

void AddSC_mythic_affix_bursting()
{
    new MythicBurstingUnitScript();
}
