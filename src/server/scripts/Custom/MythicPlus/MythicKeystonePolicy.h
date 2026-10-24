/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_KEYSTONE_POLICY_H
#define _MYTHIC_KEYSTONE_POLICY_H

#include <algorithm>
#include <cmath>
#include <cstdint>

// Mythic+ keystone rules (solo-wow), after Legion 7.2.5: key levels, enemy scaling over heroic, the timer's +1/+2/+3,
// the death penalty, enemy forces and what the key becomes after a run.
namespace MythicPlus
{
inline constexpr uint8_t MinKeyLevel = 2;
inline constexpr uint8_t MaxKeyLevel = 30;
inline constexpr uint32_t CountdownMs = 10000;
inline constexpr uint32_t DeathPenaltyMs = 5000;
// Enemy forces needed, in percent of the dungeon's trash when the run starts
inline constexpr uint8_t DefaultForcesPercent = 80;

enum class RunState : uint8_t
{
    None = 0,
    Countdown = 1,
    Running = 2,
    Completed = 3,
    // The timer ran out: the run goes on for loot and crests, but the key drops a level
    Overtime = 4,
    Abandoned = 5
};

constexpr uint8_t ClampLevel(int level)
{
    return uint8_t(std::clamp(level, int(MinKeyLevel), int(MaxKeyLevel)));
}

// Health and damage over heroic: 10% per key level, compounding (Legion 7.2.5)
inline double ScalingMultiplier(uint8_t level)
{
    return std::pow(1.10, double(ClampLevel(level)));
}

// Fortified: trash only. Tyrannical: bosses only.
inline constexpr double FortifiedHealth = 1.20;
inline constexpr double FortifiedDamage = 1.30;
inline constexpr double TyrannicalHealth = 1.40;
inline constexpr double TyrannicalDamage = 1.15;

constexpr uint32_t PenaltyMs(uint32_t deaths)
{
    return deaths * DeathPenaltyMs;
}

// The timer's verdict: 0 out of time, 1 timed, 2 within 80% of the limit, 3 within 60%
constexpr uint8_t TimerUpgrade(uint64_t elapsedMs, uint64_t limitMs)
{
    if (!limitMs || elapsedMs > limitMs)
        return 0;
    if (elapsedMs * 10 <= limitMs * 6)
        return 3;
    if (elapsedMs * 10 <= limitMs * 8)
        return 2;
    return 1;
}

// A timed key goes up by its upgrade; any other finish, or leaving the key unfinished, takes one level off
constexpr uint8_t NextKeyLevel(uint8_t level, uint8_t upgrade)
{
    return upgrade ? ClampLevel(int(level) + upgrade) : ClampLevel(int(level) - 1);
}

constexpr uint32_t ForcesQuota(uint32_t totalForces, uint8_t percent)
{
    if (!totalForces || !percent)
        return 0;
    return std::max<uint32_t>(1, (totalForces * percent + 99) / 100);
}

constexpr bool IsComplete(uint8_t bossesKilled, uint8_t bossesTotal, uint32_t forces, uint32_t quota)
{
    return bossesTotal && bossesKilled >= bossesTotal && forces >= quota;
}

// Each player's chance of an item from the end chest: Legion's 2 items for the group (3 when timed) spread over five
constexpr uint8_t ChestItemChancePercent(bool timed)
{
    return timed ? 60 : 40;
}
}

#endif
