/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <algorithm>
#include <list>

#include "CellImpl.h"
#include "DataMap.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "UnitScript.h"

// Windrunner, the Ranger path's own talent tree (solo-wow patches/client/spell-data/subclasses/ranger-tree.json,
// data/sql/custom/db_world/subclasses/2026_11_19_01_ranger_tree.sql). Talents that are plain spell modifiers live in
// the spell data; the rest are dummy auras whose values this file reads. Advantage is a buff of up to 5 stacks on the
// Ranger: the builders' proc adds stacks, and the finishers (Precision Shot, Skirmish, Hushing Arrow, Incendiary Shot)
// spend them all.
namespace
{
// Abilities and the effects talents cast
constexpr uint32 SpellPrecisionShot = 993000;
constexpr uint32 SpellSerratedShot = 993001;
constexpr uint32 SpellSkirmish = 993002;
constexpr uint32 SpellFinalArrow = 993003;
constexpr uint32 SpellHushingArrow = 993005;
constexpr uint32 SpellIncendiaryShot = 993006;
constexpr uint32 SpellAdvantage = 993010;
constexpr uint32 SpellRendingShot = 993012;
constexpr uint32 SpellMarkedQuarry = 993013;
constexpr uint32 SpellQuarryArrow = 993014;
constexpr uint32 SpellBarbedTipsBleed = 993015;
constexpr uint32 SpellCruelAim = 993016;
constexpr uint32 SpellPatientAim = 993017;
constexpr uint32 SpellKindledQuiver = 993018;
constexpr uint32 SpellKindling = 993019;
constexpr uint32 SpellKindlingSplash = 993020;
constexpr uint32 SpellTellingBlow = 993021;
constexpr uint32 SpellOpportunistAim = 993024;

// Talents (first rank)
constexpr uint32 TalentQuickVenom = 993107;
constexpr uint32 TalentOpportunist = 993115;
constexpr uint32 TalentLingeringBarbs = 993119;
constexpr uint32 TalentPressTheAdvantage = 993120;
constexpr uint32 TalentCalmBreath = 993121;
constexpr uint32 TalentRendingShot = 993123;
constexpr uint32 TalentCorrodedHide = 993124;
constexpr uint32 TalentRebound = 993126;
constexpr uint32 TalentBowmaster = 993127;
constexpr uint32 TalentOpenWounds = 993128;
constexpr uint32 TalentHuntersRhythm = 993130;
constexpr uint32 TalentMarkedQuarry = 993131;
constexpr uint32 TalentQuickenedBarbs = 993137;
constexpr uint32 TalentLongShot = 993142;
constexpr uint32 TalentPatientAim = 993144;
constexpr uint32 TalentKindledQuiver = 993145;
constexpr uint32 TalentSeasonedEye = 993147;

constexpr int32 SkirmishMsPerStack = 4000;
constexpr int32 HushMs = 1000;                 // the silence, and as much again a stack
constexpr int32 LingeringBarbsMs = 2000;
constexpr int32 LingeringBarbsMaxMs = 6000;
constexpr int32 KindlingStacks = 3;
constexpr float KindlingRange = 8.0f;
constexpr float LongShotRange = 30.0f;
constexpr float QuarryRange = 30.0f;
constexpr float OpportunistHealthPct = 80.0f;
constexpr uint32 CalmBreathManaPct = 4;
constexpr uint64 MultiShotWindowMs = 500;      // Multi-Shot builds once a cast, not once a target

struct WindrunnerState : public DataMap::Base
{
    uint64 lastMultiShot = 0;
};

WindrunnerState* State(Player* player)
{
    return player->CustomData.GetDefault<WindrunnerState>("RangerWindrunner");
}

uint64 Now()
{
    return GameTime::GetGameTimeMS().count();
}

// Hunter spells the tree reaches, by family flags
bool IsHunter(SpellInfo const* spellInfo) { return spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER; }
bool IsArcaneShot(SpellInfo const* spellInfo) { return IsHunter(spellInfo) && (spellInfo->SpellFamilyFlags[0] & 0x800); }
bool IsMultiShot(SpellInfo const* spellInfo) { return IsHunter(spellInfo) && (spellInfo->SpellFamilyFlags[0] & 0x1000); }
bool IsSerpentSting(SpellInfo const* spellInfo) { return IsHunter(spellInfo) && (spellInfo->SpellFamilyFlags[0] & 0x4000); }
bool IsFireTrap(SpellInfo const* spellInfo) { return IsHunter(spellInfo) && (spellInfo->SpellFamilyFlags[0] & 0x4); }
bool IsSteadyShot(SpellInfo const* spellInfo) { return IsHunter(spellInfo) && (spellInfo->SpellFamilyFlags[1] & 0x1); }
bool IsRangerBleed(uint32 id) { return id == SpellSerratedShot || id == SpellBarbedTipsBleed; }

// The value of a ranked talent the player has, or 0
int32 Talent(Unit* unit, uint32 firstRank, uint8 effIndex = EFFECT_0)
{
    AuraEffect const* effect = unit->GetAuraEffectOfRankedSpell(firstRank, effIndex);
    return effect ? effect->GetAmount() : 0;
}

bool HasTalent(Unit* unit, uint32 firstRank)
{
    return unit->GetAuraEffectOfRankedSpell(firstRank, EFFECT_0) != nullptr;
}

Player* Ranger(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_HUNTER ? player : nullptr;
}

// Runs `action` once the current damage calculation is done, so auras aren't changed while they're being read
template <typename Action>
void Later(Player* player, Unit* target, Action action)
{
    ObjectGuid const playerGuid = player->GetGUID();
    ObjectGuid const targetGuid = target->GetGUID();
    player->m_Events.AddEventAtOffset([playerGuid, targetGuid, action]()
    {
        Player* p = ObjectAccessor::FindPlayer(playerGuid);
        if (!p)
            return;
        Unit* t = ObjectAccessor::GetUnit(*p, targetGuid);
        if (t && t->IsInWorld())
            action(p, t);
    }, 0ms);
}

std::list<Unit*> EnemiesNear(Player* player, Unit* center, float range)
{
    std::list<Unit*> found;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, found, check);
    Cell::VisitObjects(center, searcher, range);
    return found;
}

