/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_AFFIX_POLICY_H
#define _MYTHIC_AFFIX_POLICY_H

#include <array>
#include <cstdint>

// Mythic+ affixes (solo-wow): Legion's unlock levels (+4, +7, +10 with Fortified or Tyrannical) and a fixed weekly
// rotation. Skittish is left out because it breaks bot tanks.
namespace MythicPlus
{
enum class Affix : uint8_t
{
    None = 0,
    Fortified = 1,
    Tyrannical = 2,
    // +4
    Bolstering = 3,
    Bursting = 4,
    Raging = 5,
    Sanguine = 6,
    Teeming = 7,
    // +7
    Explosive = 8,
    Grievous = 9,
    Necrotic = 10,
    Quaking = 11,
    Volcanic = 12
};

inline constexpr uint8_t AffixCount = 13;

struct WeekAffixes
{
    Affix plus4;
    Affix plus7;
    Affix plus10;
};

// Ten weeks; every +4 and +7 affix meets a different partner each time round, and Fortified and Tyrannical alternate.
inline constexpr std::array<WeekAffixes, 10> Rotation = {{
    {Affix::Raging, Affix::Volcanic, Affix::Tyrannical},
    {Affix::Teeming, Affix::Explosive, Affix::Fortified},
    {Affix::Bolstering, Affix::Grievous, Affix::Tyrannical},
    {Affix::Sanguine, Affix::Necrotic, Affix::Fortified},
    {Affix::Bursting, Affix::Quaking, Affix::Tyrannical},
    {Affix::Raging, Affix::Explosive, Affix::Fortified},
    {Affix::Teeming, Affix::Necrotic, Affix::Tyrannical},
    {Affix::Bolstering, Affix::Volcanic, Affix::Fortified},
    {Affix::Sanguine, Affix::Quaking, Affix::Tyrannical},
    {Affix::Bursting, Affix::Grievous, Affix::Fortified},
}};

// The weekly reset: Wednesday 06:00 UTC, counted from 2025-01-01 (a Wednesday)
inline constexpr uint64_t WeekEpoch = 1735711200;
inline constexpr uint64_t WeekSeconds = 7 * 24 * 3600;

constexpr uint32_t WeekIndex(uint64_t unixTime)
{
    return unixTime < WeekEpoch ? 0 : uint32_t((unixTime - WeekEpoch) / WeekSeconds);
}

// Start of the week that `unixTime` falls in
constexpr uint64_t WeekStart(uint64_t unixTime)
{
    return WeekEpoch + uint64_t(WeekIndex(unixTime)) * WeekSeconds;
}

constexpr WeekAffixes AffixesForWeek(uint32_t weekIndex)
{
    return Rotation[weekIndex % Rotation.size()];
}

// Whether `affix` is on at `level` this week
constexpr bool IsActive(WeekAffixes const& week, uint8_t level, Affix affix)
{
    return affix != Affix::None && ((level >= 4 && week.plus4 == affix) || (level >= 7 && week.plus7 == affix) ||
                                    (level >= 10 && week.plus10 == affix));
}

constexpr char const* AffixName(Affix affix)
{
    switch (affix)
    {
        case Affix::Fortified: return "Fortified";
        case Affix::Tyrannical: return "Tyrannical";
        case Affix::Bolstering: return "Bolstering";
        case Affix::Bursting: return "Bursting";
        case Affix::Raging: return "Raging";
        case Affix::Sanguine: return "Sanguine";
        case Affix::Teeming: return "Teeming";
        case Affix::Explosive: return "Explosive";
        case Affix::Grievous: return "Grievous";
        case Affix::Necrotic: return "Necrotic";
        case Affix::Quaking: return "Quaking";
        case Affix::Volcanic: return "Volcanic";
        default: return "";
    }
}

// Raging: non-boss enemies enrage below this health
inline constexpr uint8_t RagingHealthPercent = 30;
// Grievous: players below this health take a stacking wound until healed above it
inline constexpr uint8_t GrievousHealthPercent = 90;
inline constexpr uint32_t GrievousTickMs = 3000;
// Teeming: extra copies of this share of the trash, which also raises the forces needed
inline constexpr uint8_t TeemingPercent = 20;
// Volcanic: plumes under players this far from every enemy they fight
inline constexpr float VolcanicMinDistance = 10.0f;
inline constexpr uint32_t VolcanicIntervalMs = 7000;
// Explosive: in trash combat, one orb per interval near a random fighting enemy
inline constexpr uint32_t ExplosiveIntervalMs = 9000;
// Quaking: every player quakes on this cycle, hurting and interrupting allies near them
inline constexpr uint32_t QuakingIntervalMs = 20000;
inline constexpr float QuakingRadius = 5.0f;
// Bursting and Bolstering reach this far from the dying enemy
inline constexpr float DeathEffectRadius = 30.0f;
}

#endif
