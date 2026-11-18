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
#include <vector>

#include "CellImpl.h"
#include "DataMap.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "TemporarySummon.h"

// Blight, the Necromancer path's own talent tree (solo-wow patches/client/spell-data/subclasses/necromancer-tree.json,
// data/sql/custom/db_world/subclasses/2026_11_17_03_necromancer_tree.sql). Talents that are plain spell modifiers live
// in the spell data; the rest are dummy auras whose values this file reads.
namespace
{
// Abilities and the effects talents cast
constexpr uint32 SpellGraveWorms = 992000;
constexpr uint32 SpellPlagueOfTheRisen = 992001;
constexpr uint32 SpellRisenBurst = 992002;
constexpr uint32 SpellContagion = 992003;
constexpr uint32 SpellContagionCarry = 992005;
constexpr uint32 SpellWastingPlague = 992006;
constexpr uint32 SpellMutation = 992007;
constexpr uint32 SpellPlagueBile = 992009;
constexpr uint32 SpellCryptPlague = 992011;
constexpr uint32 SpellFester = 992012;
constexpr uint32 SpellPutridCommand = 992013;
constexpr uint32 SpellParasites = 992014;
constexpr uint32 SpellPestilentHaste = 992015;
constexpr uint32 SpellBrewSplash = 992016;
constexpr uint32 SpellRottingHunger = 992017;
constexpr uint32 SpellRotlingBurst = 992019;
constexpr uint32 SpellRotEcho = 992020;
constexpr uint32 SpellRotEchoDamage = 992021;

// Talents (first rank)
constexpr uint32 TalentCryptPlague = 992010;
constexpr uint32 TalentGraveCommand = 992107;
constexpr uint32 TalentDoomedFlesh = 992108;
constexpr uint32 TalentGravecrawl = 992111;
constexpr uint32 TalentFester = 992117;
constexpr uint32 TalentDeathreachRank2 = 992121;
constexpr uint32 TalentSpreadingSickness = 992122;
constexpr uint32 TalentPutridCommand = 992123;
constexpr uint32 TalentCryptBurst = 992127;
constexpr uint32 TalentPlagueTide = 992128;
constexpr uint32 TalentHastenedEnd = 992129;
constexpr uint32 TalentLichsMalice = 992136;
constexpr uint32 TalentRotEcho = 992139;
constexpr uint32 TalentAbominationsBrew = 992140;
constexpr uint32 TalentWithering = 992142;
constexpr uint32 TalentPlagueScience = 992143;
constexpr uint32 TalentWakingCrypts = 992145;
constexpr uint32 TalentRottingHunger = 992147;
constexpr uint32 TalentMuckSummoner = 992148;

// Plague Strike (Immolate's mirror) ranks, Chilling Touch (Searing Pain's) and Frost Lance (Incinerate's)
constexpr uint32 PlagueStrikeFirst = 990300;
constexpr uint32 PlagueStrikeLast = 990310;
constexpr uint32 ChillingTouchFirst = 990311;
constexpr uint32 ChillingTouchLast = 990320;
constexpr uint32 FrostLanceFirst = 990327;
constexpr uint32 FrostLanceLast = 990330;

// The minions (subclass_creature_swap rows for path 2) and the tree's own creatures
constexpr uint32 MinionFirst = 990350;
constexpr uint32 MinionLast = 990356;
constexpr uint32 NpcRotling = 990381;

constexpr uint8 CryptPlagueStacks = 5;
constexpr uint8 CryptPlagueStacksWithering = 10;
constexpr uint8 FesterStacks = 3;
constexpr float CryptBurstRange = 8.0f;
constexpr uint8 CryptBurstTargets = 2;
constexpr uint64 CommandCooldownMs = 6000;   // a minion "special ability" counts once every 6 sec at most
constexpr uint64 PlagueTideCooldownMs = 2000;
constexpr int32 PlagueTideMs = 1000;
constexpr uint64 MuckSummonerCooldownMs = 10000;
constexpr uint32 RotlingLifeMs = 8000;
constexpr uint32 RotlingFuseMs = 4000;
constexpr uint32 ContagionCooldownMs = 60000;

// One copy of a disease Contagion carries
struct DiseaseCopy
{
    uint32 spellId = 0;
    int32 duration = 0;
    int32 maxDuration = 0;
    uint8 stacks = 1;
    int32 amounts[MAX_SPELL_EFFECTS] = { 0, 0, 0 };
};

struct BlightState : public DataMap::Base
{
    uint64 lastCommand = 0;
    uint64 lastTide = 0;
    uint64 lastMuck = 0;
    std::vector<DiseaseCopy> carried;
    uint8 charges = 0;
    bool spent = false;   // the carried copies ran out by being given away, not by time
};

BlightState* State(Player* player)
{
    return player->CustomData.GetDefault<BlightState>("NecromancerBlight");
}

uint64 Now()
{
    return GameTime::GetGameTimeMS().count();
}

bool IsPlagueStrike(uint32 id) { return id >= PlagueStrikeFirst && id <= PlagueStrikeLast; }

// Plague Strike, Grave Worms, Plague of the Risen, Wasting Plague and Crypt Plague
bool IsDisease(uint32 id)
{
    return IsPlagueStrike(id) || id == SpellGraveWorms || id == SpellPlagueOfTheRisen || id == SpellWastingPlague ||
           id == SpellCryptPlague;
}

bool IsFrostNuke(uint32 id)
{
    return (id >= ChillingTouchFirst && id <= ChillingTouchLast) || (id >= FrostLanceFirst && id <= FrostLanceLast);
}

bool IsDrainLife(SpellInfo const* spellInfo)
{
    return spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK && (spellInfo->SpellFamilyFlags[0] & 0x8);
}

// The value of a ranked talent the player has, or 0
int32 Talent(Unit* player, uint32 firstRank, uint8 effIndex = EFFECT_0)
{
    AuraEffect const* effect = player->GetAuraEffectOfRankedSpell(firstRank, effIndex);
    return effect ? effect->GetAmount() : 0;
}

bool HasTalent(Unit* player, uint32 firstRank)
{
    return player->GetAuraEffectOfRankedSpell(firstRank, EFFECT_0) != nullptr;
}

// The player whose Necromancer minion this is, or nullptr
Player* MinionOwner(Unit* unit)
{
    if (!unit || unit->IsPlayer() || unit->GetEntry() < MinionFirst || unit->GetEntry() > MinionLast)
        return nullptr;
    Unit* owner = unit->GetOwner();
    return owner ? owner->ToPlayer() : nullptr;
}

// The caster's diseases on the target, one aura each
std::vector<Aura*> Diseases(Unit* target, ObjectGuid caster)
{
    std::vector<Aura*> found;
    for (auto const& [id, application] : target->GetAppliedAuras())
    {
        Aura* aura = application->GetBase();
        if (aura->GetCasterGUID() == caster && IsDisease(aura->GetId()))
            found.push_back(aura);
    }
    return found;
}

uint32 CountDiseases(Unit* target, ObjectGuid caster, uint32 exceptSpell = 0)
{
    uint32 count = 0;
    for (Aura* aura : Diseases(target, caster))
        if (aura->GetId() != exceptSpell)
            ++count;
    return count;
}

uint8 FesterOn(Unit* target, ObjectGuid caster)
{
    Aura const* fester = target->GetAura(SpellFester, caster);
    return fester ? fester->GetStackAmount() : 0;
}

// Adds stacks of Crypt Plague without renewing it; Rotting Hunger feeds the minion each time
void PlantCryptPlague(Player* player, Unit* target, uint8 stacks)
{
    if (!target->IsAlive() || !player->IsValidAttackTarget(target) || !stacks)
        return;
    uint8 const cap = HasTalent(player, TalentWithering) ? CryptPlagueStacksWithering : CryptPlagueStacks;
    Aura* aura = target->GetAura(SpellCryptPlague, player->GetGUID());
    if (!aura)
    {
        aura = player->AddAura(SpellCryptPlague, target);
        if (!aura)
            return;
        --stacks;
    }
    uint8 const room = aura->GetStackAmount() < cap ? cap - aura->GetStackAmount() : 0;
    if (stacks && room)
    {
        int32 const duration = aura->GetDuration();
        aura->ModStackAmount(std::min(stacks, room));
        aura->SetDuration(duration);
    }

    if (HasTalent(player, TalentRottingHunger))
        if (Pet* pet = player->GetPet())
            if (pet->IsAlive())
                pet->CastSpell(pet, SpellRottingHunger, true);
}

// Runs `action` with the player and target once the current damage calculation is done, so auras aren't added to the
// target while its own auras are ticking
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

// A minion's special ability struck `target`: Grave Command, Crypt Burst, Fester and Putrid Command
void MinionCommand(Player* player, Unit* target)
{
    BlightState* state = State(player);
    uint64 const now = Now();
    if (now - state->lastCommand < CommandCooldownMs)
        return;
    bool const graveCommand = HasTalent(player, TalentGraveCommand);
    bool const cryptBurst = HasTalent(player, TalentCryptBurst);
    bool const fester = HasTalent(player, TalentFester);
    bool const putrid = HasTalent(player, TalentPutridCommand);
    if (!graveCommand && !cryptBurst && !fester && !putrid)
        return;
    state->lastCommand = now;

    Later(player, target, [graveCommand, cryptBurst, fester, putrid](Player* p, Unit* t)
    {
        if (graveCommand)
            PlantCryptPlague(p, t, 1);
        if (cryptBurst)
        {
            uint8 planted = 0;
            for (Unit* other : EnemiesNear(p, t, CryptBurstRange))
            {
                if (other == t || !other->IsInCombat() || planted >= CryptBurstTargets)
                    continue;
                PlantCryptPlague(p, other, 1);
                ++planted;
            }
        }
        if (fester && CountDiseases(t, p->GetGUID()))
        {
            uint8 const cap = FesterStacks + (p->HasAura(TalentDeathreachRank2) ? 1 : 0);
            if (Aura* aura = t->GetAura(SpellFester, p->GetGUID()); aura && aura->GetStackAmount() >= cap)
                aura->RefreshDuration();
            else
                p->AddAura(SpellFester, t);
        }
        if (putrid)
            p->AddAura(SpellPutridCommand, t);
    });
}

// Any minion damage: Plague Tide and Abomination's Brew
void MinionDamage(Player* player, Unit* target)
{
    uint64 const now = Now();
    if (HasTalent(player, TalentPlagueTide))
    {
        BlightState* state = State(player);
        if (now - state->lastTide >= PlagueTideCooldownMs && player->HasSpellCooldown(SpellContagion))
        {
            state->lastTide = now;
            player->ModifySpellCooldown(SpellContagion, -PlagueTideMs);
        }
    }
    if (int32 const chance = Talent(player, TalentAbominationsBrew))
        if (roll_chance_i(chance))
            Later(player, target, [](Player* p, Unit* t) { p->CastSpell(t, SpellBrewSplash, true); });
}

// Percent bonuses the target's state gives the player's damage. `periodic` is a tick of the player's own DoT.
float DamageBonusPct(Player* player, Unit* target, SpellInfo const* spellInfo, bool periodic)
{
    ObjectGuid const guid = player->GetGUID();
    uint32 const id = spellInfo->Id;
    float pct = 0.0f;

    if (target->GetAura(SpellPutridCommand, guid))
        pct += sSpellMgr->AssertSpellInfo(SpellPutridCommand)->Effects[EFFECT_0].CalcValue();

    if ((spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_SHADOW) && target->HealthBelowPct(35))
        pct += Talent(player, TalentDoomedFlesh);

    bool const disease = IsDisease(id);
    if (periodic && disease)
    {
        if (uint8 const fester = FesterOn(target, guid))
        {
            int32 const perStack = sSpellMgr->AssertSpellInfo(SpellFester)->Effects[EFFECT_0].CalcValue();
            pct += perStack * fester;
            if (id == SpellPlagueOfTheRisen)
                pct += Talent(player, TalentHastenedEnd) * fester;
        }
        if (id == SpellGraveWorms && target->HealthBelowPct(35))
            pct += Talent(player, TalentGravecrawl);
        if (id == SpellWastingPlague)
            pct += spellInfo->Effects[EFFECT_1].CalcValue() * CountDiseases(target, guid, SpellWastingPlague);
        if (id == SpellCryptPlague)
            pct += Talent(player, TalentWakingCrypts);
    }

    bool const drainLife = periodic && IsDrainLife(spellInfo);
    if (drainLife && HasTalent(player, TalentWakingCrypts))
        pct += 4.0f * CountDiseases(target, guid);
    if (((periodic && disease) || drainLife) && target->HealthBelowPct(20))
        pct += Talent(player, TalentPlagueScience);

    if (!periodic && IsFrostNuke(id) && CountDiseases(target, guid))
        pct += Talent(player, TalentLichsMalice);

    return pct;
}

void ApplyPct(uint32& damage, float pct)
{
    if (pct > 0.0f)
        damage = uint32(damage * (100.0f + pct) / 100.0f);
}

void SpawnRotlings(Player* player, Unit* target)
{
    for (uint8 i = 0; i < 2; ++i)
    {
        Position pos = player->GetNearPosition(2.0f, float(M_PI) / 2.0f * (i ? 1.0f : -1.0f));
        TempSummon* rotling = player->SummonCreature(NpcRotling, pos, TEMPSUMMON_TIMED_DESPAWN, RotlingLifeMs);
        if (!rotling)
            continue;
        rotling->SetFaction(player->GetFaction());
        rotling->SetLevel(player->GetLevel());
        if (rotling->AI())
            rotling->AI()->SetGUID(target->GetGUID());
    }
}
}

