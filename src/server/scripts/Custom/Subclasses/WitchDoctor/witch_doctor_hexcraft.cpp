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
#include "UnitScript.h"

// Hexcraft, the Witch Doctor path's own talent tree (solo-wow patches/client/spell-data/subclasses/witch-doctor-tree.json,
// data/sql/custom/db_world/subclasses/2026_11_20_03_witch_doctor_tree.sql). Talents that are plain spell modifiers live
// in the spell data; the rest are dummy auras whose values this file reads. Hex Doll is a debuff that stores a share of
// the shaman's damage and pays it out when it ends; Wandering Spirit is a buff of up to 5 stacks, with a hidden
// companion aura carrying the per-stack bonuses three talents hang on it.
namespace
{
// Abilities and the effects talents cast
constexpr uint32 SpellShadePuppet = 994000;        // first rank
constexpr uint32 SpellLoasSpite = 994001;          // first rank
constexpr uint32 SpellWitchfire = 994002;          // first rank
constexpr uint32 SpellMojoGolem = 994007;
constexpr uint32 SpellHexDoll = 994010;            // the debuff that holds the store
constexpr uint32 SpellHexDollPayout = 994011;
constexpr uint32 SpellWanderingSpirit = 994012;
constexpr uint32 SpellSpiritCompanion = 994013;    // the hidden per-stack bonuses
constexpr uint32 SpellEcho = 994014;
constexpr uint32 SpellJinxSilence = 994015;
constexpr uint32 SpellSpiteHealingCut = 994016;
constexpr uint32 SpellPinsAndNeedles = 994020;
constexpr uint32 SpellSplinter = 994021;
constexpr uint32 SpellHexedStrike = 994023;
constexpr uint32 SpellHexedHex = 994024;
constexpr uint32 SpellMarionetteHaste = 994027;
constexpr uint32 SpellMarionetteForm = 994028;
constexpr uint32 SpellGolemMagnet = 994029;
constexpr uint32 SpellLoasRideBurst = 994030;      // first rank; no talent teaches the ranks
constexpr uint32 SpellMarionetteBurst = 994031;    // first rank; no talent teaches the ranks

// Talents (first rank)
constexpr uint32 TalentHexDoll = 994103;
constexpr uint32 TalentSpiritCatcher = 994106;
constexpr uint32 TalentJujuSurge = 994115;
constexpr uint32 TalentCatchingCurse = 994118;
constexpr uint32 TalentRestlessShades = 994123;
constexpr uint32 TalentFesteringHex = 994129;
constexpr uint32 TalentBurningDoll = 994131;
constexpr uint32 TalentPinsAndNeedles = 994132;
constexpr uint32 TalentGuidingSpirits = 994134;
constexpr uint32 TalentMarkedForTheLoa = 994136;
constexpr uint32 TalentSplinteringHex = 994139;
constexpr uint32 TalentWardsEcho = 994140;
constexpr uint32 TalentHexedLightning = 994142;
constexpr uint32 TalentWrathfulGolem = 994145;

// The path's spells (2026_11_16_01_witch_doctor_path.sql)
constexpr uint32 VoodooShockFirst = 990700;        // Flame Shock's mirror, 990700-990708
constexpr uint32 VoodooShockLast = 990708;
constexpr uint32 SpiritNovaDamageFirst = 990720;   // Fire Nova's damage mirror, 990720-990728
constexpr uint32 SpiritNovaDamageLast = 990728;

// The shaman's own
constexpr uint32 SpellElementalFury = 16089;       // a rank of the talent; its chain gives the rest
constexpr uint32 SpellLightningOverload = 45284;   // first rank of Lightning Overload's Lightning Bolt copies

// The tree's creatures
constexpr uint32 NpcEchoWard = 990811;
constexpr uint32 NpcMarionette = 990813;

constexpr float SpiritRegenPct = 0.25f;            // of maximum health and mana, per stack and tick
constexpr float SplinterRange = 8.0f;
constexpr uint8 SplinterTargets = 2;
constexpr float HexedLightningRange = 8.0f;
constexpr uint8 HexedLightningTargets = 2;
constexpr float CreatureSearchRange = 100.0f;
constexpr float ShadeChaseRange = 60.0f;

struct HexcraftState : public DataMap::Base
{
    uint8 marionettesLost = 0;   // copies killed during the current Marionette Dance
};

HexcraftState* State(Player* player)
{
    return player->CustomData.GetDefault<HexcraftState>("WitchDoctorHexcraft");
}

uint64 Now()
{
    return GameTime::GetGameTimeMS().count();
}

Player* Shaman(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_SHAMAN ? player : nullptr;
}

// The value of a ranked talent the unit has, or 0
int32 Talent(Unit* unit, uint32 firstRank, uint8 effIndex = EFFECT_0)
{
    AuraEffect const* effect = unit->GetAuraEffectOfRankedSpell(firstRank, effIndex);
    return effect ? effect->GetAmount() : 0;
}

bool HasTalent(Unit* unit, uint32 firstRank)
{
    return unit->GetAuraEffectOfRankedSpell(firstRank, EFFECT_0) != nullptr;
}

uint32 FirstRank(uint32 id) { return sSpellMgr->GetFirstSpellInChain(id); }

bool IsVoodooShock(uint32 id) { return id >= VoodooShockFirst && id <= VoodooShockLast; }
bool IsSpiritNovaDamage(uint32 id) { return id >= SpiritNovaDamageFirst && id <= SpiritNovaDamageLast; }

// Lightning Bolt itself, not Lightning Overload's copies (they carry its family flag too)
bool IsLightningBolt(SpellInfo const* spellInfo)
{
    return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_SHAMAN && (spellInfo->SpellFamilyFlags[0] & 0x1) &&
           FirstRank(spellInfo->Id) != SpellLightningOverload;
}

// The highest rank of a chain at or below `level`. For spells no talent teaches the ranks of (Loa's Ride's and
// Marionette Dance's bursts).
uint32 RankForLevel(uint32 first, uint8 level)
{
    uint32 best = first;
    for (uint32 id = sSpellMgr->GetNextSpellInChain(first); id; id = sSpellMgr->GetNextSpellInChain(id))
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(id);
        if (!info || info->SpellLevel > level)
            break;
        best = id;
    }
    return best;
}

