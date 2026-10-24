/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <algorithm>
#include <limits>

#include "Containers.h"
#include "Creature.h"
#include "Map.h"
#include "MythicMechanicPolicy.h"
#include "MythicPlus.h"
#include "Player.h"
#include "ScriptMgr.h"

// Volcanic: in combat, plumes erupt under players standing well away from every enemy they fight, three seconds
// after the ground glows, so ranged keep moving.
using namespace MythicPlus;

namespace
{
constexpr float SearchRange = 60.0f;

struct VolcanicTimer : public DataMap::Base
{
    uint32 ms = 0;
};
}

class MythicVolcanicMapScript : public AllMapScript
{
public:
    MythicVolcanicMapScript() : AllMapScript("MythicVolcanicMapScript", {ALLMAPHOOK_ON_MAP_UPDATE}) { }

    void OnMapUpdate(Map* map, uint32 diff) override
    {
        MythicRun* run = ActiveRun(map);
        if (!run || !run->Has(Affix::Volcanic))
            return;
        VolcanicTimer* timer = map->CustomData.GetDefault<VolcanicTimer>("MythicPlusVolcanic");
        timer->ms += diff;
        if (timer->ms < VolcanicIntervalMs)
            return;
        timer->ms = 0;

        std::vector<std::pair<Player*, Creature*>> distant;
        for (Player* player : RunPlayers(map, true))
        {
            Creature* nearest = nullptr;
            float nearestDistance = std::numeric_limits<float>::max();
            for (Creature* enemy : EnemiesNear(player, SearchRange))
            {
                if (!enemy->IsInCombat())
                    continue;
                float const distance = player->GetDistance(enemy);
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearest = enemy;
                }
            }
            if (nearest && nearestDistance >= VolcanicMinDistance)
                distant.emplace_back(player, nearest);
        }
        if (distant.empty())
            return;
        Acore::Containers::RandomShuffle(distant);
        distant.resize(TargetCount(run->level, uint8(std::min<size_t>(distant.size(), 255))));
        for (auto const& [player, source] : distant)
            CastGround(source, player->GetPosition(), SpellVolcanicPlume, ScaledDamage(VolcanicDamage, run->level),
                       CircleMs);
    }
};

void AddSC_mythic_affix_volcanic()
{
    new MythicVolcanicMapScript();
}