// A stack more of a self buff, renewing it (the buff's own limit caps it)
void AddStacks(Player* player, uint32 spellId, uint8 stacks = 1)
{
    if (!stacks)
        return;
    if (Aura* aura = player->GetAura(spellId))
        aura->ModStackAmount(stacks);
    else if (Aura* added = player->AddAura(spellId, player); added && stacks > 1)
        added->ModStackAmount(stacks - 1);
}

// Advantage only builds once the Ranger has the tree's first finisher
void AddAdvantage(Player* player, uint8 stacks)
{
    if (player->HasSpell(SpellPrecisionShot))
        AddStacks(player, SpellAdvantage, stacks);
}

// Seasoned Eye: Advantage's per-stack bonuses are this much larger, in percent
float SeasonedScale(Player* player)
{
    return (100.0f + Talent(player, TalentSeasonedEye)) / 100.0f;
}

// Spends all of the Ranger's Advantage and returns the stacks. Calm Breath may give mana back for each, and spending a
// full 5 lengthens Skirmish (Press the Advantage).
uint8 SpendAdvantage(Player* player)
{
    Aura* aura = player->GetAura(SpellAdvantage);
    if (!aura)
        return 0;
    uint8 const stacks = aura->GetStackAmount();
    aura->Remove();

    if (int32 const chance = Talent(player, TalentCalmBreath))
        for (uint8 i = 0; i < stacks; ++i)
            if (roll_chance_i(chance))
                player->EnergizeBySpell(player, TalentCalmBreath, CalculatePct(player->GetCreateMana(), CalmBreathManaPct), POWER_MANA);

    if (stacks >= sSpellMgr->AssertSpellInfo(SpellAdvantage)->StackAmount)
        if (int32 const seconds = Talent(player, TalentPressTheAdvantage))
            if (Aura* skirmish = player->GetAura(SpellSkirmish))
            {
                int32 const add = seconds * IN_MILLISECONDS;
                skirmish->SetMaxDuration(skirmish->GetMaxDuration() + add);
                skirmish->SetDuration(skirmish->GetDuration() + add);
            }
    return stacks;
}

