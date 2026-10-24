/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <algorithm>
#include <cmath>
#include <vector>

#include "CreatureAI.h"
#include "DBCStores.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SubclassPaths.h"
#include "TemporarySummon.h"

// Sub-class pets (subclass_creature_swap): a replacement creature takes its original's pet stats and scaling, and one
// with bodies > 1 is joined by bodies - 1 guardians of its entry. The pet and its bodies share the original's health,
// and subclass_combat.cpp splits their melee the same way. The extra bodies only melee the pet's target; the pet
// itself keeps its spells and its pet bar.
namespace
{
constexpr char const* BodiesKey = "SubclassBodies";
constexpr uint32 CheckIntervalMs = 1000;

// The extra bodies following one pet
struct Bodies : DataMap::Base
{
    ObjectGuid pet;
    std::vector<ObjectGuid> guids;
    uint32 sinceCheck = 0;

    bool Shares(ObjectGuid guid) const
    {
        return !guids.empty() && (guid == pet || std::find(guids.begin(), guids.end(), guid) != guids.end());
    }
};

SummonPropertiesEntry const* GuardianProperties()
{
    static SummonPropertiesEntry const* const properties = []() -> SummonPropertiesEntry const*
    {
        SummonPropertiesEntry const* fallback = nullptr;
        for (SummonPropertiesEntry const* entry : sSummonPropertiesStore)
        {
            if (entry->Category != SUMMON_CATEGORY_ALLY || entry->Type != SUMMON_TYPE_GUARDIAN)
                continue;
            if (!entry->Slot && !entry->Flags)
                return entry;
            if (!fallback)
                fallback = entry;
        }
        return fallback;
    }();
    return properties;
}

// Follows the owner and melees whatever the owner's pet is fighting
class SubclassBodyAI : public CreatureAI
{
public:
    SubclassBodyAI(Creature* creature, float angle) : CreatureAI(creature), _angle(angle) { }

    void InitializeAI() override { FollowOwner(); }

    void MoveInLineOfSight(Unit* /*who*/) override { }

    void EnterEvadeMode(EvadeReason /*why*/) override { FollowOwner(); }

    void UpdateAI(uint32 /*diff*/) override
    {
        Unit* owner = me->GetOwner();
        Unit* pet = owner ? owner->GetGuardianPet() : nullptr;
        Unit* target = pet ? pet->GetVictim() : nullptr;
        if (target && target != me->GetVictim() && me->IsValidAttackTarget(target))
            AttackStart(target);
        else if (!target && me->GetVictim())
        {
            me->AttackStop();
            FollowOwner();
        }
        if (me->GetVictim())
            DoMeleeAttackIfReady();
    }

private:
    void FollowOwner()
    {
        if (Unit* owner = me->GetOwner())
        {
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, _angle);
        }
    }

    float _angle;
};

SubclassPaths::CreatureSwap const* SplitSwap(Creature const* creature)
{
    SubclassPaths::CreatureSwap const* swap = SubclassPaths::SwapForReplacementCreature(creature->GetEntry());
    return swap && swap->bodies > 1 ? swap : nullptr;
}

// Health auras rebuild the total health percent from auras alone when one ends, so the share is held, not applied once.
void HoldHealthShare(Creature* creature, uint8 bodies)
{
    float const wanted = creature->GetTotalAuraMultiplier(SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT) *
                         SubclassPaths::BodyShare(bodies);
    if (std::fabs(creature->GetPctModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT) - wanted) > 0.0001f)
        creature->SetStatPctModifier(UNIT_MOD_HEALTH, TOTAL_PCT, wanted);
}

// Holds the share on the pet and every living body
void HoldShares(Player* player, Pet* pet, Bodies* bodies)
{
    uint8 const count = uint8(1 + bodies->guids.size());
    HoldHealthShare(pet, count);
    for (ObjectGuid const& guid : bodies->guids)
        if (Creature* body = ObjectAccessor::GetCreature(*player, guid))
            if (body->IsAlive())
                HoldHealthShare(body, count);
}

