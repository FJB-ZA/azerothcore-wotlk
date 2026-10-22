/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _SPEC_SLOTS_POLICY_H
#define _SPEC_SLOTS_POLICY_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Spec slots (solo-wow): up to four talent groups per character, bought from class trainers.
// Slot N (1-based) is the core's talent group N - 1.
namespace SpecSlots
{
inline constexpr uint8_t MaxSlots = 4;
inline constexpr uint8_t UnlockLevel = 10;
inline constexpr uint32_t FirstSwapSpell = 990010;  // patches/client/spell-data/30-spec-slots.json
inline constexpr char const* AddonPrefix = "SoloWowSlots";
inline constexpr uint32_t DefaultPrice[3] = {250000, 750000, 1500000};  // slots 2-4 in copper (vanilla prices)

// The switch spell for a slot, or 0 when the slot doesn't exist
constexpr uint32_t SwapSpell(uint8_t slot)
{
    return slot >= 1 && slot <= MaxSlots ? FirstSwapSpell + slot - 1 : 0;
}

// The slot a switch spell leads to, or 0 when the spell isn't one of ours
constexpr uint8_t SlotOfSwapSpell(uint32_t spellId)
{
    return spellId >= FirstSwapSpell && spellId < FirstSwapSpell + MaxSlots ? uint8_t(spellId - FirstSwapSpell + 1) : 0;
}

enum class BuyCheck { Ok, TooLow, AllOwned, NoMoney };

// Can a character who owns `owned` slots buy the next one at `price` copper?
constexpr BuyCheck CanBuy(uint8_t level, uint8_t owned, uint64_t money, uint32_t price)
{
    if (level < UnlockLevel)
        return BuyCheck::TooLow;
    if (owned >= MaxSlots)
        return BuyCheck::AllOwned;
    if (money < price)
        return BuyCheck::NoMoney;
    return BuyCheck::Ok;
}

enum class SwapCheck { Ok, NotOwned, AlreadyActive, InCombat, InArena };

// Can a character switch to `slot`? `owned` and `active` are slot counts/numbers, 1-based.
constexpr SwapCheck CanSwap(uint8_t slot, uint8_t owned, uint8_t active, bool inCombat, bool inArena)
{
    if (slot < 1 || slot > MaxSlots || slot > owned)
        return SwapCheck::NotOwned;
    if (slot == active)
        return SwapCheck::AlreadyActive;
    if (inCombat)
        return SwapCheck::InCombat;
    if (inArena)
        return SwapCheck::InArena;
    return SwapCheck::Ok;
}

// Talent points spent in each of a slot's three trees
struct TreePoints
{
    uint8_t a, b, c;
};

// Server to addon: "S1 <active> <owned> <nextPrice> <p1> <p2> <p3> <p4>", where pN is "a/b/c" or "-" when not owned
inline std::string FormatState(uint8_t active, uint8_t owned, uint32_t nextPrice, std::array<TreePoints, MaxSlots> const& points)
{
    std::string state = "S1 " + std::to_string(active) + ' ' + std::to_string(owned) + ' ' + std::to_string(nextPrice);
    for (uint8_t i = 0; i < MaxSlots; ++i)
    {
        state += ' ';
        if (i < owned)
            state += std::to_string(points[i].a) + '/' + std::to_string(points[i].b) + '/' + std::to_string(points[i].c);
        else
            state += '-';
    }
    return state;
}

// A slot's sub-class path for the addon: "-" for the plain class, else "<id>:<Name>" with spaces as underscores
inline std::string FormatPath(uint8_t pathId, std::string name)
{
    if (pathId == 0)
        return "-";
    for (char& c : name)
        if (c == ' ')
            c = '_';
    return std::to_string(pathId) + ':' + name;
}

// Server to addon, with paths: "S2 <active> <owned> <nextPrice> <p1> <p2> <p3> <p4> <path1> <path2> <path3> <path4>".
// Clients that only know S1 ignore it and keep the S1 the server also sends.
inline std::string FormatStateWithPaths(uint8_t active, uint8_t owned, uint32_t nextPrice,
                                        std::array<TreePoints, MaxSlots> const& points,
                                        std::array<std::string, MaxSlots> const& paths)
{
    std::string state = FormatState(active, owned, nextPrice, points);
    state[1] = '2';
    for (std::string const& path : paths)
        state += ' ' + path;
    return state;
}

struct Command
{
    enum class Kind { Query, Swap } kind;
    uint8_t slot;
};

// Addon to server: "Q" asks for the state, "W <1-4>" switches slot. Anything else is ignored.
inline std::optional<Command> ParseCommand(std::string_view body)
{
    if (body == "Q")
        return Command{Command::Kind::Query, 0};
    if (body.size() == 3 && body[0] == 'W' && body[1] == ' ' && body[2] >= '1' && body[2] < char('1' + MaxSlots))
        return Command{Command::Kind::Swap, uint8_t(body[2] - '0')};
    return std::nullopt;
}
}

#endif
