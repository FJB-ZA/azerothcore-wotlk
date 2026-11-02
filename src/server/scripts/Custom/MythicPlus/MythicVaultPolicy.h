/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_VAULT_POLICY_H
#define _MYTHIC_VAULT_POLICY_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "MythicCrestPolicy.h"

// The Great Vault (solo-wow), as retail's: three rows of three slots, each slot unlocked by enough of the row's
// activity in a week. The week after, every unlocked slot offers one item and the player takes one of them.
namespace MythicPlus
{
enum class VaultRow : uint8_t
{
    Raid = 1,
    Dungeon = 2,
    World = 3
};

inline constexpr uint8_t VaultRows = 3;
inline constexpr uint8_t VaultSlots = 3;
// A World slot gives a Mythic 0 item, the lowest of the three rows
inline constexpr uint8_t WorldRank = 0;

// The activity slot 1-3 of `row` needs: raid bosses 2/4/6, keystones 1/4/8, heroic dungeons 2/4/8; 0 for no slot
constexpr uint8_t VaultThreshold(VaultRow row, uint8_t slot)
{
    if (slot < 1 || slot > VaultSlots)
        return 0;
    switch (row)
    {
        case VaultRow::Raid: return uint8_t(2 * slot);
        case VaultRow::Dungeon: return slot == 1 ? 1 : uint8_t(4 * (slot - 1));
        case VaultRow::World: return uint8_t(2 << (slot - 1));
        default: return 0;
    }
}

// The rank a keystone's slot gives: the first rank its crest tier upgrades to, plus one (+2-4 Mythic 2, +5-9 Mythic
// 6, +10-14 Mythic 10, +15 and up Mythic 14); 0 below +2
constexpr uint8_t VaultRankForKey(uint8_t level)
{
    Crest const crest = CrestForKeyLevel(level);
    return crest == Crest::None ? 0 : uint8_t((uint8_t(crest) - 1) * RanksPerTier + 2);
}

// A choice is made from the options last shown (the addon's window or the gossip list); after the reset they are
// another week's, and the choice is refused until the new ones are shown. No week shown yet is never current.
inline constexpr uint32_t VaultNotShown = UINT32_MAX;

constexpr bool VaultChoiceStale(uint32_t shownWeek, uint32_t optionsWeek)
{
    return shownWeek == VaultNotShown || shownWeek != optionsWeek;
}

// Each slot's value from the week's key levels (or raid item levels), best first: slot n takes the value at its
// threshold, or 0 when there were fewer
inline std::array<uint16_t, VaultSlots> NthBest(std::vector<uint16_t> values, VaultRow row)
{
    std::sort(values.begin(), values.end(), std::greater<uint16_t>());
    std::array<uint16_t, VaultSlots> slots{};
    for (uint8_t slot = 1; slot <= VaultSlots; ++slot)
    {
        uint8_t const threshold = VaultThreshold(row, slot);
        if (threshold && values.size() >= threshold)
            slots[slot - 1] = values[threshold - 1];
    }
    return slots;
}
}

#endif
