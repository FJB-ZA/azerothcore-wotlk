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
#include "Creature.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "MythicPlus.h"
#include "Player.h"
#include "TemporarySummon.h"

// Mythic+ ground effects: an invisible rune stands where a circle lands and casts its spell, so the circle stays put
// when its source moves or dies.
namespace MythicPlus
{
Creature* CastGround(Unit* source, Position const& pos, uint32 spellId, uint32 damage, uint32 lifeMs)
{
    float z = pos.GetPositionZ();
    float const ground = source->GetMapHeight(pos.GetPositionX(), pos.GetPositionY(), z + 2.0f);
    if (ground > INVALID_HEIGHT)
        z = ground;
    TempSummon* rune = source->SummonCreature(RuneEntry, pos.GetPositionX(), pos.GetPositionY(), z, 0.0f,
                                              TEMPSUMMON_TIMED_DESPAWN, lifeMs + 1000);
    if (!rune)
        return nullptr;
    // A player's quake keeps the rune's hostile faction; an enemy's circle takes its side, so Sanguine heals its allies
    if (source->IsCreature())
        rune->SetFaction(source->GetFaction());
    // The spells roll one die side on top of their base points
    int32 const basePoints = int32(std::max<uint32>(damage, 1)) - 1;
    rune->CastCustomSpell(spellId, SPELLVALUE_BASE_POINT0, basePoints, rune, TRIGGERED_FULL_MASK);
    return rune;
}

std::vector<Creature*> EnemiesNear(WorldObject* center, float range)
{
    std::list<Creature*> found;
    Acore::AllWorldObjectsInRange check(center, range);
    Acore::CreatureListSearcher<Acore::AllWorldObjectsInRange> searcher(center, found, check);
    Cell::VisitObjects(center, searcher, range);
    std::vector<Creature*> enemies;
    for (Creature* creature : found)
        if (creature != center && IsMythicEnemy(creature))
            enemies.push_back(creature);
    return enemies;
}

std::vector<Player*> RunPlayers(Map* map, bool inCombatOnly)
{
    std::vector<Player*> players;
    map->DoForAllPlayers([&](Player* player)
    {
        if (player->IsAlive() && !player->IsGameMaster() && (!inCombatOnly || player->IsInCombat()))
            players.push_back(player);
    });
    return players;
}
}