class necromancer_blight_damage : public UnitScript
{
public:
    necromancer_blight_damage() : UnitScript("necromancer_blight_damage", true,
        { UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_MELEE_DAMAGE }) { }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = attacker ? attacker->ToPlayer() : nullptr;
        if (!player || player->getClass() != CLASS_WARLOCK || !spellInfo || !target || !damage)
            return;

        ApplyPct(damage, DamageBonusPct(player, target, spellInfo, true));

        // Rot Echo: part of each tick repeats as direct damage
        if (AuraEffect const* echo = player->GetAuraEffect(SpellRotEcho, EFFECT_0))
            if (int32 const echoed = int32(damage * echo->GetAmount() / 100))
                Later(player, target, [echoed](Player* p, Unit* t) { p->CastCustomSpell(t, SpellRotEchoDamage, &echoed, nullptr, nullptr, true); });

        if (IsDrainLife(spellInfo) && HasTalent(player, TalentCryptPlague))
            Later(player, target, [](Player* p, Unit* t) { PlantCryptPlague(p, t, 1); });

        if (int32 const chance = Talent(player, TalentMuckSummoner))
        {
            BlightState* state = State(player);
            uint64 const now = Now();
            if (now - state->lastMuck >= MuckSummonerCooldownMs && roll_chance_i(chance))
            {
                state->lastMuck = now;
                Later(player, target, [](Player* p, Unit* t) { SpawnRotlings(p, t); });
            }
        }
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!attacker || !target || !spellInfo || damage <= 0)
            return;

        if (Player* owner = MinionOwner(attacker))
        {
            if (target->GetAura(SpellParasites, owner->GetGUID()))
                damage += damage * sSpellMgr->AssertSpellInfo(SpellParasites)->Effects[EFFECT_0].CalcValue() / 100;
            MinionCommand(owner, target);
            MinionDamage(owner, target);
            return;
        }

        Player* player = attacker->ToPlayer();
        if (!player || player->getClass() != CLASS_WARLOCK)
            return;

        // The gargoyle's bile burns hotter for each disease (the gargoyle casts it for its master)
        if (spellInfo->Id == SpellPlagueBile)
            damage += damage * CountDiseases(target, player->GetGUID()) / 2;

        uint32 value = uint32(damage);
        ApplyPct(value, DamageBonusPct(player, target, spellInfo, false));
        damage = int32(value);

        if (IsFrostNuke(spellInfo->Id) && HasTalent(player, TalentCryptPlague))
            Later(player, target, [](Player* p, Unit* t) { PlantCryptPlague(p, t, 2); });
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        Player* owner = MinionOwner(attacker);
        if (!owner || !target || !damage)
            return;
        if (target->GetAura(SpellParasites, owner->GetGUID()))
            ApplyPct(damage, float(sSpellMgr->AssertSpellInfo(SpellParasites)->Effects[EFFECT_0].CalcValue()));
        MinionDamage(owner, target);
    }
};

