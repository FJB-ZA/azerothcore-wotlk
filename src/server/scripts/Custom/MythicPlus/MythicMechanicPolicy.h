/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_MECHANIC_POLICY_H
#define _MYTHIC_MECHANIC_POLICY_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Mythic+ boss mechanics (solo-wow): where the ground circles go and how hard they hit. Every circle is a persistent
// area spell that shows its ground visual first and hurts once its first tick lands, so players and bots
// (AvoidAoeAction flees dynamic objects) can step out in time.
namespace MythicPlus
{
enum class Pattern : uint8_t
{
    None = 0,
    // Circles under random players
    Circles = 1,
    // A ring of circles around the boss with one gap; under the boss is safe
    Ring = 2,
    // A row of circles from the boss through a random player
    Line = 3,
    // A lingering pool under a random player
    Pool = 4,
    // Mythic servants join the fight
    Adds = 5,
    // Explosive orbs that hit the whole group unless killed
    Orbs = 6,
    // One wide circle on the boss: melee must step out
    Nova = 7,
    // The boss grows stronger every cast (for fights on drakes, where ground circles can't reach)
    Empower = 8
};

struct Point
{
    float x;
    float y;
};

inline constexpr float BurstRadius = 6.0f;
inline constexpr float NovaRadius = 10.0f;
inline constexpr float RingDistance = 10.0f;
inline constexpr uint8_t RingSlots = 10;
inline constexpr float LineSpacing = 5.5f;
inline constexpr uint8_t LineLength = 7;
inline constexpr float TwoPi = 6.28318530718f;

// More targets as keys climb: one, a second from +7, a third from +12
constexpr uint8_t TargetCount(uint8_t level, uint8_t players)
{
    uint8_t const wanted = uint8_t(1 + (level >= 7) + (level >= 12));
    return std::min<uint8_t>(wanted, players);
}

// Mechanics come a little faster at higher keys, down to 70% of the interval at +17
constexpr uint32_t ScaledInterval(uint32_t intervalMs, uint8_t level)
{
    uint32_t const steps = level > 2 ? std::min<uint32_t>(level - 2, 15) : 0;
    return intervalMs * (100 - 2 * steps) / 100;
}

// Damage that grows with the key like the enemies do, from a value tuned for +2
inline uint32_t ScaledDamage(uint32_t atPlusTwo, uint8_t level)
{
    double const steps = double(std::max<int>(0, int(level) - 2));
    return uint32_t(std::lround(double(atPlusTwo) * std::pow(1.10, steps)));
}

// Base damage at +2: a burst takes a fresh 232-geared player's quarter of health
inline constexpr uint32_t BurstDamage = 7000;
inline constexpr uint32_t NovaDamage = 9000;
inline constexpr uint32_t PoolTickDamage = 2500;
inline constexpr uint32_t OrbExplosionDamage = 8000;

// Circles around `center`, leaving out the slot at `gap` so there is a way through
inline std::vector<Point> RingPoints(Point center, float distance, uint8_t slots, uint8_t gap, float startAngle)
{
    std::vector<Point> points;
    if (!slots)
        return points;
    for (uint8_t i = 0; i < slots; ++i)
    {
        if (i == gap % slots)
            continue;
        float const angle = startAngle + TwoPi * float(i) / float(slots);
        points.push_back({center.x + distance * std::cos(angle), center.y + distance * std::sin(angle)});
    }
    return points;
}

// Circles from just past `from` towards `through`, carrying on past it
inline std::vector<Point> LinePoints(Point from, Point through, float spacing, uint8_t count)
{
    std::vector<Point> points;
    float const dx = through.x - from.x;
    float const dy = through.y - from.y;
    float const length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.01f || !count)
        return points;
    for (uint8_t i = 1; i <= count; ++i)
        points.push_back({from.x + dx / length * spacing * float(i), from.y + dy / length * spacing * float(i)});
    return points;
}
}

#endif
