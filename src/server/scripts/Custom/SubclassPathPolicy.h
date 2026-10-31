/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _SUBCLASS_PATH_POLICY_H
#define _SUBCLASS_PATH_POLICY_H

#include <cstdint>
#include <string>
#include <string_view>

// Sub-class paths (solo-wow): each spec slot may follow one path of its class, whose replacement spells mirror the
// spells they replace. Path 0 is the plain class.
namespace SubclassPaths
{
inline constexpr uint16_t PlainClass = 0;
inline constexpr uint32_t Gold = 10000;
inline constexpr uint64_t MonthSeconds = 30ull * 24 * 3600;  // the core's MONTH
inline constexpr char const* AddonPrefix = "SoloWowPath";

// The core keeps a learned spell's talent groups as a bit mask (character_spell.specMask).
constexpr uint8_t SlotMask(uint8_t talentGroup) { return uint8_t(1u << talentGroup); }

// What a path asks of a character, from subclass_path
struct PathRules
{
    uint8_t classId;
    uint8_t unlockLevel;
    uint8_t era;  // 0 vanilla, 1 TBC, 2 Wrath
};

enum class ChangeCheck { Ok, SamePath, WrongClass, TooLow, NotYetInEra, Locked, InCombat, InArena, NoMoney };

// Can the active slot move from `current` to `wanted`? `rules` and `unlocked` are ignored when going back to the plain
// class, which is always allowed. `era` is the character's account expansion, which the server phase sets; `unlocked`
// is whether the character has done the path's unlock quest, if it has one.
constexpr ChangeCheck CanChangePath(uint16_t current, uint16_t wanted, PathRules rules, uint8_t classId, uint8_t level,
                                   uint8_t era, bool unlocked, bool inCombat, bool inArena, uint64_t money, uint32_t fee)
{
    if (wanted == current)
        return ChangeCheck::SamePath;
    if (wanted != PlainClass)
    {
        if (rules.classId != classId)
            return ChangeCheck::WrongClass;
        if (level < rules.unlockLevel)
            return ChangeCheck::TooLow;
        if (era < rules.era)
            return ChangeCheck::NotYetInEra;
        if (!unlocked)
            return ChangeCheck::Locked;
    }
    if (inCombat)
        return ChangeCheck::InCombat;
    if (inArena)
        return ChangeCheck::InArena;
    if (money < fee)
        return ChangeCheck::NoMoney;
    return ChangeCheck::Ok;
}

// The next path change fee, on the talent reset's schedule but with its own counter: 1, 5 and 10 gold, then 5 gold
// more per change up to 50, and 5 gold less for each month without a change, never below 10.
constexpr uint32_t NextFee(uint32_t lastFee, uint64_t secondsSinceLast)
{
    if (lastFee < 1 * Gold)
        return 1 * Gold;
    if (lastFee < 5 * Gold)
        return 5 * Gold;
    if (lastFee < 10 * Gold)
        return 10 * Gold;
    if (uint64_t const months = secondsSinceLast / MonthSeconds)
    {
        uint64_t const drop = 5 * Gold * months;
        return lastFee > drop + 10 * Gold ? uint32_t(lastFee - drop) : 10 * Gold;
    }
    return lastFee + 5 * Gold > 50 * Gold ? 50 * Gold : lastFee + 5 * Gold;
}

// Per-path tuning from subclass_path, 1.0 meaning unchanged
struct Modifiers
{
    float damagePve = 1.0f;
    float damagePvp = 1.0f;
    float healingPve = 1.0f;
    float healingPvp = 1.0f;

    constexpr bool Neutral() const
    {
        return damagePve == 1.0f && damagePvp == 1.0f && healingPve == 1.0f && healingPvp == 1.0f;
    }
};

// Damage dealt by a path's character or its minions: the PvP value against anything a player controls. A pet split
// into `bodies` melees for that share each, so all bodies together match the original pet.
constexpr float DamageFactor(Modifiers const& modifiers, bool againstPlayer, uint8_t bodies)
{
    return (againstPlayer ? modifiers.damagePvp : modifiers.damagePve) / float(bodies > 1 ? bodies : 1);
}

// Healing done by a path's character: the PvP value inside battlegrounds and arenas
constexpr float HealingFactor(Modifiers const& modifiers, bool inPvpZone)
{
    return inPvpZone ? modifiers.healingPvp : modifiers.healingPve;
}

// Each of `bodies`' share of the original pet's health, as a multiplier on its own
constexpr float BodyShare(uint8_t bodies)
{
    return 1.0f / float(bodies > 1 ? bodies : 1);
}

enum class SpellMove { None, ToReplacement, ToOriginal, DropOriginal, GiveOriginal };

// One swap row in the active slot: its replacement belongs there only while the slot follows the row's path.
constexpr SpellMove Move(bool slotOnRowPath, bool hasOriginal, bool hasReplacement)
{
    if (slotOnRowPath)
        return hasOriginal ? SpellMove::ToReplacement : SpellMove::None;
    return hasReplacement ? SpellMove::ToOriginal : SpellMove::None;
}

// A swap row whose replacement is a talent of the path's tree: the talent system owns the replacement, so only the
// original moves. It leaves the slot while the slot is on the path and knows the talent, and comes back once either
// stops, but only if the path took it (`originalTaken`): a spell the slot never learned isn't handed out.
constexpr SpellMove TalentMove(bool slotOnRowPath, bool hasOriginal, bool hasTalent, bool originalTaken)
{
    bool const talentInEffect = slotOnRowPath && hasTalent;
    if (hasOriginal)
        return talentInEffect ? SpellMove::DropOriginal : SpellMove::None;
    return originalTaken && !talentInEffect ? SpellMove::GiveOriginal : SpellMove::None;
}

// A path's own tree (subclass_path.talent_tab) opens only while the active slot follows it; class trees have owner 0.
constexpr bool TreeOpen(uint16_t treeOwner, uint16_t activePath)
{
    return treeOwner == PlainClass || treeOwner == activePath;
}

// "P1 <slot> <pathId> <tree> <points> <name>"; tree is the client's tab number (1-4) or 0, points are spent in it.
inline std::string FormatSlotPath(uint8_t slot, uint16_t pathId, uint8_t tree, uint32_t points, std::string_view name)
{
    std::string line = "P1 " + std::to_string(slot) + ' ' + std::to_string(pathId) + ' ' + std::to_string(tree) + ' ' +
                       std::to_string(points);
    if (pathId != PlainClass && !name.empty())
        line.append(" ").append(name);
    return line;
}

// A stable hash of guid and path, so the same `share` (0-1) of a class's bots take the path and nothing is stored.
constexpr bool BotTakesPath(uint32_t guid, uint16_t pathId, float share)
{
    if (share <= 0.0f)
        return false;
    uint32_t hash = guid * 2654435761u ^ uint32_t(pathId) * 40503u;
    hash ^= hash >> 16;
    hash *= 0x45d9f3bu;
    hash ^= hash >> 16;
    return float(hash % 10000u) < share * 10000.0f;
}

// The addon asks for every slot's line with "Q".
constexpr bool IsQuery(std::string_view body) { return body == "Q"; }
}

#endif