bool HasOwnSerpentSting(Unit* target, ObjectGuid caster)
{
    for (auto const& [id, application] : target->GetAppliedAuras())
        if (application->GetBase()->GetCasterGUID() == caster && IsSerpentSting(application->GetBase()->GetSpellInfo()))
            return true;
    return false;
}

bool BleedingOrPoisoned(Unit* target)
{
    if (target->HasAuraWithMechanic(1ULL << MECHANIC_BLEED))
        return true;
    for (auto const& [id, application] : target->GetAppliedAuras())
        if (application->GetBase()->GetSpellInfo()->Dispel == DISPEL_POISON)
            return true;
    return false;
}

// The share of physical damage armor takes, as the core works it out (Unit::CalcArmorReducedDamage)
float ArmorReduction(float armor, uint8 level)
{
    float levelModifier = level;
    if (levelModifier > 59.0f)
        levelModifier += 4.5f * (levelModifier - 59.0f);
    float reduction = 0.1f * armor / (8.5f * levelModifier + 40.0f);
    reduction = reduction / (1.0f + reduction);
    return std::clamp(reduction, 0.0f, 0.75f);
}

// Corroded Hide, as the damage the ignored armor would have taken: the hook runs before armor, so the bonus is the
// ratio of what gets through with and without the talent
float CorrodedHidePct(Player* player, Unit* target, SpellInfo const* spellInfo)
{
    int32 const ignored = Talent(player, TalentCorrodedHide);
    if (!ignored || spellInfo->DmgClass != SPELL_DAMAGE_CLASS_RANGED ||
        !Unit::IsDamageReducedByArmor(spellInfo->GetSchoolMask(), spellInfo) || !HasOwnSerpentSting(target, player->GetGUID()))
        return 0.0f;
    float const armor = float(target->GetArmor());
    if (armor <= 0.0f)
        return 0.0f;
    float const before = ArmorReduction(armor, player->GetLevel());
    float const after = ArmorReduction(armor * (100 - ignored) / 100.0f, player->GetLevel());
    return ((1.0f - after) / (1.0f - before) - 1.0f) * 100.0f;
}

void ApplyPct(uint32& damage, float pct)
{
    if (pct > 0.0f)
        damage = uint32(damage * (100.0f + pct) / 100.0f);
}
}

class ranger_windrunner_damage : public UnitScript
{
public:
    ranger_windrunner_damage() : UnitScript("ranger_windrunner_damage", true,
        { UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN }) { }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = Ranger(attacker);
        if (!player || !spellInfo || !target || !damage)
            return;

        float pct = 0.0f;
        if (int32 const openWounds = Talent(player, TalentOpenWounds); openWounds && BleedingOrPoisoned(target))
            pct += openWounds;
        if (IsRangerBleed(spellInfo->Id))
            if (AuraEffect const* cruel = target->GetAuraEffect(SpellCruelAim, EFFECT_0, player->GetGUID()))
                pct += cruel->GetAmount();
        ApplyPct(damage, pct);

        bool const serpent = IsSerpentSting(spellInfo);
        if ((serpent || spellInfo->Id == SpellSerratedShot) && HasTalent(player, TalentPatientAim))
            Later(player, target, [](Player* p, Unit* /*t*/) { AddStacks(p, SpellPatientAim); });
        if ((serpent || IsFireTrap(spellInfo)) && HasTalent(player, TalentKindledQuiver))
            Later(player, target, [](Player* p, Unit* /*t*/) { AddStacks(p, SpellKindledQuiver); });
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = Ranger(attacker);
        if (!player || !spellInfo || !target || damage <= 0)
            return;

        float pct = CorrodedHidePct(player, target, spellInfo);
        if (int32 const openWounds = Talent(player, TalentOpenWounds); openWounds && BleedingOrPoisoned(target))
            pct += openWounds;
        uint32 value = uint32(damage);
        ApplyPct(value, pct);
        damage = int32(value);