// Cooldowns sit on the rank that was cast, so every rank of the chain is looked at
void FinishCooldown(Player* player, uint32 first)
{
    for (uint32 id = first; id; id = sSpellMgr->GetNextSpellInChain(id))
        if (player->HasSpellCooldown(id))
            player->RemoveSpellCooldown(id, true);
}

void ShortenCooldown(Player* player, uint32 first, int32 ms)
{
    if (ms <= 0)
        return;
    for (uint32 id = first; id; id = sSpellMgr->GetNextSpellInChain(id))
        if (player->HasSpellCooldown(id))
            player->ModifySpellCooldown(id, -ms);
}

// `pct` percent of `damage`, rounded up or down at random in proportion, so small hits still add up on average
uint32 ShareOf(uint32 damage, int32 pct)
{
    if (!damage || pct <= 0)
        return 0;
    float const exact = float(damage) * float(pct) / 100.0f;
    uint32 whole = uint32(exact);
    if (roll_chance_f((exact - float(whole)) * 100.0f))
        ++whole;
    return whole;
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

Aura* OwnVoodooShock(Unit* target, ObjectGuid caster)
{
    for (auto const& [id, application] : target->GetAppliedAuras())
        if (IsVoodooShock(id) && application->GetBase()->GetCasterGUID() == caster)
            return application->GetBase();
    return nullptr;
}

bool HoldsDoll(Unit* target, ObjectGuid caster)
{
    return target->GetAura(SpellHexDoll, caster) != nullptr;
}

// A creature the player summoned, near the player
Creature* OwnSummon(Player* player, uint32 entry)
{
    std::list<Creature*> found;
    player->GetCreatureListWithEntryInGrid(found, entry, CreatureSearchRange);
    for (Creature* creature : found)
    {
        if (!creature->IsAlive())
            continue;
        TempSummon* summon = creature->ToTempSummon();
        if ((summon && summon->GetSummonerGUID() == player->GetGUID()) || creature->GetOwnerGUID() == player->GetGUID())
            return creature;
    }
    return nullptr;
}

// --- Hex Doll ---------------------------------------------------------------------------------------------------------

// Damage from the shaman (or its wards) to `target`: binds a doll if the target holds none of this shaman's, or adds
// the talent's share to the one it holds. A doll that holds is never refreshed.
void FeedDoll(Player* player, Unit* target, uint32 damage)
{
    int32 const share = Talent(player, TalentHexDoll);
    if (!share || !damage || !target || target == player)
        return;
    uint32 const add = ShareOf(damage, share);
    Later(player, target, [add](Player* p, Unit* t)
    {
        if (!t->IsAlive() || !p->IsValidAttackTarget(t))
            return;
        Aura* doll = t->GetAura(SpellHexDoll, p->GetGUID());
        if (!doll || doll->IsRemoved())
        {
            p->AddAura(SpellHexDoll, t);
            return;
        }
        if (AuraEffect* store = doll->GetEffect(EFFECT_0); store && add)
            store->ChangeAmount(store->GetAmount() + int32(add));
    });
}

// Juju Surge: Loa's Spite's cooldown finishes
void JujuSurge(Player* player)
{
    FinishCooldown(player, SpellLoasSpite);
}

// --- Wandering Spirit -------------------------------------------------------------------------------------------------

// Elemental Fury's crit damage bonus on this unit, in percent
int32 ElementalFuryBonus(Unit* unit)
{
    return Talent(unit, FirstRank(SpellElementalFury));
}

// Guiding Spirits' effect 1 never takes the crit bonus past Elemental Fury's +100%
void CapGuidingSpirits(Unit* unit)
{
    AuraEffect* effect = unit->GetAuraEffect(TalentGuidingSpirits, EFFECT_0);
    if (!effect || effect->GetBase()->IsRemoved())
        return;
    int32 const amount = std::max(0, effect->GetSpellInfo()->Effects[EFFECT_0].CalcValue() - ElementalFuryBonus(unit));
    if (amount != effect->GetAmount())
        effect->ChangeAmount(amount);
}

// Keeps the hidden companion aura in step with the Wandering Spirit stacks: Spirit Catcher's Spirit, Restless Shades'
// Shade Puppet damage and Guiding Spirits' Loa's Spite crit, each times the stacks.
void SyncSpirits(Unit* unit)
{
    Aura const* spirits = unit->GetAura(SpellWanderingSpirit);
    int32 const stacks = spirits && !spirits->IsRemoved() ? spirits->GetStackAmount() : 0;
    int32 const amounts[MAX_SPELL_EFFECTS] = {
        Talent(unit, TalentSpiritCatcher, EFFECT_1) * stacks,
        Talent(unit, TalentRestlessShades) * stacks,
        Talent(unit, TalentGuidingSpirits, EFFECT_2) * stacks };

    Aura* companion = unit->GetAura(SpellSpiritCompanion);
    if (!amounts[0] && !amounts[1] && !amounts[2])
    {
        if (companion)
            companion->Remove();
        return;
    }
    if (!companion)
        companion = unit->AddAura(SpellSpiritCompanion, unit);
    if (!companion)
        return;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (AuraEffect* effect = companion->GetEffect(i); effect && effect->GetAmount() != amounts[i])
            effect->ChangeAmount(amounts[i]);
}

// --- Copies of damage -------------------------------------------------------------------------------------------------

// A copy over time (Pins and Needles, Hexed Lightning's hex): `total` spread over the spell's ticks, with any ticks the
// target still has from the last one rolled in, as Ignite does
void ApplyShareDot(Player* player, Unit* target, uint32 spellId, uint32 total)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info || !total || !target->IsAlive())
        return;
    int32 const amplitude = std::max<int32>(1, info->Effects[EFFECT_0].Amplitude);
    int32 const ticks = std::max<int32>(1, info->GetMaxDuration() / amplitude);
    int64 rolled = total;
    if (AuraEffect const* old = target->GetAuraEffect(spellId, EFFECT_0, player->GetGUID()))
        if (int32 const left = old->GetTotalTicks() - int32(old->GetTickNumber()); left > 0)
            rolled += int64(old->GetAmount()) * left;
    int32 perTick = int32(rolled / ticks);
    if (perTick > 0)
        player->CastCustomSpell(target, spellId, &perTick, nullptr, nullptr, true);
}

