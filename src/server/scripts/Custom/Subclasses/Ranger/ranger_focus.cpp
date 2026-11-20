/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "AllSpellScript.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "UnitScript.h"

// Ranger's Focus (990500, granted while a hunter's slot follows the Ranger path): Auto Shot deals the aura's amount
// more damage, in percent. It stands in for the pet's damage, which the balance harness found to keep close to a
// fixed share of a hunter's Auto Shot at every level and in both Marksmanship and Survival; a passive aura has no
// way to touch Auto Shot alone, so the amount is a dummy and this hook applies it.
//
// It also lets the Ranger shoot point-blank. A hunter's shots can't be fired inside melee reach; a hunter with a pet
// holds the mobs off, but a Ranger has none, and in the harness's 2-mob pack, with both mobs on it, the base path lost
// 39% of its damage. The core skips the melee-reach part of a ranged spell's minimum range when this answers
// yes; the client patch drops the Hunter Range row's ranged flag so the client lets the shot through.
namespace
{
constexpr uint32 SpellRangersFocus = 990500;
}

class ranger_focus_unit : public UnitScript
{
public:
    ranger_focus_unit() : UnitScript("ranger_focus_unit", true, {UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN}) { }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!attacker || !spellInfo || damage <= 0 || !spellInfo->IsAutoRepeatRangedSpell())
            return;
        if (AuraEffect const* focus = attacker->GetAuraEffect(SpellRangersFocus, EFFECT_0))
            damage = int32(damage * (100 + focus->GetAmount()) / 100);
    }
};

class ranger_focus_point_blank : public AllSpellScript
{
public:
    ranger_focus_point_blank() : AllSpellScript("ranger_focus_point_blank", {ALLSPELLHOOK_CAN_IGNORE_RANGED_MIN_RANGE}) { }

    bool CanIgnoreRangedMinRange(Spell const* /*spell*/, Unit const* caster) override
    {
        return caster->IsPlayer() && caster->HasAura(SpellRangersFocus);
    }
};

void AddSC_ranger_focus()
{
    new ranger_focus_unit();
    new ranger_focus_point_blank();
}