        if (spellInfo->IsAutoRepeatRangedSpell())
        {
            if (int32 const chance = Talent(player, TalentLingeringBarbs); chance && roll_chance_i(chance))
                Later(player, target, [](Player* p, Unit* t)
                {
                    Aura* serrated = t->GetAura(SpellSerratedShot, p->GetGUID());
                    if (!serrated || serrated->GetMaxDuration() + LingeringBarbsMs > serrated->GetSpellInfo()->GetMaxDuration() + LingeringBarbsMaxMs)
                        return;
                    serrated->SetMaxDuration(serrated->GetMaxDuration() + LingeringBarbsMs);
                    serrated->SetDuration(serrated->GetDuration() + LingeringBarbsMs);
                });
            if (int32 const manaPct = Talent(player, TalentHuntersRhythm))
                Later(player, target, [manaPct](Player* p, Unit* /*t*/)
                {
                    p->EnergizeBySpell(p, TalentHuntersRhythm, CalculatePct(p->GetMaxPower(POWER_MANA), manaPct), POWER_MANA);
                });
        }

        if (IsFireTrap(spellInfo) && HasTalent(player, TalentKindledQuiver))
            Later(player, target, [](Player* p, Unit* /*t*/) { AddStacks(p, SpellKindledQuiver); });
    }
};

// 993000 Precision Shot, 993002 Skirmish, 993005 Hushing Arrow, 993006 Incendiary Shot - the Advantage finishers. Each
// needs Advantage (the spell data's CasterAuraSpell) and spends all of it as it is cast.
class spell_ranger_finisher : public SpellScript
{
    PrepareSpellScript(spell_ranger_finisher);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellAdvantage, SpellRendingShot, SpellPatientAim, SpellKindledQuiver, SpellKindling,
            SpellKindlingSplash, SpellOpportunistAim });
    }

    void HandleBeforeCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;
        uint32 const id = GetSpellInfo()->Id;

        // Opportunist: on the Ranger before the shot's crit is rolled, gone after the cast
        if (id == SpellPrecisionShot)
            if (int32 bonus = Talent(player, TalentOpportunist))
                if (Unit* target = GetExplTargetUnit(); target && target->GetHealthPct() > OpportunistHealthPct)
                {
                    player->CastCustomSpell(player, SpellOpportunistAim, &bonus, nullptr, nullptr, true);
                    _aimed = true;
                }

        _stacks = SpendAdvantage(player);
        _perStack = sSpellMgr->AssertSpellInfo(SpellAdvantage)->Effects[EFFECT_0].CalcValue() * SeasonedScale(player);

        if (id == SpellPrecisionShot)
            if (Aura* patient = player->GetAura(SpellPatientAim))
            {
                _patientPct = float(sSpellMgr->AssertSpellInfo(SpellPatientAim)->Effects[EFFECT_0].CalcValue() * patient->GetStackAmount());
                patient->Remove();
            }

        // Kindling: a finisher that hits one target splashes it; Skirmish hits no one and Incendiary Shot bursts anyway
        if (id == SpellPrecisionShot || id == SpellHushingArrow)
            if (Aura* kindling = player->GetAura(SpellKindling))
            {
                _splashPct = kindling->GetSpellInfo()->Effects[EFFECT_0].CalcValue();
                kindling->ModStackAmount(-1);
            }
    }

    void HandleHit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        uint32 const id = GetSpellInfo()->Id;
        int32 damage = GetHitDamage();
        if (damage <= 0)
            return;

        float pct = 0.0f;
        if (id == SpellPrecisionShot || id == SpellIncendiaryShot)
            pct += _stacks * _perStack;
        if (id == SpellPrecisionShot)
        {
            pct += _patientPct;
            if (int32 const longShot = Talent(player, TalentLongShot); longShot && player->GetDistance(target) >= LongShotRange)
                pct += longShot;
        }
        if (pct > 0.0f)
        {
            damage = int32(damage * (100.0f + pct) / 100.0f);
            SetHitDamage(damage);
        }

        if (_splashPct)
        {
            int32 const splash = CalculatePct(damage, _splashPct);
            Later(player, target, [splash](Player* p, Unit* t)
            {
                int32 bp = splash;
                for (Unit* other : EnemiesNear(p, t, KindlingRange))
                    if (other != t && other->IsAlive() && p->IsValidAttackTarget(other))
                        p->CastCustomSpell(other, SpellKindlingSplash, &bp, nullptr, nullptr, true);
            });
        }
    }

    void HandleAfterHit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        uint32 const id = GetSpellInfo()->Id;

        if (id == SpellPrecisionShot && HasTalent(player, TalentRendingShot) && target->IsAlive())
            player->AddAura(SpellRendingShot, target);

        // Hushing Arrow: 1 sec of silence, and 1 sec more a stack spent
        if (id == SpellHushingArrow)
            if (Aura* silence = target->GetAura(SpellHushingArrow, player->GetGUID()))
            {
                int32 const duration = HushMs + int32(_stacks * HushMs * SeasonedScale(player));
                silence->SetMaxDuration(duration);
                silence->SetDuration(duration);
            }
    }

    void HandleAfterCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;
        uint32 const id = GetSpellInfo()->Id;

        if (_aimed)
            player->RemoveAurasDueToSpell(SpellOpportunistAim);

        // Skirmish lasts 4 sec a stack spent
        if (id == SpellSkirmish)
            if (Aura* skirmish = player->GetAura(SpellSkirmish))
            {
                int32 const duration = std::max<int32>(SkirmishMsPerStack, int32(_stacks * SkirmishMsPerStack * SeasonedScale(player)));
                skirmish->SetMaxDuration(duration);
                skirmish->SetDuration(duration);
            }

        if (id == SpellIncendiaryShot)
        {
            player->RemoveAurasDueToSpell(SpellKindledQuiver);
            player->RemoveAurasDueToSpell(SpellKindling);
            AddStacks(player, SpellKindling, KindlingStacks);
        }
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(spell_ranger_finisher::HandleBeforeCast);
        OnHit += SpellHitFn(spell_ranger_finisher::HandleHit);
        AfterHit += SpellHitFn(spell_ranger_finisher::HandleAfterHit);
        AfterCast += SpellCastFn(spell_ranger_finisher::HandleAfterCast);
    }

    uint8 _stacks = 0;
    float _perStack = 0.0f;
    float _patientPct = 0.0f;
    int32 _splashPct = 0;
    bool _aimed = false;
};

