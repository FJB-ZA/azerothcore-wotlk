/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Creature.h"
#include "CreatureData.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "../../SubclassPaths.h"

// The Witch Doctor's spells and wards (data/sql/custom/db_world/subclasses/2026_11_16_01_witch_doctor_path.sql).
namespace
{
constexpr uint32 SpellSpiritNovaR1 = 990710;        // mirrors Fire Nova (1535)
constexpr uint32 SpellSpiritNovaDamageR1 = 990720;  // mirrors Fire Nova's damage (8349)
}

// -990710 - Spirit Nova: Fire Nova's script (spell_sha_fire_nova) for the mirror, whose Validate only takes Fire
// Nova's own ranks. The fire-slot totem or ward casts the same rank of Spirit Nova's damage.
class spell_witch_doctor_spirit_nova : public SpellScript
{
    PrepareSpellScript(spell_witch_doctor_spirit_nova);

    bool Validate(SpellInfo const* spellInfo) override
    {
        SpellInfo const* firstRank = sSpellMgr->GetSpellInfo(SpellSpiritNovaR1);
        return firstRank && spellInfo->IsRankOf(firstRank) &&
               sSpellMgr->GetSpellWithRank(SpellSpiritNovaDamageR1, spellInfo->GetRank(), true);
    }

    SpellCastResult CheckFireTotem()
    {
        Unit* caster = GetCaster();
        Creature* totem = caster->GetMap()->GetCreature(caster->m_SummonSlot[SUMMON_SLOT_TOTEM_FIRE]);
        if (!totem)
        {
            SetCustomCastResultMessage(SPELL_CUSTOM_ERROR_MUST_HAVE_FIRE_TOTEM);
            return SPELL_FAILED_CUSTOM_ERROR;
        }
        if (!caster->IsWithinDistInMap(totem, caster->GetSpellMaxRangeForTarget(totem, GetSpellInfo())))
            return SPELL_FAILED_OUT_OF_RANGE;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (Creature* totem = caster->GetMap()->GetCreature(caster->m_SummonSlot[SUMMON_SLOT_TOTEM_FIRE]))
            if (totem->IsTotem())
                caster->CastSpell(totem, sSpellMgr->GetSpellWithRank(SpellSpiritNovaDamageR1, GetSpellInfo()->GetRank()), true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_witch_doctor_spirit_nova::CheckFireTotem);
        OnEffectHitTarget += SpellEffectFn(spell_witch_doctor_spirit_nova::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// A player's totem takes its race's totem model as it is summoned (Totem::InitStats). A ward is a totem in another
// body, so it puts its own model back as it enters the world: any creature a path swaps in for a totem.
class witch_doctor_ward_model : public AllCreatureScript
{
public:
    witch_doctor_ward_model() : AllCreatureScript("witch_doctor_ward_model") { }

    void OnCreatureAddWorld(Creature* creature) override
    {
        if (!creature->IsTotem() || !SubclassPaths::SwapForReplacementCreature(creature->GetEntry()))
            return;
        if (CreatureModel const* model = creature->GetCreatureTemplate()->GetFirstValidModel())
        {
            creature->SetDisplayId(model->CreatureDisplayID, model->DisplayScale);
            creature->SetNativeDisplayId(model->CreatureDisplayID);
        }
    }
};

void AddSC_witch_doctor_spells()
{
    RegisterSpellScript(spell_witch_doctor_spirit_nova);
    new witch_doctor_ward_model();
}
