/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_CREST_POLICY_H
#define _MYTHIC_CREST_POLICY_H

#include <algorithm>
#include <array>
#include <cstdint>

// Mythic+ crests (solo-wow): four tiers earned by key level. Each tier upgrades items through its own four ranks,
// and a higher tier also pays for a lower rank, so better crests are worth more.
namespace MythicPlus
{
enum class Crest : uint8_t
{
    None = 0,
    Cobalt = 1,
    Saronite = 2,
    Titanium = 3,
    Titansteel = 4
};

inline constexpr uint8_t CrestTiers = 4;
inline constexpr uint32_t CrestsPerRun = 10;
inline constexpr uint32_t CrestsPerTimedRun = 15;
inline constexpr uint32_t CrestsPerRank = 15;
// Crests of each tier a player can earn per week
inline constexpr uint32_t WeeklyCrestCap = 90;
inline constexpr uint8_t RanksPerTier = 4;

constexpr Crest CrestForKeyLevel(uint8_t level)
{
    if (level >= 15)
        return Crest::Titansteel;
    if (level >= 10)
        return Crest::Titanium;
    if (level >= 5)
        return Crest::Saronite;
    if (level >= 2)
        return Crest::Cobalt;
    return Crest::None;
}

constexpr uint32_t CrestsForRun(bool timed)
{
    return timed ? CrestsPerTimedRun : CrestsPerRun;
}

// How many crests a run adds, given what the player already earned of that tier this week
constexpr uint32_t CappedAward(uint32_t award, uint32_t earnedThisWeek)
{
    return earnedThisWeek >= WeeklyCrestCap ? 0 : std::min(award, WeeklyCrestCap - earnedThisWeek);
}

// The lowest crest tier that pays for upgrading an item to `rank` (1-16)
constexpr Crest CrestForRank(uint8_t rank)
{
    if (!rank)
        return Crest::None;
    return Crest(std::min<uint8_t>(CrestTiers, uint8_t((rank - 1) / RanksPerTier + 1)));
}

constexpr char const* CrestName(Crest crest)
{
    switch (crest)
    {
        case Crest::Cobalt: return "Cobalt";
        case Crest::Saronite: return "Saronite";
        case Crest::Titanium: return "Titanium";
        case Crest::Titansteel: return "Titansteel";
        default: return "";
    }
}

// Crests owned per tier, index 0 = Cobalt
using CrestWallet = std::array<uint32_t, CrestTiers>;

// Pays CrestsPerRank for `rank` from the lowest tier that may pay, so better crests are kept for higher ranks.
// Returns the tier paid with, or None when no allowed tier holds enough.
constexpr Crest PayForRank(CrestWallet& wallet, uint8_t rank)
{
    Crest const lowest = CrestForRank(rank);
    if (lowest == Crest::None)
        return Crest::None;
    for (uint8_t tier = uint8_t(lowest); tier <= CrestTiers; ++tier)
    {
        if (wallet[tier - 1] >= CrestsPerRank)
        {
            wallet[tier - 1] -= CrestsPerRank;
            return Crest(tier);
        }
    }
    return Crest::None;
}

constexpr bool CanAfford(CrestWallet wallet, uint8_t rank)
{
    return PayForRank(wallet, rank) != Crest::None;
}
}

#endif