// 993003 - Final Arrow spends Telling Blow, the buff that let it ignore the target's health.
class spell_ranger_final_arrow : public SpellScript
{
    PrepareSpellScript(spell_ranger_final_arrow);

    void HandleAfterCast()
    {
        GetCaster()->RemoveAurasDueToSpell(SpellTellingBlow);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_ranger_final_arrow::HandleAfterCast);
    }
};

// 993004 - Ricochet Shot. Rebound: more damage with each enemy after the first. Marked Quarry: it marks each one.
class spell_ranger_ricochet_shot : public SpellScript
{
    PrepareSpellScript(spell_ranger_ricochet_shot);

    void HandleHit()
    {
        Unit* caster = GetCaster();
        if (int32 const rebound = Talent(caster, TalentRebound); rebound && _hits && GetHitDamage() > 0)
            SetHitDamage(int32(GetHitDamage() * (100.0f + rebound * _hits) / 100.0f));
        ++_hits;
    }

    void HandleAfterHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (target && target->IsAlive() && HasTalent(caster, TalentMarkedQuarry))
            caster->AddAura(SpellMarkedQuarry, target);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_ranger_ricochet_shot::HandleHit);
        AfterHit += SpellHitFn(spell_ranger_ricochet_shot::HandleAfterHit);
    }

    uint8 _hits = 0;
};