// Splintering Hex: up to 2 other enemies near the target take the talent's share of the hit
void Splinter(Player* player, Unit* target, uint32 damage)
{
    int32 bp = int32(ShareOf(damage, Talent(player, TalentSplinteringHex)));
    if (bp <= 0)
        return;
    uint8 hit = 0;
    for (Unit* other : EnemiesNear(player, target, SplinterRange))
    {
        if (hit >= SplinterTargets)
            break;
        if (other == target || !other->IsAlive() || !player->IsValidAttackTarget(other))
            continue;
        player->CastCustomSpell(other, SpellSplinter, &bp, nullptr, nullptr, true);
        ++hit;
    }
}

// Catching Curse: the shaman's Voodoo Shock on `from` (its periodic part, time left and tick amount) catches on the
// nearest enemy in range that lacks one
void SpreadVoodooShock(Player* player, Unit* from)
{
    int32 const range = Talent(player, TalentCatchingCurse);
    Aura* shock = range ? OwnVoodooShock(from, player->GetGUID()) : nullptr;
    if (!shock)
        return;
    Unit* nearest = nullptr;
    float nearestDist = 0.0f;
    for (Unit* other : EnemiesNear(player, from, float(range)))
    {
        if (other == from || !other->IsAlive() || !player->IsValidAttackTarget(other) || OwnVoodooShock(other, player->GetGUID()))
            continue;
        float const dist = from->GetDistance(other);
        if (!nearest || dist < nearestDist)
        {
            nearest = other;
            nearestDist = dist;
        }
    }
    if (!nearest)
        return;
    Aura* copy = player->AddAura(shock->GetId(), nearest);
    if (!copy)
        return;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (AuraEffect* effect = copy->GetEffect(i))
            if (AuraEffect const* original = shock->GetEffect(i))
                effect->ChangeAmount(original->GetAmount());
    copy->SetMaxDuration(shock->GetMaxDuration());
    copy->SetDuration(shock->GetDuration());
}

