/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_DUNGEON_TABLE_H
#define _MYTHIC_DUNGEON_TABLE_H

#include <array>
#include <cstdint>

#include "MythicMechanicPolicy.h"

// Mythic+ dungeons (solo-wow): the sixteen WotLK 5-mans, their timers and entrances, and one extra mechanic per boss.
// Entrances are areatrigger_teleport's targets; boss entries are the normal-mode ones (Creature::GetEntry keeps them
// on heroic).
namespace MythicPlus
{
struct DungeonInfo
{
    uint16_t map;
    char const* name;
    uint32_t timerSeconds;
    uint8_t forcesPercent;
    // Encounters a run must finish, leaving out optional ones (Amanitar, Eck, the Infinite Corruptor)
    uint8_t requiredBosses;
    float x;
    float y;
    float z;
    float o;
};

// Forces 0: the trash comes in waves or events (Violet Hold, Culling, the joust, Halls of Reflection)
inline constexpr std::array<DungeonInfo, 16> Dungeons = {{
    {574, "Utgarde Keep", 1800, 75, 3, 153.789f, -86.548f, 12.551f, 0.304f},
    {575, "Utgarde Pinnacle", 1980, 75, 4, 584.117f, -327.974f, 110.138f, 3.122f},
    {576, "The Nexus", 1980, 75, 5, 145.87f, -10.554f, -16.636f, 1.528f},
    {578, "The Oculus", 2400, 60, 4, 1055.93f, 986.85f, 361.07f, 5.745f},
    {595, "The Culling of Stratholme", 2400, 0, 4, 1431.1f, 556.92f, 36.69f, 5.16f},
    {599, "Halls of Stone", 2280, 75, 4, 1153.24f, 806.164f, 195.937f, 4.715f},
    {600, "Drak'Tharon Keep", 1800, 75, 4, -517.343f, -487.976f, 11.01f, 4.831f},
    {601, "Azjol-Nerub", 1500, 75, 3, 413.314f, 795.968f, 831.351f, 5.5f},
    {602, "Halls of Lightning", 1980, 75, 4, 1331.47f, 259.619f, 53.398f, 4.772f},
    {604, "Gundrak", 1980, 75, 4, 1891.84f, 832.169f, 176.669f, 2.109f},
    {608, "The Violet Hold", 1800, 0, 3, 1808.82f, 803.93f, 44.364f, 6.282f},
    {619, "Ahn'kahet: The Old Kingdom", 1980, 75, 4, 333.351f, -1109.94f, 69.772f, 0.553f},
    {632, "The Forge of Souls", 1500, 75, 2, 4922.86f, 2175.63f, 638.734f, 2.00355f},
    {650, "Trial of the Champion", 1800, 0, 3, 805.227f, 618.038f, 412.393f, 3.1456f},
    {658, "Pit of Saron", 1980, 75, 3, 435.743f, 212.413f, 528.709f, 6.25646f},
    {668, "Halls of Reflection", 1980, 0, 3, 5239.01f, 1932.64f, 707.695f, 0.800565f},
}};

constexpr DungeonInfo const* FindDungeon(uint32_t map)
{
    for (DungeonInfo const& dungeon : Dungeons)
        if (dungeon.map == map)
            return &dungeon;
    return nullptr;
}

struct BossMechanic
{
    uint32_t entry;
    uint16_t map;
    Pattern pattern;
    uint32_t firstMs;
    uint32_t intervalMs;
    // Raid warning shown when it starts; "%s" is the boss's name
    char const* warning;
};

inline constexpr std::array<BossMechanic, 64> BossMechanics = {{
    // Utgarde Keep
    {23953, 574, Pattern::Circles, 8000, 16000, "%s scatters frost shards!"},
    {24200, 574, Pattern::Line, 10000, 18000, "%s charges a shockwave!"},
    {24201, 574, Pattern::Pool, 12000, 20000, "%s pools shadow beneath you!"},
    {23954, 574, Pattern::Ring, 12000, 22000, "%s rings himself with dark fire!"},
    // Utgarde Pinnacle
    {26668, 575, Pattern::Circles, 8000, 16000, "%s calls down ritual flames!"},
    {26687, 575, Pattern::Nova, 10000, 18000, "%s rears up to stomp!"},
    {26693, 575, Pattern::Circles, 10000, 16000, "%s hurls burning harpoons!"},
    {26861, 575, Pattern::Pool, 12000, 20000, "%s spreads the bane of the Vrykul!"},
    // The Nexus
    {26731, 576, Pattern::Circles, 8000, 15000, "%s unleashes arcane bursts!"},
    {26763, 576, Pattern::Orbs, 12000, 25000, "%s tears open unstable orbs!"},
    {26794, 576, Pattern::Line, 10000, 17000, "%s drives a line of crystal spikes!"},
    {26723, 576, Pattern::Ring, 12000, 20000, "%s rings herself with frost!"},
    {26796, 576, Pattern::Nova, 10000, 16000, "%s whirls into a shockwave!"},
    {26798, 576, Pattern::Nova, 10000, 16000, "%s whirls into a shockwave!"},
    // The Oculus: the drake fights get Empower, the ground fights get circles
    {27654, 578, Pattern::Ring, 10000, 20000, "%s rings himself with lightning!"},
    {27447, 578, Pattern::Empower, 30000, 30000, "%s draws power from the ley lines!"},
    {27655, 578, Pattern::Circles, 8000, 15000, "%s conjures arcane bursts!"},
    {27656, 578, Pattern::Empower, 30000, 30000, "%s draws power from the ley lines!"},
    // Azjol-Nerub
    {28684, 601, Pattern::Adds, 12000, 25000, "%s calls more swarmers!"},
    {28921, 601, Pattern::Pool, 10000, 18000, "%s spits webbing acid!"},
    {29120, 601, Pattern::Line, 10000, 18000, "%s drives a line of spikes!"},
    // Ahn'kahet
    {29309, 619, Pattern::Adds, 12000, 25000, "%s's brood rushes in!"},
    {29308, 619, Pattern::Orbs, 15000, 25000, "%s conjures unstable flames!"},
    {29310, 619, Pattern::Ring, 12000, 22000, "%s rings herself with twilight!"},
    {30258, 619, Pattern::Pool, 10000, 18000, "%s releases toxic spores!"},
    {29311, 619, Pattern::Circles, 8000, 15000, "%s's madness bursts around you!"},
    // Drak'Tharon Keep
    {26630, 600, Pattern::Adds, 12000, 25000, "%s calls invaders!"},
    {26631, 600, Pattern::Circles, 8000, 15000, "%s rains arcane bursts!"},
    {27483, 600, Pattern::Nova, 10000, 16000, "%s rears up to stomp!"},
    {26632, 600, Pattern::Line, 10000, 18000, "%s drives a line of bone spikes!"},
    // The Violet Hold
    {29315, 608, Pattern::Circles, 8000, 15000, "%s calls lightning!"},
    {29316, 608, Pattern::Pool, 10000, 18000, "%s spits corrosive saliva!"},
    {29313, 608, Pattern::Ring, 12000, 22000, "%s rings himself with water!"},
    {29266, 608, Pattern::Orbs, 12000, 25000, "%s summons ethereal spheres!"},
    {29312, 608, Pattern::Line, 10000, 17000, "%s spews a line of lava!"},
    {29314, 608, Pattern::Pool, 10000, 18000, "%s opens void zones!"},
    {31134, 608, Pattern::Ring, 12000, 22000, "%s rings herself with arcane!"},
    // Gundrak
    {29304, 604, Pattern::Adds, 12000, 25000, "%s summons snakes!"},
    {29307, 604, Pattern::Nova, 10000, 16000, "%s slams the ground!"},
    {29305, 604, Pattern::Line, 10000, 17000, "%s charges a mojo wave!"},
    {29306, 604, Pattern::Circles, 8000, 15000, "%s calls impaling spikes!"},
    {29932, 604, Pattern::Pool, 10000, 18000, "%s spits venom!"},
    // Halls of Stone (the Tribunal is an event with no boss to fight)
    {27977, 599, Pattern::Ring, 12000, 22000, "%s shatters the ground around him!"},
    {27975, 599, Pattern::Pool, 10000, 18000, "%s weeps pools of grief!"},
    {27978, 599, Pattern::Circles, 8000, 15000, "%s forges molten bursts!"},
    // Halls of Lightning
    {28586, 602, Pattern::Line, 10000, 17000, "%s charges a line of steel!"},
    {28587, 602, Pattern::Circles, 8000, 15000, "%s flings molten slag!"},
    {28546, 602, Pattern::Ring, 12000, 22000, "%s rings himself with storm!"},
    {28923, 602, Pattern::Circles, 8000, 15000, "%s calls lightning!"},
    // The Culling of Stratholme
    {26529, 595, Pattern::Pool, 10000, 18000, "%s spews plague!"},
    {26530, 595, Pattern::Adds, 12000, 25000, "%s raises more ghouls!"},
    {26532, 595, Pattern::Ring, 12000, 22000, "%s rings himself with time!"},
    {26533, 595, Pattern::Circles, 8000, 15000, "%s rains shadow!"},
    {32273, 595, Pattern::Pool, 10000, 18000, "%s spills corrupted time!"},
    // Trial of the Champion: the grand champions on foot, then the Argent fight and the Black Knight
    {35119, 650, Pattern::Ring, 12000, 22000, "%s rings himself with holy hammers!"},
    {34928, 650, Pattern::Pool, 10000, 18000, "%s consecrates the ground!"},
    {35451, 650, Pattern::Adds, 15000, 30000, "%s raises the fallen!"},
    // The Forge of Souls
    {36497, 632, Pattern::Orbs, 12000, 25000, "%s tears loose unstable souls!"},
    {36502, 632, Pattern::Line, 10000, 17000, "%s lets out a wailing line!"},
    // Pit of Saron
    {36494, 658, Pattern::Circles, 8000, 15000, "%s hurls saronite!"},
    {36476, 658, Pattern::Pool, 10000, 18000, "%s spews toxic waste!"},
    {36658, 658, Pattern::Line, 10000, 17000, "%s calls Rimefang's frost line!"},
    // Halls of Reflection
    {38112, 668, Pattern::Ring, 12000, 22000, "%s rings himself with despair!"},
    {38113, 668, Pattern::Pool, 10000, 18000, "%s spreads corrupted flesh!"},
}};

// The ten grand champions of Trial of the Champion share one mechanic
inline constexpr std::array<uint32_t, 10> GrandChampions = {
    34705, 34702, 34701, 34657, 34703, 35572, 35569, 35571, 35570, 35617};
inline constexpr BossMechanic GrandChampionMechanic = {0, 650, Pattern::Circles, 10000, 18000,
                                                       "%s calls a hail of blows!"};

constexpr BossMechanic const* FindBossMechanic(uint32_t entry)
{
    for (BossMechanic const& mechanic : BossMechanics)
        if (mechanic.entry == entry)
            return &mechanic;
    for (uint32_t champion : GrandChampions)
        if (champion == entry)
            return &GrandChampionMechanic;
    return nullptr;
}
}

#endif
