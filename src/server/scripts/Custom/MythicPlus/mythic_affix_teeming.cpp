/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <cmath>
#include <numbers>

#include "Creature.h"
#include "CreatureAI.h"
#include "Map.h"
#include "MythicPlus.h"
#include "ObjectAccessor.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"

// Teeming: a fifth of the trash brings a copy that stands beside it and joins its fights. The run chose those
// spawns when the key went in, and their copies count towards enemy forces.
namespace MythicPlus
{
namespace
{
constexpr char const* PartnerKey = "MythicPlusTeeming";
constexpr uint32 CorpseMs = 30000;

struct TeemingPartner : public DataMap::Base
{
    ObjectGuid partner;
    bool copy = false;
};

void SummonCopy(Creature* original)
{
    MythicRun* run = FindRun(original->GetMap());
    if (!run || !original->IsAlive() || original->CustomData.Get<TeemingPartner>(PartnerKey))
        return;
    float const angle = original->GetOrientation() + std::numbers::pi_v<float> / 2.0f;
    float const x = original->GetPositionX() + 2.5f * std::cos(angle);
    float const y = original->GetPositionY() + 2.5f * std::sin(angle);
    TempSummon* copy = original->SummonCreature(original->GetEntry(), x, y, original->GetPositionZ(),
                                                original->GetOrientation(), TEMPSUMMON_CORPSE_TIMED_DESPAWN, CorpseMs);
    if (!copy)
        return;
    copy->CustomData.GetDefault<TeemingPartner>(PartnerKey)->partner = original->GetGUID();
    copy->CustomData.GetDefault<TeemingPartner>(PartnerKey)->copy = true;
    original->CustomData.GetDefault<TeemingPartner>(PartnerKey)->partner = copy->GetGUID();
}
}

bool IsTeemingCopy(Creature* creature)
{
    TeemingPartner const* link = creature->CustomData.Get<TeemingPartner>(PartnerKey);
    return link && link->copy;
}

void BringTeemingCopy(Creature* creature, MythicRun& run)
{
    // Each chosen spawn brings its copy once, even if its grid loads again
    if (!creature->GetSpawnId() || !run.teeming.erase(creature->GetSpawnId()))
        return;
    // Summoning while the grid is still loading the original isn't safe; do it on its first update
    creature->m_Events.AddEventAtOffset([creature]() { SummonCopy(creature); }, 1ms);
}

void BringTeemingCopies(Map* map, MythicRun& run)
{
    for (auto const& [spawnId, creature] : map->GetCreatureBySpawnIdStore())
        BringTeemingCopy(creature, run);
}
}

using namespace MythicPlus;

class MythicTeemingCreatureScript : public AllCreatureScript
{
public:
    MythicTeemingCreatureScript() : AllCreatureScript("MythicTeemingCreatureScript") { }

    void OnCreatureAddWorld(Creature* creature) override
    {
        MythicRun* run = FindRun(creature->GetMap());
        if (run && (run->state == RunState::Countdown || run->Active()))
            BringTeemingCopy(creature, *run);
    }
};

class MythicTeemingUnitScript : public UnitScript
{
public:
    MythicTeemingUnitScript() : UnitScript("MythicTeemingUnitScript", true, {UNITHOOK_ON_UNIT_ENTER_COMBAT}) { }

    void OnUnitEnterCombat(Unit* unit, Unit* victim) override
    {
        Creature* creature = unit->ToCreature();
        TeemingPartner const* link = creature ? creature->CustomData.Get<TeemingPartner>(PartnerKey) : nullptr;
        if (!link || !victim)
            return;
        Creature* partner = ObjectAccessor::GetCreature(*creature, link->partner);
        if (partner && partner->IsAlive() && !partner->IsInCombat() && partner->AI())
            partner->AI()->AttackStart(victim);
    }
};

void AddSC_mythic_affix_teeming()
{
    new MythicTeemingCreatureScript();
    new MythicTeemingUnitScript();
}
