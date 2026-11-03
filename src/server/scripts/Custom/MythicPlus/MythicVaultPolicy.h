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

// The Wrath raids whose bosses count for the Raids row, and the item level each difficulty gives (0 10-man normal,
// 1 25-man normal, 2 10-man heroic, 3 25-man heroic; 0 where it doesn't exist). Vault of Archavon isn't one: its loot
// is PvP gear and tier tokens.
struct RaidInfo
{
    uint16_t map;
    std::array<uint16_t, 4> itemLevel;
};

inline constexpr uint16_t OnyxiasLair = 249;
// Onyxia counts at her Wrath level only, not as the classic raid
inline constexpr uint8_t RaidMinCreatureLevel = 80;

// solo-wow tools/mythic-plus/generate_items.py reads this table for the raids' loot (mythic_plus_raid_item)
inline constexpr std::array<RaidInfo, 8> Raids = {{
    {533, {200, 213, 0, 0}},     // Naxxramas
    {615, {200, 213, 0, 0}},     // The Obsidian Sanctum
    {616, {213, 226, 0, 0}},     // The Eye of Eternity
    {603, {219, 226, 0, 0}},     // Ulduar
    {249, {232, 245, 0, 0}},     // Onyxia's Lair
    {649, {232, 245, 245, 258}}, // Trial of the Crusader
    {631, {251, 264, 264, 277}}, // Icecrown Citadel
    {724, {258, 271, 271, 284}}, // The Ruby Sanctum
}};

constexpr RaidInfo const* FindRaid(uint16_t map)
{
    for (RaidInfo const& raid : Raids)
        if (raid.map == map)
            return &raid;
    return nullptr;
}

constexpr uint16_t RaidItemLevel(uint16_t map, uint8_t difficulty)
{
    RaidInfo const* raid = FindRaid(map);
    return raid && difficulty < raid->itemLevel.size() ? raid->itemLevel[difficulty] : 0;
}

// Whether a boss of `creatureLevel` killed in `map` counts toward the Raids row
constexpr bool CountsForRaid(uint16_t map, uint8_t creatureLevel)
{
    return FindRaid(map) && (map != OnyxiasLair || creatureLevel >= RaidMinCreatureLevel);
}

// A raid boss killed this week: where, on which difficulty, and its encounter credit
struct RaidKill
{
    uint16_t map;
    uint8_t difficulty;
    uint32_t boss;
};

// The week's kills as the Raids row counts them: each boss once, however many difficulties it fell on (10- and
// 25-man are separate lockouts with the same credit), at the difficulty giving the highest item level; kills on a
// difficulty that gives none are left out. First-seen order is kept.
inline std::vector<RaidKill> BestKillPerBoss(std::vector<RaidKill> const& kills)
{
    std::vector<RaidKill> best;
    for (RaidKill const& kill : kills)
    {
        uint16_t const level = RaidItemLevel(kill.map, kill.difficulty);
        if (!level)
            continue;
        auto const known = std::find_if(best.begin(), best.end(), [&](RaidKill const& other)
                                        { return other.map == kill.map && other.boss == kill.boss; });
        if (known == best.end())
            best.push_back(kill);
        else if (level > RaidItemLevel(known->map, known->difficulty))
            *known = kill;
    }
    return best;
}

// An item a Raids slot could give: its Raid 0 id and its real item level
struct RaidReward
{
    uint32_t id;
    uint16_t itemLevel;
};

// A Raids slot's reward pool from the items its loot holds: those at the slot's item level, as retail's vault
// promises, or all of them when none is
inline std::vector<uint32_t> RaidRewardPool(std::vector<RaidReward> const& items, uint16_t itemLevel)
{
    std::vector<uint32_t> atLevel;
    std::vector<uint32_t> all;
    for (RaidReward const& item : items)
    {
        all.push_back(item.id);
        if (item.itemLevel == itemLevel)
            atLevel.push_back(item.id);
    }
    return atLevel.empty() ? all : atLevel;
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
