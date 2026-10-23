/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <algorithm>

#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "Unit.h"

// The Gladiator path's WoD spells that need more than spell data: Shield Barrier's absorb and Bloodbath's bleed.
enum GladiatorSpells
{
    SPELL_GLADIATOR_BLOODBATH_BLEED = 990190,
};

// 990182 - Shield Barrier
class spell_gladiator_shield_barrier : public AuraScript
{
    PrepareAuraScript(spell_gladiator_shield_barrier);

    // The effect's value is the percentage of attack power absorbed.
    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Unit* caster = GetCaster())
            amount = int32(CalculatePct(caster->GetTotalAttackPowerValue(BASE_ATTACK), amount));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_gladiator_shield_barrier::CalculateAmount, EFFECT_0,
                                                     SPELL_AURA_SCHOOL_ABSORB);
    }
};

// 990189 - Bloodbath
class spell_gladiator_bloodbath : public AuraScript
{
    PrepareAuraScript(spell_gladiator_bloodbath);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GLADIATOR_BLOODBATH_BLEED });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        return damageInfo && damageInfo->GetDamage() && eventInfo.GetProcTarget();
    }

    // The effect's value is the share of the hit dealt again as a bleed; unticked bleed damage rolls into the new one.
    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetProcTarget();
        SpellInfo const* bleed = sSpellMgr->AssertSpellInfo(SPELL_GLADIATOR_BLOODBATH_BLEED);
        int32 const period = std::max<int32>(bleed->Effects[EFFECT_0].Amplitude, 1);
        int32 const ticks = std::max<int32>(bleed->GetMaxDuration() / period, 1);
        int32 total = int32(CalculatePct(eventInfo.GetDamageInfo()->GetDamage(), aurEff->GetAmount()));
        if (AuraEffect const* old = target->GetAuraEffect(SPELL_GLADIATOR_BLOODBATH_BLEED, EFFECT_0, caster->GetGUID()))
            total += old->GetAmount() * std::max<int32>(old->GetTotalTicks() - int32(old->GetTickNumber()), 0);
        int32 const perTick = total / ticks;
        if (perTick > 0)
            caster->CastCustomSpell(SPELL_GLADIATOR_BLOODBATH_BLEED, SPELLVALUE_BASE_POINT0, perTick, target, true,
                                    nullptr, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_gladiator_bloodbath::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_gladiator_bloodbath::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

void AddSC_gladiator_spells()
{
    RegisterSpellScript(spell_gladiator_shield_barrier);
    RegisterSpellScript(spell_gladiator_bloodbath);
}
