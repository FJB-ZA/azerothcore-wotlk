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
#include "SpellAuras.h"

// Raging: non-boss enemies enrage below 30% health, doubling their damage and shrugging off crowd control, so the
// group kills them fast or saves a cooldown.
using namespace MythicPlus;

class MythicRagingUnitScript : public UnitScript
{
public:
    MythicRagingUnitScript() : UnitScript("MythicRagingUnitScript", true, {UNITHOOK_ON_DAMAGE, UNITHOOK_ON_AURA_REMOVE})
    {
    }

    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        Creature* creature = victim ? victim->ToCreature() : nullptr;
        if (!creature || damage >= creature->GetHealth() ||
            !creature->HealthBelowPctDamaged(RagingHealthPercent, damage))
            return;
        MythicRun* run = ActiveRun(creature->GetMap());
        if (!run || !run->Has(Affix::Raging) || IsBoss(creature) || !IsMythicEnemy(creature) ||
            creature->HasAura(SpellRaging))
            return;
        creature->AddAura(SpellRaging, creature);
        creature->RemoveAurasWithMechanic(IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK);
        SetCrowdControlImmune(creature, true);
    }

    // Evading clears auras, so the immunity ends with the enrage instead of outliving a reset
    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode /*mode*/) override
    {
        if (unit->IsCreature() && aurApp->GetBase()->GetId() == SpellRaging)
            SetCrowdControlImmune(unit, false);
    }

private:
    // Keyed to the Raging spell, so removing it never touches the creature's own immunities
    static void SetCrowdControlImmune(Unit* unit, bool apply)
    {
        for (uint32 mechanic = 0; mechanic < MAX_MECHANIC; ++mechanic)
            if (IMMUNE_TO_MOVEMENT_IMPAIRMENT_AND_LOSS_CONTROL_MASK & (1ULL << mechanic))
                unit->ApplySpellImmune(SpellRaging, IMMUNITY_MECHANIC, mechanic, apply);
    }
};

void AddSC_mythic_affix_raging()
{
    new MythicRagingUnitScript();
}