// 992003 - Contagion. The first cast refreshes the caster's diseases on the target and carries copies of them; the
// next gives the copies to another enemy (Spreading Sickness: two more). The cooldown starts once the copies are
// given away or lost.
class spell_necro_contagion : public SpellScript
{
    PrepareSpellScript(spell_necro_contagion);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellContagionCarry, SpellRotEcho });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        BlightState* state = State(player);

        if (player->HasAura(SpellContagionCarry) && !state->carried.empty())
        {
            for (DiseaseCopy const& copy : state->carried)
            {
                if (Aura* old = target->GetAura(copy.spellId, player->GetGUID()))
                    old->Remove();
                Aura* aura = player->AddAura(copy.spellId, target);
                if (!aura)
                    continue;
                aura->SetStackAmount(copy.stacks);
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    if (AuraEffect* effect = aura->GetEffect(i))
                        effect->ChangeAmount(copy.amounts[i]);
                aura->SetMaxDuration(copy.maxDuration);
                aura->SetDuration(copy.duration);
            }
            if (state->charges > 1)
            {
                --state->charges;
                _keepReady = true;
            }
            else
            {
                state->spent = true;
                player->RemoveAurasDueToSpell(SpellContagionCarry);
            }
            return;
        }

        state->carried.clear();
        for (Aura* aura : Diseases(target, player->GetGUID()))
        {
            aura->RefreshDuration();
            DiseaseCopy copy;
            copy.spellId = aura->GetId();
            copy.duration = aura->GetDuration();
            copy.maxDuration = aura->GetMaxDuration();
            copy.stacks = aura->GetStackAmount();
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                if (AuraEffect const* effect = aura->GetEffect(i))
                    copy.amounts[i] = effect->GetAmount();
            state->carried.push_back(copy);
        }
        if (state->carried.empty())
            return;   // nothing to carry: the cooldown runs as usual
        state->charges = 1 + (HasTalent(player, TalentSpreadingSickness) ? 1 : 0);
        state->spent = false;
        player->CastSpell(player, SpellContagionCarry, true);
        if (HasTalent(player, TalentRotEcho))
            player->CastSpell(player, SpellRotEcho, true);
        _keepReady = true;
    }

    void ClearCooldown()
    {
        if (!_keepReady)
            return;
        if (Player* player = GetCaster()->ToPlayer())
            player->RemoveSpellCooldown(GetSpellInfo()->Id, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_necro_contagion::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_necro_contagion::ClearCooldown);
    }

    bool _keepReady = false;
};