// Hexed Lightning's charge, spent by a Lightning Bolt: the bolt strikes up to 2 enemies near its target as well, and
// all three take the talent's share of the bolt again over time
void HexedLightning(Player* player, Unit* target, uint32 damage)
{
    uint32 const share = ShareOf(damage, Talent(player, TalentHexedLightning, EFFECT_1));
    int32 strike = int32(damage);
    std::vector<Unit*> struck = { target };
    uint8 hit = 0;
    for (Unit* other : EnemiesNear(player, target, HexedLightningRange))
    {
        if (hit >= HexedLightningTargets)
            break;
        if (other == target || !other->IsAlive() || !player->IsValidAttackTarget(other))
            continue;
        player->CastCustomSpell(other, SpellHexedStrike, &strike, nullptr, nullptr, true);
        struck.push_back(other);
        ++hit;
    }
    for (Unit* unit : struck)
        if (unit->IsAlive())
            ApplyShareDot(player, unit, SpellHexedHex, share);
}

// Loa's Spite (`spite`) or Witchfire landed on `target` for `damage`
void NukeLanded(Player* player, Unit* target, uint32 damage, bool spite)
{
    ObjectGuid const guid = player->GetGUID();
    if (!target->IsAlive())
        return;

    if (spite && OwnVoodooShock(target, guid))
        player->CastSpell(target, SpellSpiteHealingCut, true);

    if (spite && HoldsDoll(target, guid))
        if (uint32 const pins = ShareOf(damage, Talent(player, TalentPinsAndNeedles)))
            Later(player, target, [pins](Player* p, Unit* t) { ApplyShareDot(p, t, SpellPinsAndNeedles, pins); });

    if (HasTalent(player, TalentSplinteringHex) && damage)
        Later(player, target, [damage](Player* p, Unit* t) { Splinter(p, t, damage); });

    if (HasTalent(player, TalentCatchingCurse))
        Later(player, target, [](Player* p, Unit* t) { SpreadVoodooShock(p, t); });

    // Burning Doll: queued after the hit's own Hex Doll proc, so the doll pays out with this hit's share in it
    if (!spite && HasTalent(player, TalentBurningDoll))
        Later(player, target, [](Player* p, Unit* t)
        {
            Aura* doll = t->GetAura(SpellHexDoll, p->GetGUID());
            if (!doll || doll->IsRemoved())
                return;
            doll->Remove(AURA_REMOVE_BY_EXPIRE);   // pays out as on expiry (spell_hexcraft_hex_doll_store)
            p->CastSpell(p, SpellWanderingSpirit, true);
        });
}
}

// Bonuses that depend on the target, Voodoo Shock and Shade Puppet ticks, and Guiding Spirits' cap
class witch_doctor_hexcraft_damage : public UnitScript
{
public:
    witch_doctor_hexcraft_damage() : UnitScript("witch_doctor_hexcraft_damage", true,
        { UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_ON_AURA_APPLY,
          UNITHOOK_ON_AURA_REMOVE }) { }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = Shaman(attacker);
        if (!player || !spellInfo || !target || !damage)
            return;
        bool const shock = IsVoodooShock(spellInfo->Id);
        bool const puppet = !shock && FirstRank(spellInfo->Id) == SpellShadePuppet;
        if (!shock && !puppet)
            return;

        // Festering Hex: each tick of this shock makes the next stronger, up to 10 stacks
        if (shock)
            if (int32 const perStack = Talent(player, TalentFesteringHex))
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    if (spellInfo->Effects[i].ApplyAuraName != SPELL_AURA_PERIODIC_DAMAGE)
                        continue;
                    if (AuraEffect const* effect = target->GetAuraEffect(spellInfo->Id, i, player->GetGUID()))
                    {
                        // the tick number counts this tick already
                        int32 const stacks = std::min<int32>(10, std::max<int32>(0, int32(effect->GetTickNumber()) - 1));
                        if (stacks)
                            damage = uint32(damage * (100.0f + perStack * stacks) / 100.0f);
                    }
                    break;
                }

        // Wrathful Golem: each tick takes time off Mojo Golem's cooldown
        if (int32 const ms = Talent(player, TalentWrathfulGolem, EFFECT_1))
            ShortenCooldown(player, SpellMojoGolem, ms);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = Shaman(attacker);
        if (!player || !spellInfo || !target || damage <= 0)
            return;
        ObjectGuid const guid = player->GetGUID();
        uint32 const first = FirstRank(spellInfo->Id);

        float pct = 0.0f;
        if ((first == SpellLoasSpite || first == SpellWitchfire) && OwnVoodooShock(target, guid))
            pct += Talent(player, TalentMarkedForTheLoa);
        if (IsSpiritNovaDamage(spellInfo->Id) && HoldsDoll(target, guid))
            pct += Talent(player, TalentSplinteringHex, EFFECT_1);
        if (pct > 0.0f)
            damage = int32(damage * (100.0f + pct) / 100.0f);
    }

    // Elemental Fury comes and goes with talents, specs and logins; Guiding Spirits' cap follows it
    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (unit && aura && Shaman(unit) && FirstRank(aura->GetId()) == FirstRank(SpellElementalFury))
            unit->m_Events.AddEventAtOffset([unit]() { CapGuidingSpirits(unit); }, 0ms);
    }

    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode /*mode*/) override
    {
        if (unit && aurApp && Shaman(unit) && FirstRank(aurApp->GetBase()->GetId()) == FirstRank(SpellElementalFury))
            unit->m_Events.AddEventAtOffset([unit]() { CapGuidingSpirits(unit); }, 0ms);
    }
};

