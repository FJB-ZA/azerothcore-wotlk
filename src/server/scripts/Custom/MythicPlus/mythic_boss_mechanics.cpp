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
#include <string>

#include "Containers.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Map.h"
#include "MythicMechanicPolicy.h"
#include "MythicPlus.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"

// Mythic+ boss mechanics: during a run every WotLK dungeon boss adds one mechanic from MythicDungeonTable.h on its
// own timer, on top of its normal fight. It warns with a boss emote first.
namespace
{
using namespace MythicPlus;

constexpr char const* BossKey = "MythicPlusBoss";
constexpr float TargetRange = 60.0f;
constexpr float AddDistance = 5.0f;
constexpr float OrbDistance = 8.0f;
constexpr uint8 MaxSummons = 3;
constexpr uint32 AddCorpseMs = 15000;

struct BossMechanicState : public DataMap::Base
{
    BossMechanic const* mechanic = nullptr;
    uint32 timerMs = 0;
    bool engaged = false;
};

std::vector<Player*> Targets(Creature* boss, uint8 level)
{
    std::vector<Player*> players;
    for (Player* player : RunPlayers(boss->GetMap(), true))
        if (boss->IsWithinDistInMap(player, TargetRange))
            players.push_back(player);
    Acore::Containers::RandomShuffle(players);
    players.resize(TargetCount(level, uint8(std::min<size_t>(players.size(), 255))));
    return players;
}

void Warn(Creature* boss, BossMechanic const& mechanic)
{
    std::string text = mechanic.warning;
    if (std::size_t const at = text.find("%s"); at != std::string::npos)
        text.replace(at, 2, boss->GetName());
    boss->TextEmote(text, nullptr, true);
}

Position At(Creature* boss, Point point)
{
    return Position(point.x, point.y, boss->GetPositionZ());
}

Position Around(Creature* boss, float distance, uint8 index, uint8 count)
{
    float const angle = boss->GetOrientation() + TwoPi * float(index) / float(count);
    return Position(boss->GetPositionX() + distance * std::cos(angle),
                    boss->GetPositionY() + distance * std::sin(angle), boss->GetPositionZ(), angle);
}

void Perform(Creature* boss, uint8 level, BossMechanic const& mechanic)
{
    Point const center{boss->GetPositionX(), boss->GetPositionY()};
    uint32 const burst = ScaledDamage(BurstDamage, level);
    switch (mechanic.pattern)
    {
        case Pattern::Circles:
            for (Player* player : Targets(boss, level))
                CastGround(boss, player->GetPosition(), SpellShatteringGround, burst, CircleMs);
            break;
        case Pattern::Ring:
            for (Point point : RingPoints(center, RingDistance, RingSlots, uint8(urand(0, RingSlots - 1)),
                                          frand(0.0f, TwoPi)))
                CastGround(boss, At(boss, point), SpellShatteringGround, burst, CircleMs);
            break;
        case Pattern::Line:
            for (Player* player : Targets(boss, 1))
                for (Point point : LinePoints(center, {player->GetPositionX(), player->GetPositionY()}, LineSpacing,
                                              LineLength))
                    CastGround(boss, At(boss, point), SpellShatteringGround, burst, CircleMs);
            break;
        case Pattern::Pool:
            for (Player* player : Targets(boss, level))
                CastGround(boss, player->GetPosition(), SpellCreepingShadow, ScaledDamage(PoolTickDamage, level),
                           PoolMs);
            break;
        case Pattern::Adds:
        {
            uint8 const count = TargetCount(level, MaxSummons);
            std::vector<Player*> const targets = RunPlayers(boss->GetMap(), true);
            for (uint8 i = 0; i < count; ++i)
            {
                Position const pos = Around(boss, AddDistance, i, count);
                TempSummon* servant = boss->SummonCreature(ServantEntry, pos.GetPositionX(), pos.GetPositionY(),
                                                           pos.GetPositionZ(), pos.GetOrientation(),
                                                           TEMPSUMMON_CORPSE_TIMED_DESPAWN, AddCorpseMs);
                if (!servant)
                    continue;
                servant->SetFaction(boss->GetFaction());
                if (!targets.empty() && servant->AI())
                    servant->AI()->AttackStart(targets[i % targets.size()]);
            }
            break;
        }
        case Pattern::Orbs:
        {
            uint8 const count = TargetCount(level, MaxSummons);
            for (uint8 i = 0; i < count; ++i)
                SpawnOrb(boss, Around(boss, OrbDistance, i, count));
            break;
        }
        case Pattern::Nova:
            CastGround(boss, boss->GetPosition(), SpellShadowNova, ScaledDamage(NovaDamage, level), CircleMs);
            break;
        case Pattern::Empower:
            boss->AddAura(SpellEmpowered, boss);
            break;
        default:
            break;
    }
}
}

class MythicBossMechanicScript : public AllCreatureScript
{
public:
    MythicBossMechanicScript() : AllCreatureScript("MythicBossMechanicScript") { }

    void OnAllCreatureUpdate(Creature* creature, uint32 diff) override
    {
        if (!creature->IsInCombat() || !creature->IsAlive())
            return;
        BossMechanicState* state = creature->CustomData.Get<BossMechanicState>(BossKey);
        if (!state)
        {
            if (!creature->GetMap()->IsDungeon() || !ActiveRun(creature->GetMap()))
                return;
            // Every creature that fights in a run is looked up once; most have no mechanic
            state = creature->CustomData.GetDefault<BossMechanicState>(BossKey);
            state->mechanic = FindBossMechanic(creature->GetEntry());
        }
        MythicRun* run = state->mechanic ? ActiveRun(creature->GetMap()) : nullptr;
        if (!run)
            return;
        if (!state->engaged)
        {
            state->engaged = true;
            state->timerMs = ScaledInterval(state->mechanic->firstMs, run->level);
            return;
        }
        if (state->timerMs > diff)
        {
            state->timerMs -= diff;
            return;
        }
        state->timerMs = ScaledInterval(state->mechanic->intervalMs, run->level);
        Warn(creature, *state->mechanic);
        Perform(creature, run->level, *state->mechanic);
    }
};

class MythicBossMechanicUnitScript : public UnitScript
{
public:
    MythicBossMechanicUnitScript()
        : UnitScript("MythicBossMechanicUnitScript", true, {UNITHOOK_ON_UNIT_ENTER_EVADE_MODE})
    {
    }

    // A reset fight starts its mechanic timer over and drops its empowerment
    void OnUnitEnterEvadeMode(Unit* unit, uint8 /*evadeReason*/) override
    {
        Creature* creature = unit->ToCreature();
        BossMechanicState* state = creature ? creature->CustomData.Get<BossMechanicState>(BossKey) : nullptr;
        if (!state || !state->mechanic)
            return;
        state->engaged = false;
        creature->RemoveAurasDueToSpell(SpellEmpowered);
    }
};

void AddSC_mythic_boss_mechanics()
{
    new MythicBossMechanicScript();
    new MythicBossMechanicUnitScript();
}
