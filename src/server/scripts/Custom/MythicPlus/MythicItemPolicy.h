/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_ITEM_POLICY_H
#define _MYTHIC_ITEM_POLICY_H

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

// Mythic+ item ranks (solo-wow). Every eligible dungeon item has 17 generated versions, Mythic 0 to Mythic 16, made
// from the real item by one rule, so nothing is copied into SQL. The client caches stats by item id, so each rank
// needs its own id: FirstRankId + index * RankCount + rank, where `index` is the item's fixed row in mythic_plus_item.
namespace MythicPlus
{
inline constexpr uint8_t MaxRank = 16;
inline constexpr uint8_t RankCount = MaxRank + 1;
inline constexpr uint32_t FirstRankId = 900000;
// Below the custom item range 990000-990999 the spell-data generator owns
inline constexpr uint32_t RankIdEnd = 990000;
inline constexpr uint32_t MaxItemIndex = (RankIdEnd - FirstRankId) / RankCount;
// Mythic 0 matches the Frozen Halls heroics; Mythic 16 matches ICC 25 normal, so heroic raid gear stays ahead
inline constexpr uint16_t RankZeroItemLevel = 232;
inline constexpr uint16_t ItemLevelPerRank = 2;

// Growth per item level, the median of every WotLK PvP season step of the same item (savage to wrathful)
inline constexpr double StatGrowth = 1.0099;
inline constexpr double ArmorGrowth = 1.0026;
inline constexpr double WeaponGrowth = 1.0074;

constexpr uint16_t ItemLevelForRank(uint8_t rank)
{
    return uint16_t(RankZeroItemLevel + ItemLevelPerRank * (rank > MaxRank ? MaxRank : rank));
}

constexpr bool IsRankId(uint32_t itemId)
{
    return itemId >= FirstRankId && itemId < FirstRankId + MaxItemIndex * RankCount;
}

constexpr uint32_t RankId(uint32_t index, uint8_t rank)
{
    return FirstRankId + index * RankCount + rank;
}

struct RankRef
{
    uint32_t index;
    uint8_t rank;
};

constexpr std::optional<RankRef> DecodeRankId(uint32_t itemId)
{
    if (!IsRankId(itemId))
        return std::nullopt;
    uint32_t const offset = itemId - FirstRankId;
    return RankRef{offset / RankCount, uint8_t(offset % RankCount)};
}

inline double Growth(double perLevel, int fromLevel, int toLevel)
{
    return std::pow(perLevel, double(toLevel - fromLevel));
}

// A stat, armor or damage value moved from the real item's level to the rank's, rounded to the nearest whole point
inline int32_t Rescale(int32_t value, double perLevel, int fromLevel, int toLevel)
{
    if (!value)
        return 0;
    double const scaled = double(value) * Growth(perLevel, fromLevel, toLevel);
    int32_t const rounded = int32_t(std::lround(scaled));
    // A real stat never rounds away
    return rounded ? rounded : (value > 0 ? 1 : -1);
}

inline float RescaleDamage(float value, int fromLevel, int toLevel)
{
    return value ? float(std::round(double(value) * Growth(WeaponGrowth, fromLevel, toLevel))) : 0.0f;
}

// The line the tooltip shows under the name
inline std::string RankDescription(uint8_t rank)
{
    return "Mythic " + std::to_string(rank);
}
}

#endif