// 992005 - Contagion's carried copies. Lost to time, they start Contagion's cooldown.
class spell_necro_contagion_carry : public AuraScript
{
    PrepareAuraScript(spell_necro_contagion_carry);

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player)
            return;
        BlightState* state = State(player);
        bool const spent = state->spent;
        state->carried.clear();
        state->charges = 0;
        state->spent = false;
        if (spent || player->HasSpellCooldown(SpellContagion))
            return;
        player->AddSpellCooldown(SpellContagion, 0, ContagionCooldownMs, true);
        WorldPacket data;
        player->BuildCooldownPacket(data, SPELL_COOLDOWN_FLAG_NONE, SpellContagion, ContagionCooldownMs);
        player->SendDirectMessage(&data);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_necro_contagion_carry::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Plague Strike, Grave Worms, Plague of the Risen, Wasting Plague and Crypt Plague: Mutation makes those applied while
// it lasts tick faster.
class spell_necro_disease : public AuraScript
{
    PrepareAuraScript(spell_necro_disease);

    void CalcPeriodic(AuraEffect const* /*aurEff*/, bool& isPeriodic, int32& amplitude)
    {
        if (!isPeriodic || amplitude <= 0)
            return;
        if (Unit* caster = GetCaster())
            if (AuraEffect const* mutation = caster->GetAuraEffect(SpellMutation, EFFECT_0))
                amplitude = std::max(500, amplitude * 100 / (100 + mutation->GetAmount()));
    }