// 993013 - Marked Quarry: every tick, the Ranger looses an arrow at the marked enemy if it is close enough.
class spell_ranger_marked_quarry : public AuraScript
{
    PrepareAuraScript(spell_ranger_marked_quarry);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellQuarryArrow });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !caster->IsAlive() || !target->IsAlive() || !caster->IsWithinDistInMap(target, QuarryRange) ||
            !caster->IsValidAttackTarget(target))
            return;
        caster->CastSpell(target, SpellQuarryArrow, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_ranger_marked_quarry::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 993011 - Advantage, the builders' proc: 1 stack a hit (Multi-Shot once a cast), 2 for Final Arrow, and Bowmaster's
// extra one on an Arcane Shot or Steady Shot crit. Nothing builds until the Ranger has Precision Shot.
class spell_ranger_advantage : public AuraScript
{
    PrepareAuraScript(spell_ranger_advantage);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellAdvantage, SpellPrecisionShot });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Player* player = GetTarget()->ToPlayer();
        return player && eventInfo.GetSpellInfo() && player->HasSpell(SpellPrecisionShot);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Player* player = GetTarget()->ToPlayer();
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (!player || !spellInfo)
            return;

        uint8 stacks = spellInfo->Id == SpellFinalArrow ? 2 : 1;
        if (IsMultiShot(spellInfo))
        {
            WindrunnerState* state = State(player);
            uint64 const now = Now();
            if (now - state->lastMultiShot < MultiShotWindowMs)
                return;
            state->lastMultiShot = now;
        }
        if ((IsArcaneShot(spellInfo) || IsSteadyShot(spellInfo)) && (eventInfo.GetHitMask() & PROC_HIT_CRITICAL) &&
            HasTalent(player, TalentBowmaster))
            ++stacks;
        AddAdvantage(player, stacks);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_ranger_advantage::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_ranger_advantage::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// -993139 - Cruel Aim: ranged ability crits leave the target taking more from the Ranger's bleeds.
class spell_ranger_cruel_aim : public AuraScript
{
    PrepareAuraScript(spell_ranger_cruel_aim);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellCruelAim });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        if (!target || target == caster || !target->IsAlive())
            return;
        int32 amount = aurEff->GetAmount();
        caster->CastCustomSpell(target, SpellCruelAim, &amount, nullptr, nullptr, true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_ranger_cruel_aim::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 993141 - Barbed Tips: Aimed Shot and Precision Shot crits bleed the target for a share of the damage over 4 sec.
class spell_ranger_barbed_tips : public AuraScript
{
    PrepareAuraScript(spell_ranger_barbed_tips);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellBarbedTipsBleed });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!target || target == caster || !target->IsAlive() || !damageInfo || !damageInfo->GetDamage())
            return;
        SpellInfo const* bleed = sSpellMgr->AssertSpellInfo(SpellBarbedTipsBleed);
        int32 const ticks = std::max(1, bleed->GetMaxDuration() / std::max<int32>(1, bleed->Effects[EFFECT_0].Amplitude));
        int32 perTick = int32(CalculatePct(damageInfo->GetDamage(), aurEff->GetAmount()) / ticks);
        if (perTick > 0)
            caster->CastCustomSpell(target, SpellBarbedTipsBleed, &perTick, nullptr, nullptr, true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_ranger_barbed_tips::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Serrated Shot, Serpent Sting and Black Arrow: Quickened Barbs makes them tick faster, and Quick Venom Serpent Sting
// faster still.
class spell_ranger_dot : public AuraScript
{
    PrepareAuraScript(spell_ranger_dot);

    void CalcPeriodic(AuraEffect const* /*aurEff*/, bool& isPeriodic, int32& amplitude)
    {
        Unit* caster = GetCaster();
        if (!isPeriodic || amplitude <= 0 || !caster)
            return;
        int32 faster = Talent(caster, TalentQuickenedBarbs);
        if (IsSerpentSting(GetSpellInfo()))
            faster += Talent(caster, TalentQuickVenom);
        if (faster > 0)
            amplitude = std::max(500, amplitude * 100 / (100 + faster));
    }

    void Register() override
    {
        DoEffectCalcPeriodic += AuraEffectCalcPeriodicFn(spell_ranger_dot::CalcPeriodic, EFFECT_ALL, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

void AddSC_ranger_windrunner()
{
    new ranger_windrunner_damage();
    RegisterSpellScript(spell_ranger_finisher);
    RegisterSpellScript(spell_ranger_final_arrow);
    RegisterSpellScript(spell_ranger_ricochet_shot);
    RegisterSpellScript(spell_ranger_marked_quarry);
    RegisterSpellScript(spell_ranger_advantage);
    RegisterSpellScript(spell_ranger_cruel_aim);
    RegisterSpellScript(spell_ranger_barbed_tips);
    RegisterSpellScript(spell_ranger_dot);
}
