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
#include <numbers>

#include "Containers.h"
#include "Creature.h"
#include "Map.h"
#include "MythicMechanicPolicy.h"
#include "MythicPlus.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"

// Explosive: in combat, enemies call explosive orbs that swell for six seconds and then hit every player, so someone
// has to kill each one. Boss mechanics with the Orbs pattern use the same orb.
namespace MythicPlus
{
namespace
{
constexpr float SearchRange = 40.0f;
constexpr float ExplosionRange = 60.0f;
constexpr uint32 GrowSteps = 3;

struct ExplosiveTimer : public DataMap::Base
{
    uint32 ms = 0;
};
}

Creature* SpawnOrb(Unit* source, Position const& pos)
{
    TempSummon* orb = source->SummonCreature(ExplosiveOrbEntry, pos.GetPositionX(), pos.GetPositionY(),
                                             pos.GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN,
                                             ExplosiveFuseMs + 5000);
    // On the caller's side, so its allies leave the orb alone
    if (orb && source->IsCreature())
        orb->SetFaction(source->GetFaction());
    return orb;
}
}

using namespace MythicPlus;

struct npc_mythic_explosive_orb : public ScriptedAI
{
    npc_mythic_explosive_orb(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetControlled(true, UNIT_STATE_ROOT);
    }

    void Reset() override { }

    void AttackStart(Unit* /*victim*/) override { }

    void UpdateAI(uint32 diff) override
    {
        if (_exploded)
            return;
        _elapsedMs += diff;
        uint32 const step = std::min(GrowSteps, _elapsedMs * GrowSteps / ExplosiveFuseMs);
        if (step != _grown)
        {
            _grown = step;
            me->SetObjectScale(1.0f + 0.5f * float(step));
        }
        if (_elapsedMs >= ExplosiveFuseMs)
            Explode();
    }

private:
    void Explode()
    {
        _exploded = true;
        MythicRun* run = FindRun(me->GetMap());
        int32 const basePoints = int32(ScaledDamage(OrbExplosionDamage, run ? run->level : MinKeyLevel)) - 1;
        for (Player* player : RunPlayers(me->GetMap(), false))
            if (me->IsWithinDistInMap(player, ExplosionRange))
                me->CastCustomSpell(SpellExplosion, SPELLVALUE_BASE_POINT0, basePoints, player, TRIGGERED_FULL_MASK);
        me->DespawnOrUnsummon(500ms);
    }

    uint32 _elapsedMs = 0;
    uint32 _grown = 0;
    bool _exploded = false;
};

class MythicExplosiveMapScript : public AllMapScript
{
public:
    MythicExplosiveMapScript() : AllMapScript("MythicExplosiveMapScript", {ALLMAPHOOK_ON_MAP_UPDATE}) { }

    void OnMapUpdate(Map* map, uint32 diff) override
    {
        MythicRun* run = ActiveRun(map);
        if (!run || !run->Has(Affix::Explosive))
            return;
        ExplosiveTimer* timer = map->CustomData.GetDefault<ExplosiveTimer>("MythicPlusExplosive");
        timer->ms += diff;
        if (timer->ms < ExplosiveIntervalMs)
            return;
        timer->ms = 0;

        std::vector<Creature*> fighting;
        for (Player* player : RunPlayers(map, true))
            for (Creature* enemy : EnemiesNear(player, SearchRange))
                if (enemy->IsInCombat())
                    fighting.push_back(enemy);
        if (fighting.empty())
            return;
        Creature* caller = Acore::Containers::SelectRandomContainerElement(fighting);
        float const angle = frand(0.0f, 2.0f * std::numbers::pi_v<float>);
        float const distance = frand(3.0f, 8.0f);
        Position const pos(caller->GetPositionX() + distance * std::cos(angle),
                           caller->GetPositionY() + distance * std::sin(angle), caller->GetPositionZ());
        SpawnOrb(caller, pos);
    }
};

void AddSC_mythic_affix_explosive()
{
    RegisterCreatureAI(npc_mythic_explosive_orb);
    new MythicExplosiveMapScript();
}
