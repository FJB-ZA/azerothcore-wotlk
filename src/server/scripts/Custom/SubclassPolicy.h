/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _SUBCLASS_POLICY_H
#define _SUBCLASS_POLICY_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Sub-class paths (solo-wow): an optional way to play a class, held per spec slot. A path swaps some spells and pets
// for themed mirrors with the same numbers. Path 0 is the plain class. The paths themselves are data (world tables
// subclass_path, subclass_spell_swap, subclass_creature_swap); these are the rules around them.
namespace Subclass
{
inline constexpr uint32_t FirstPathSpell = 990100;
inline constexpr uint32_t SpellsPerPath = 200;
inline constexpr uint32_t LastCustomSpell = 990999;
inline constexpr uint32_t Gold = 10000;  // copper

// The custom spell ids a path owns: path 1 is 990100-990299, path 2 990300-990499, and so on. {0, 0} when the range
// would run past the custom spell range.
constexpr std::pair<uint32_t, uint32_t> SpellRange(uint8_t pathIndex)
{
    if (pathIndex == 0)
        return {0, 0};
    uint32_t const first = FirstPathSpell + (pathIndex - 1u) * SpellsPerPath;
    uint32_t const last = first + SpellsPerPath - 1;
    if (last > LastCustomSpell)
        return {0, 0};
    return {first, last};
}

enum class ChangeCheck { Ok, SamePath, WrongClass, TooLow, InCombat, NoMoney };

// Can a character on path `current` change the active slot to `target`? Checked in this order. Going back to the plain
// class (target 0) always fits the class and has no level requirement.
constexpr ChangeCheck CanChange(uint8_t current, uint8_t target, bool targetFitsClass, uint8_t level,
                                uint8_t unlockLevel, bool inCombat, uint64_t money, uint32_t fee)
{
    if (current == target)
        return ChangeCheck::SamePath;
    if (target != 0 && !targetFitsClass)
        return ChangeCheck::WrongClass;
    if (target != 0 && level < unlockLevel)
        return ChangeCheck::TooLow;
    if (inCombat)
        return ChangeCheck::InCombat;
    if (money < fee)
        return ChangeCheck::NoMoney;
    return ChangeCheck::Ok;
}

// The fee for the next path change, in copper, on its own counter: the talent reset's schedule. 1, 5 and 10 gold, then
// 5 gold more per change up to 50 gold. A month or more since the last change takes 5 gold off per month instead, never
// below 1 gold. `lastFee` is 0 before the first change.
constexpr uint32_t NextChangeFee(uint32_t lastFee, uint32_t monthsSinceLast)
{
    if (lastFee < 1 * Gold)
        return 1 * Gold;
    if (lastFee < 5 * Gold)
        return 5 * Gold;
    if (lastFee < 10 * Gold)
        return 10 * Gold;
    if (monthsSinceLast > 0)
    {
        uint64_t const discount = uint64_t(monthsSinceLast) * 5 * Gold;
        return discount + 1 * Gold > lastFee ? 1 * Gold : uint32_t(lastFee - discount);
    }
    return std::min<uint32_t>(lastFee + 5 * Gold, 50 * Gold);
}

// One spell rank of the plain class and its mirror on the path. Several originals may share one replacement (Fear's
// three ranks all become Entomb).
struct Swap
{
    uint32_t original;
    uint32_t replacement;
};

inline bool Contains(std::vector<uint32_t> const& list, uint32_t id)
{
    return std::find(list.begin(), list.end(), id) != list.end();
}

inline void AddOnce(std::vector<uint32_t>& list, uint32_t id)
{
    if (!Contains(list, id))
        list.push_back(id);
}

// Joining a path: the replacements to learn for the originals the character knows
inline std::vector<uint32_t> ToLearn(std::vector<Swap> const& swaps, std::vector<uint32_t> const& known)
{
    std::vector<uint32_t> learn;
    for (Swap const& swap : swaps)
        if (Contains(known, swap.original))
            AddOnce(learn, swap.replacement);
    return learn;
}

// Joining a path: the originals those replace
inline std::vector<uint32_t> ToUnlearn(std::vector<Swap> const& swaps, std::vector<uint32_t> const& known)
{
    std::vector<uint32_t> unlearn;
    for (Swap const& swap : swaps)
        if (Contains(known, swap.original))
            AddOnce(unlearn, swap.original);
    return unlearn;
}

// Leaving a path: the originals to give back for the replacements the character knows. When several originals share a
// replacement, the caller keeps the ones the character's level allows (Fear's ranks for Entomb).
inline std::vector<uint32_t> ToRestore(std::vector<Swap> const& swaps, std::vector<uint32_t> const& known)
{
    std::vector<uint32_t> restore;
    for (Swap const& swap : swaps)
        if (Contains(known, swap.replacement))
            AddOnce(restore, swap.original);
    return restore;
}

// Leaving a path: the replacements to forget
inline std::vector<uint32_t> ToForget(std::vector<Swap> const& swaps, std::vector<uint32_t> const& known)
{
    std::vector<uint32_t> forget;
    for (Swap const& swap : swaps)
        if (Contains(known, swap.replacement))
            AddOnce(forget, swap.replacement);
    return forget;
}

// An action-bar spell mapped to the path (original to replacement) or back. Going back to an original shared by several
// ranks picks the highest-listed rank the character knows (`known`), else the highest-listed one. Spells outside the
// table stay as they are.
inline uint32_t MapAction(std::vector<Swap> const& swaps, uint32_t spellId, bool toPath,
                          std::vector<uint32_t> const& known = {})
{
    if (toPath)
    {
        for (Swap const& swap : swaps)
            if (swap.original == spellId)
                return swap.replacement;
        return spellId;
    }
    uint32_t lastKnown = 0;
    uint32_t last = 0;
    for (Swap const& swap : swaps)
    {
        if (swap.replacement != spellId)
            continue;
        last = swap.original;
        if (Contains(known, swap.original))
            lastKnown = swap.original;
    }
    if (lastKnown)
        return lastKnown;
    return last ? last : spellId;
}

// The original spell a replacement mirrors, for bonuses that look at the school (Destruction's fire talents count a
// Necromancer's frost and disease mirrors). Any other spell is its own original.
inline uint32_t BonusSpell(std::vector<Swap> const& swaps, uint32_t spellId)
{
    for (Swap const& swap : swaps)
        if (swap.replacement == spellId)
            return swap.original;
    return spellId;
}
}

#endif