    void Register() override
    {
        DoEffectCalcPeriodic += AuraEffectCalcPeriodicFn(spell_necro_disease::CalcPeriodic, EFFECT_ALL, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 992001 - Plague of the Risen: cured or dispelled, it bursts and stuns the target.
class spell_necro_plague_of_the_risen : public AuraScript
{
    PrepareAuraScript(spell_necro_plague_of_the_risen);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellRisenBurst });
    }

    void HandleDispel(DispelInfo* /*dispelInfo*/)
    {
        if (Unit* caster = GetCaster())
            if (Unit* target = GetUnitOwner())
                caster->CastSpell(target, SpellRisenBurst, true);
    }

    void Register() override
    {
        AfterDispel += AuraDispelFn(spell_necro_plague_of_the_risen::HandleDispel);
    }
};

// 992131 - Revenant's Touch: spell critical strikes plant Crypt Plague.
class spell_necro_revenants_touch : public AuraScript
{
    PrepareAuraScript(spell_necro_revenants_touch);

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Player* player = GetTarget()->ToPlayer();
        Unit* target = eventInfo.GetActionTarget();
        if (player && target && target != player)
            Later(player, target, [](Player* p, Unit* t) { PlantCryptPlague(p, t, 1); });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_necro_revenants_touch::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// -992132 - Parasites: damaging spells sometimes infest the target, and the minion hits it harder.
class spell_necro_parasites : public AuraScript
{
    PrepareAuraScript(spell_necro_parasites);

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Player* player = GetTarget()->ToPlayer();
        Unit* target = eventInfo.GetActionTarget();
        if (player && target && target != player)
            Later(player, target, [](Player* p, Unit* t) { p->AddAura(SpellParasites, t); });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_necro_parasites::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// -992134 - Pestilent Haste: periodic damage sometimes quickens the Necromancer and the minion.
class spell_necro_pestilent_haste : public AuraScript
{
    PrepareAuraScript(spell_necro_pestilent_haste);

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player)
            return;
        player->AddAura(SpellPestilentHaste, player);
        if (Pet* pet = player->GetPet())
            if (pet->IsAlive())
                player->AddAura(SpellPestilentHaste, pet);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_necro_pestilent_haste::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 990380 - the gargoyle Raise Gargoyle calls. It flies over its master's target and spits Plague Bile, which its
// master casts in effect (spell power and the disease bonus are the master's).
struct npc_necro_gargoyle : public ScriptedAI
{
    npc_necro_gargoyle(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        me->SetReactState(REACT_PASSIVE);
        _boltTimer = 1000;
    }

