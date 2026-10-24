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
#include "Player.h"
#include "ScriptMgr.h"

// Necrotic: every enemy melee hit adds a stack of Necrotic Rot, 2% less healing taken, for 15 seconds, so tanks kite
// to let it fall off.
using namespace MythicPlus;

class MythicNecroticUnitScript : public UnitScript
{
public:
    MythicNecroticUnitScript() : UnitScript("MythicNecroticUnitScript", true, {UNITHOOK_MODIFY_MELEE_DAMAGE}) { }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        Player* player = target ? target->ToPlayer() : nullptr;
        Creature* creature = attacker ? attacker->ToCreature() : nullptr;
        if (!damage || !player || !creature)
            return;
        MythicRun* run = ActiveRun(creature->GetMap());
        if (run && run->Has(Affix::Necrotic) && IsMythicEnemy(creature))
            player->AddAura(SpellNecroticRot, player);
    }
};

void AddSC_mythic_affix_necrotic()
{
    new MythicNecroticUnitScript();
}