// 994103 - Hex Doll, the talent: the shaman's damaging spells, direct or periodic, bind or feed the target's doll
class spell_hexcraft_hex_doll : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_hex_doll);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellHexDoll, SpellHexDollPayout });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        Unit* target = eventInfo.GetActionTarget();
        if (!damageInfo || !damageInfo->GetDamage() || !spellInfo || !target || target == GetTarget())
            return false;
        // the copies never feed it (the proc's family mask already leaves them out)
        uint32 const id = spellInfo->Id;
        return id != SpellHexDollPayout && id != SpellEcho && id != SpellPinsAndNeedles && id != SpellHexedHex;
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        if (Player* player = Shaman(GetTarget()))
            FeedDoll(player, eventInfo.GetActionTarget(), eventInfo.GetDamageInfo()->GetDamage());
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_hexcraft_hex_doll::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_hexcraft_hex_doll::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 994010 - Hex Doll, the debuff. When its time runs out (or Burning Doll burns it) the target takes the whole store as
// one shadow hit; if the target dies first the store is lost. The payout triggers no procs, so Juju Surge is rolled here.
class spell_hexcraft_hex_doll_store : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_hex_doll_store);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellHexDollPayout });
    }

    void HandleRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        int32 store = aurEff->GetAmount();
        if (!caster || !caster->IsAlive() || store <= 0 || !target->IsAlive())
            return;
        caster->CastCustomSpell(target, SpellHexDollPayout, &store, nullptr, nullptr, true);
        if (Player* player = Shaman(caster))
            if (int32 const chance = Talent(player, TalentJujuSurge); chance && roll_chance_i(chance))
                JujuSurge(player);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_hexcraft_hex_doll_store::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// -990750 Venom Spit (the Serpent Ward's bolt) and -990780 the Voodoo Ward's pulse. A ward is the caster, so the
// shaman's Hex Doll proc never sees these: the ward feeds its owner's doll here.
class spell_hexcraft_ward_hit : public SpellScript
{
    PrepareSpellScript(spell_hexcraft_ward_hit);

    void HandleAfterHit()
    {
        Unit* ward = GetCaster();
        Unit* target = GetHitUnit();
        int32 const damage = GetHitDamage();
        if (!ward || !target || damage <= 0)
            return;
        if (Player* player = Shaman(ward->GetOwner()))
            FeedDoll(player, target, uint32(damage));
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_hexcraft_ward_hit::HandleAfterHit);
    }
};