void Despawn(Player* player, Bodies* bodies)
{
    for (ObjectGuid const& guid : bodies->guids)
        if (Creature* body = ObjectAccessor::GetCreature(*player, guid))
            if (TempSummon* summon = body->ToTempSummon())
                summon->UnSummon();
    bodies->guids.clear();
    bodies->pet.Clear();
}

void Summon(Player* player, Pet* pet, uint8 count, Bodies* bodies)
{
    bodies->pet = pet->GetGUID();
    SummonPropertiesEntry const* properties = GuardianProperties();
    if (!properties)
        return;
    for (uint8 i = 1; i <= count; ++i)
    {
        float const angle = PET_FOLLOW_ANGLE + std::numbers::pi_v<float> * 2 * i / (count + 1);
        TempSummon* body = player->SummonCreature(pet->GetEntry(), pet->GetPosition(), TEMPSUMMON_CORPSE_DESPAWN, 0, 0,
                                                  properties);
        if (!body)
            continue;
        body->SetReactState(pet->GetReactState());
        body->AIM_Initialize(new SubclassBodyAI(body, angle));
        bodies->guids.push_back(body->GetGUID());
    }
    HoldShares(player, pet, bodies);
    for (ObjectGuid const& guid : bodies->guids)
        if (Creature* body = ObjectAccessor::GetCreature(*player, guid))
            body->SetFullHealth();
}
}  // namespace

namespace SubclassPaths
{
uint8 MeleeBodies(Unit const* minion)
{
    Player const* owner = minion->IsCreature() ? minion->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    Bodies const* bodies = owner ? owner->CustomData.Get<Bodies>(BodiesKey) : nullptr;
    return bodies && bodies->Shares(minion->GetGUID()) ? uint8(1 + bodies->guids.size()) : 1;
}
}

class SubclassMinionsPlayerScript : public PlayerScript
{
public:
    SubclassMinionsPlayerScript()
        : PlayerScript("SubclassMinionsPlayerScript",
                       {PLAYERHOOK_ON_BEFORE_GUARDIAN_INIT_STATS_FOR_LEVEL, PLAYERHOOK_ON_UPDATE})
    {
    }

    // A replacement scales like its original. Bodies are guardians and pets of other classes have no pet type, so
    // both would otherwise get plain creature stats.
    void OnPlayerBeforeGuardianInitStatsForLevel(Player* player, Guardian* guardian, CreatureTemplate const* /*cinfo*/, PetType& petType) override
    {
        SubclassPaths::CreatureSwap const* swap = SubclassPaths::SwapForReplacementCreature(guardian->GetEntry());
        if (!swap)
            return;
        guardian->SetStatEntry(swap->original);
        if (petType == MAX_PET_TYPE && (!guardian->IsPet() || !player->IsClass(CLASS_HUNTER, CLASS_CONTEXT_PET)))
            petType = SUMMON_PET;
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!SubclassPaths::AffectsCombat())
            return;
        Bodies* bodies = player->CustomData.Get<Bodies>(BodiesKey);
        Pet* pet = player->GetPet();
        SubclassPaths::CreatureSwap const* swap = pet ? SplitSwap(pet) : nullptr;
        if (!bodies && !swap)
            return;
        if (!bodies)
            bodies = player->CustomData.GetDefault<Bodies>(BodiesKey);
        if ((bodies->sinceCheck += diff) < CheckIntervalMs)
            return;
        bodies->sinceCheck = 0;

        bool const petReady = swap && pet->IsInWorld() && pet->IsAlive();
        if (!bodies->pet.IsEmpty() && (!petReady || bodies->pet != pet->GetGUID()))
            Despawn(player, bodies);
        if (!petReady)
            return;
        // Bodies come once per pet summon; one that dies stays dead, and its share with it, until the next summon.
        if (bodies->pet.IsEmpty())
            Summon(player, pet, swap->bodies - 1, bodies);
        else
            HoldShares(player, pet, bodies);
    }
};

void AddSC_subclass_minions()
{
    new SubclassMinionsPlayerScript();
}