    Unit* Target(Unit* owner) const
    {
        Unit* target = owner->GetVictim();
        if (!target)
            if (Player* player = owner->ToPlayer())
                target = player->GetSelectedUnit();
        return target && me->IsValidAttackTarget(target) && target->IsAlive() ? target : nullptr;
    }

    void UpdateAI(uint32 diff) override
    {
        Unit* owner = me->GetOwner();
        if (!owner)
            return;
        Unit* target = Target(owner);
        if (!target)
        {
            if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
            {
                me->GetMotionMaster()->Clear(false);
                me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, me->GetFollowAngle());
            }
            return;
        }

        if (!me->IsWithinDistInMap(target, 25.0f))
        {
            me->GetMotionMaster()->MoveChase(target, 15.0f);
            return;
        }
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == FOLLOW_MOTION_TYPE)
            me->GetMotionMaster()->Clear(false);
        me->SetFacingToObject(target);

        if (_boltTimer > diff)
        {
            _boltTimer -= diff;
            return;
        }
        me->CastSpell(target, SpellPlagueBile, true, nullptr, nullptr, owner->GetGUID());
        _boltTimer = 2000;
    }

private:
    uint32 _boltTimer = 1000;
};

// 990381 - a rotling Muck Summoner raises. It runs at its target and bursts when it gets there or its fuse runs out.
struct npc_necro_rotling : public ScriptedAI
{
    npc_necro_rotling(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        me->SetReactState(REACT_PASSIVE);
        _fuse = RotlingFuseMs;
        _burst = false;
    }

    void SetGUID(ObjectGuid const& guid, int32 /*id*/) override
    {
        _target = guid;
        if (Unit* target = ObjectAccessor::GetUnit(*me, guid))
            me->GetMotionMaster()->MoveChase(target);
    }

    void UpdateAI(uint32 diff) override
    {
        if (_burst)
            return;
        Unit* target = ObjectAccessor::GetUnit(*me, _target);
        bool const there = target && me->IsWithinMeleeRange(target);
        if (!there && _fuse > diff && target && target->IsAlive())
        {
            _fuse -= diff;
            return;
        }
        _burst = true;
        ObjectGuid master;
        if (TempSummon* summon = me->ToTempSummon())
            master = summon->GetSummonerGUID();
        me->CastSpell(me, SpellRotlingBurst, true, nullptr, nullptr, master);
        me->DespawnOrUnsummon(500ms);
    }

private:
    ObjectGuid _target;
    uint32 _fuse = RotlingFuseMs;
    bool _burst = false;
};

void AddSC_necromancer_blight()
{
    new necromancer_blight_damage();
    RegisterSpellScript(spell_necro_contagion);
    RegisterSpellScript(spell_necro_contagion_carry);
    RegisterSpellScript(spell_necro_disease);
    RegisterSpellScript(spell_necro_plague_of_the_risen);
    RegisterSpellScript(spell_necro_revenants_touch);
    RegisterSpellScript(spell_necro_parasites);
    RegisterSpellScript(spell_necro_pestilent_haste);
    RegisterCreatureAI(npc_necro_gargoyle);
    RegisterCreatureAI(npc_necro_rotling);
}