// 994115 - Juju Surge: Shade Puppet's ticks sometimes finish Loa's Spite's cooldown (the chance is the spell's own)
class spell_hexcraft_juju_surge : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_juju_surge);

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        if (Player* player = Shaman(GetTarget()))
            JujuSurge(player);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_hexcraft_juju_surge::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 994012 - Wandering Spirit: each stack restores 0.25% of maximum health and mana a tick (the core multiplies by the
// stacks), and the companion aura follows the stack count.
class spell_hexcraft_wandering_spirit : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_wandering_spirit);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellSpiritCompanion });
    }

    void CalcHeal(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Unit* owner = GetUnitOwner())
            amount = std::max<int32>(1, int32(owner->GetMaxHealth() * SpiritRegenPct / 100.0f));
    }

    void CalcMana(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Unit* owner = GetUnitOwner())
            amount = std::max<int32>(owner->GetMaxPower(POWER_MANA) ? 1 : 0, int32(owner->GetMaxPower(POWER_MANA) * SpiritRegenPct / 100.0f));
    }

    // Also runs on each new stack (the core reapplies the effects with the new count)
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        SyncSpirits(GetTarget());
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        SyncSpirits(GetTarget());
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hexcraft_wandering_spirit::CalcHeal, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hexcraft_wandering_spirit::CalcMana, EFFECT_1, SPELL_AURA_PERIODIC_ENERGIZE);
        AfterEffectApply += AuraEffectApplyFn(spell_hexcraft_wandering_spirit::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(spell_hexcraft_wandering_spirit::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 994134 - Guiding Spirits: effect 1 (Elemental Fury's reach) is lowered by Elemental Fury's own bonus, so the two
// never pass +100%. Effect 2 (Hexcraft's own spells) stands as it is.
class spell_hexcraft_guiding_spirits : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_guiding_spirits);

    void CalcAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Unit* owner = GetUnitOwner())
            amount = std::max(0, amount - ElementalFuryBonus(owner));
    }

    // The per-stack crit (effect 3) lives on the companion aura: bring it in line with the talent
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        SyncSpirits(GetTarget());
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hexcraft_guiding_spirits::CalcAmount, EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER);
        AfterEffectApply += AuraEffectApplyFn(spell_hexcraft_guiding_spirits::HandleApply, EFFECT_2, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 994003 - Echo Ward. While the ward stands, the shaman's Lightning Bolts (and Loa's Spite, with Ward's Echo) are
// echoed by the ward on the same target for a share of the damage dealt. Lightning Overload's copies, Hexed
// Lightning's strikes and echoes never echo (the proc's family mask and the check below leave them out).
class spell_hexcraft_echo_ward : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_echo_ward);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellEcho });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        Unit* target = eventInfo.GetActionTarget();
        if (!damageInfo || !damageInfo->GetDamage() || !spellInfo || !target || target == GetTarget())
            return false;
        if (IsLightningBolt(spellInfo))
            return true;
        return FirstRank(spellInfo->Id) == SpellLoasSpite && HasTalent(GetTarget(), TalentWardsEcho);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Player* player = Shaman(GetTarget());
        Unit* target = eventInfo.GetActionTarget();
        if (!player || !target)
            return;
        int32 const echo = int32(ShareOf(eventInfo.GetDamageInfo()->GetDamage(), aurEff->GetAmount()));
        if (echo <= 0)
            return;
        Later(player, target, [echo](Player* p, Unit* t)
        {
            if (!t->IsAlive() || !p->IsValidAttackTarget(t))
                return;
            int32 bp = echo;
            // The ward throws the bolt; the damage is the shaman's
            Creature* ward = OwnSummon(p, NpcEchoWard);
            if (ward && ward->IsWithinDistInMap(t, sSpellMgr->AssertSpellInfo(SpellEcho)->GetMaxRange(false)))
                ward->CastCustomSpell(t, SpellEcho, &bp, nullptr, nullptr, true, nullptr, nullptr, p->GetGUID());
            else
                p->CastCustomSpell(t, SpellEcho, &bp, nullptr, nullptr, true);
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_hexcraft_echo_ward::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_hexcraft_echo_ward::HandleProc, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// 994005 - Choking Jinx: the holder's next spell cast silences it, as the shaman (the spell_proc row's one charge then
// takes the jinx away). One jinx per target: a new one takes any other shaman's off.
class spell_hexcraft_choking_jinx : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_choking_jinx);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellJinxSilence });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        ObjectGuid const mine = GetCasterGUID();
        std::vector<Aura*> others;
        auto const range = target->GetAppliedAuras().equal_range(GetId());
        for (auto itr = range.first; itr != range.second; ++itr)
            if (itr->second->GetBase()->GetCasterGUID() != mine)
                others.push_back(itr->second->GetBase());
        for (Aura* other : others)
            other->Remove();
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return eventInfo.GetActor() == GetTarget() && eventInfo.GetSpellInfo() && GetCaster();
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        Unit* caster = GetCaster();
        Unit* holder = GetTarget();
        if (caster && holder->IsAlive())
            caster->CastSpell(holder, SpellJinxSilence, true);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_hexcraft_choking_jinx::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        DoCheckProc += AuraCheckProcFn(spell_hexcraft_choking_jinx::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_hexcraft_choking_jinx::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// -994001 - Loa's Spite: the healing cut on a target with the shaman's Voodoo Shock, Pins and Needles, Splintering Hex,
// Catching Curse, and Ward's Echo's cut to Shade Puppet's cooldown
class spell_hexcraft_loas_spite : public SpellScript
{
    PrepareSpellScript(spell_hexcraft_loas_spite);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellSpiteHealingCut, SpellPinsAndNeedles, SpellSplinter });
    }

    void HandleAfterHit()
    {
        Player* player = Shaman(GetCaster());
        Unit* target = GetHitUnit();
        int32 const damage = GetHitDamage();
        if (player && target && damage > 0)
            NukeLanded(player, target, uint32(damage), true);
    }

    void HandleAfterCast()
    {
        if (Player* player = Shaman(GetCaster()))
            if (int32 const seconds = Talent(player, TalentWardsEcho))
                ShortenCooldown(player, SpellShadePuppet, seconds * IN_MILLISECONDS);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_hexcraft_loas_spite::HandleAfterHit);
        AfterCast += SpellCastFn(spell_hexcraft_loas_spite::HandleAfterCast);
    }
};

// -994002 - Witchfire: Splintering Hex, Catching Curse and Burning Doll
class spell_hexcraft_witchfire : public SpellScript
{
    PrepareSpellScript(spell_hexcraft_witchfire);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellSplinter, SpellHexDoll, SpellWanderingSpirit });
    }

    void HandleAfterHit()
    {
        Player* player = Shaman(GetCaster());
        Unit* target = GetHitUnit();
        int32 const damage = GetHitDamage();
        if (player && target && damage > 0)
            NukeLanded(player, target, uint32(damage), false);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_hexcraft_witchfire::HandleAfterHit);
    }
};

// 994022 - Hexed Lightning's charge, spent by the next Lightning Bolt to land (the spell_proc row's one charge)
class spell_hexcraft_hexed_lightning : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_hexed_lightning);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellHexedStrike, SpellHexedHex });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        Unit* target = eventInfo.GetActionTarget();
        return damageInfo && damageInfo->GetDamage() && target && target != GetTarget() &&
               IsLightningBolt(eventInfo.GetSpellInfo());
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Player* player = Shaman(GetTarget());
        Unit* target = eventInfo.GetActionTarget();
        if (!player || !target)
            return;
        uint32 const damage = eventInfo.GetDamageInfo()->GetDamage();
        Later(player, target, [damage](Player* p, Unit* t) { HexedLightning(p, t, damage); });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_hexcraft_hexed_lightning::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_hexcraft_hexed_lightning::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 994026 - Other Side: a new stack doesn't renew the buff (the time left is put back after each stack)
class spell_hexcraft_other_side : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_other_side);

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes mode)
    {
        uint64 const now = Now();
        if ((mode & AURA_EFFECT_HANDLE_REAL) || !_ends || _ends <= now)
        {
            _ends = now + uint64(std::max(0, GetDuration()));
            return;
        }
        SetDuration(int32(_ends - now));
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_hexcraft_other_side::HandleApply, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }

    uint64 _ends = 0;
};

// 994006 - Loa's Ride: each tick of the shaman's periodic damage frees a spirit that bursts on the tick's target. The
// burst's ranks follow the shaman's level (no talent teaches them).
class spell_hexcraft_loas_ride : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_loas_ride);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellLoasRideBurst });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        Unit* target = eventInfo.GetActionTarget();
        return damageInfo && damageInfo->GetDamage() && target && target != GetTarget() && target->IsAlive();
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Player* player = Shaman(GetTarget());
        Unit* target = eventInfo.GetActionTarget();
        if (!player || !target)
            return;
        Later(player, target, [](Player* p, Unit* t)
        {
            if (t->IsAlive() && p->IsValidAttackTarget(t))
                p->CastSpell(t, RankForLevel(SpellLoasRideBurst, p->GetLevel()), true);
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_hexcraft_loas_ride::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_hexcraft_loas_ride::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 994007 - Mojo Golem: the shaman absorbs damage up to effect 3's share of its maximum health
class spell_hexcraft_mojo_golem : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_mojo_golem);

    void CalcAbsorb(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Unit* owner = GetUnitOwner())
            amount = int32(CalculatePct(owner->GetMaxHealth(), GetSpellInfo()->Effects[EFFECT_2].CalcValue()));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hexcraft_mojo_golem::CalcAbsorb, EFFECT_1, SPELL_AURA_SCHOOL_ABSORB);
    }
};

// 994008 - Marionette Dance. Every 2 sec each copy still standing adds a stack of Marionette; at 20 the stacks become
// the Marionette form and the copies burst (the burst's ranks follow the shaman's level).
class spell_hexcraft_marionette_dance : public AuraScript
{
    PrepareAuraScript(spell_hexcraft_marionette_dance);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SpellMarionetteHaste, SpellMarionetteForm, SpellMarionetteBurst });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = Shaman(GetTarget()))
            State(player)->marionettesLost = 0;
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Player* player = Shaman(GetTarget());
        if (!player)
            return;
        int32 const copies = GetSpellInfo()->Effects[EFFECT_2].CalcValue();
        int32 const standing = std::max(0, copies - int32(State(player)->marionettesLost));
        if (!standing)
            return;
        player->m_Events.AddEventAtOffset([player, standing]()
        {
            if (!player->IsAlive() || player->HasAura(SpellMarionetteForm))
                return;
            AddStacks(player, SpellMarionetteHaste, uint8(standing));
            Aura* haste = player->GetAura(SpellMarionetteHaste);
            if (!haste || haste->GetStackAmount() < haste->GetSpellInfo()->StackAmount)
                return;
            haste->Remove();
            player->CastSpell(player, SpellMarionetteForm, true);
            player->CastSpell(player, RankForLevel(SpellMarionetteBurst, player->GetLevel()), true);
            std::list<Creature*> found;
            player->GetCreatureListWithEntryInGrid(found, NpcMarionette, CreatureSearchRange);
            for (Creature* copy : found)
                if (copy->GetOwnerGUID() == player->GetGUID())
                    copy->DespawnOrUnsummon();
        }, 0ms);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_hexcraft_marionette_dance::HandleApply, EFFECT_0, SPELL_AURA_MOD_ROOT, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_hexcraft_marionette_dance::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// The tree's creatures take their summoner as owner, with its faction and level, if the summon didn't set them
static void AdoptSummoner(Creature* me, WorldObject* summoner)
{
    Unit* owner = summoner ? summoner->ToUnit() : nullptr;
    if (!owner)
        return;
    if (!me->GetOwnerGUID())
        me->SetOwnerGUID(owner->GetGUID());
    me->SetFaction(owner->GetFaction());
    me->SetLevel(owner->GetLevel());
}

// A creature that stands where it was put and never fights
struct npc_hexcraft_standing : public ScriptedAI
{
    npc_hexcraft_standing(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        me->SetReactState(REACT_PASSIVE);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        AdoptSummoner(me, summoner);
        me->GetMotionMaster()->Clear(false);
        me->GetMotionMaster()->MoveIdle();
        me->SetControlled(true, UNIT_STATE_ROOT);
        if (Unit* owner = me->GetOwner())
            me->SetFacingTo(owner->GetOrientation());
    }

    void AttackStart(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }
    void EnterEvadeMode(EvadeReason /*why*/) override { }
    void UpdateAI(uint32 /*diff*/) override { }
};

// 990811 - Echo Ward: a ward body that throws the echoes (spell_hexcraft_echo_ward). Not a totem.
struct npc_hexcraft_echo_ward : public npc_hexcraft_standing
{
    npc_hexcraft_echo_ward(Creature* creature) : npc_hexcraft_standing(creature) { }
};

// 990812 - Mojo Golem: carries Grounding Totem's spell magnet for its owner's party. Not a totem.
struct npc_hexcraft_mojo_golem : public npc_hexcraft_standing
{
    npc_hexcraft_mojo_golem(Creature* creature) : npc_hexcraft_standing(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        npc_hexcraft_standing::IsSummonedBy(summoner);
        me->CastSpell(me, SpellGolemMagnet, true);
    }
};

// 990813 - a Marionette Dance copy: wears its master's look (Mirror Image's Clone Me), stands and channels. A copy that
// is killed adds no more stacks.
struct npc_hexcraft_marionette : public npc_hexcraft_standing
{
    npc_hexcraft_marionette(Creature* creature) : npc_hexcraft_standing(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        npc_hexcraft_standing::IsSummonedBy(summoner);
        if (Unit* owner = me->GetOwner())
        {
            owner->CastSpell(me, 45204 /*Clone Me!*/, true);
            me->SetFacingToObject(owner);
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Player* owner = Shaman(me->GetOwner()))
        {
            HexcraftState* state = State(owner);
            if (state->marionettesLost < 255)
                ++state->marionettesLost;
        }
    }
};

// 990810 - Shade Puppet's shade: runs at its master's target and fades with the spell (the damage is the master's own
// aura on the target). Cosmetic: it can't be attacked or selected.
struct npc_hexcraft_shade : public ScriptedAI
{
    npc_hexcraft_shade(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        me->SetReactState(REACT_PASSIVE);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        AdoptSummoner(me, summoner);
        Unit* owner = summoner ? summoner->ToUnit() : nullptr;
        if (!owner)
            return;
        Unit* target = owner->GetVictim();
        if (!target)
            if (Player* player = owner->ToPlayer())
                target = player->GetSelectedUnit();
        if (target && target->IsAlive() && me->IsWithinDistInMap(target, ShadeChaseRange))
            me->GetMotionMaster()->MoveChase(target);
    }

    void AttackStart(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }
    void EnterEvadeMode(EvadeReason /*why*/) override { }
    void UpdateAI(uint32 /*diff*/) override { }
};

void AddSC_witch_doctor_hexcraft()
{
    new witch_doctor_hexcraft_damage();
    RegisterSpellScript(spell_hexcraft_hex_doll);
    RegisterSpellScript(spell_hexcraft_hex_doll_store);
    RegisterSpellScript(spell_hexcraft_ward_hit);
    RegisterSpellScript(spell_hexcraft_juju_surge);
    RegisterSpellScript(spell_hexcraft_wandering_spirit);
    RegisterSpellScript(spell_hexcraft_guiding_spirits);
    RegisterSpellScript(spell_hexcraft_echo_ward);
    RegisterSpellScript(spell_hexcraft_choking_jinx);
    RegisterSpellScript(spell_hexcraft_loas_spite);
    RegisterSpellScript(spell_hexcraft_witchfire);
    RegisterSpellScript(spell_hexcraft_hexed_lightning);
    RegisterSpellScript(spell_hexcraft_other_side);
    RegisterSpellScript(spell_hexcraft_loas_ride);
    RegisterSpellScript(spell_hexcraft_mojo_golem);
    RegisterSpellScript(spell_hexcraft_marionette_dance);
    RegisterCreatureAI(npc_hexcraft_shade);
    RegisterCreatureAI(npc_hexcraft_echo_ward);
    RegisterCreatureAI(npc_hexcraft_mojo_golem);
    RegisterCreatureAI(npc_hexcraft_marionette);
}
