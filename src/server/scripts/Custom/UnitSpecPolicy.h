/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _UNIT_SPEC_POLICY_H
#define _UNIT_SPEC_POLICY_H

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Unit spec: what another player (a bot, usually) plays, for the SpecSlots addon's unit tooltips. The addon whispers
// itself "SoloWowSpec\tW <guid> [<guid> ...]"; the server answers one line per player it found:
//   "S1 <guid> <tree> <p1> <p2> <p3> <p4> <pathId> <path name>"
// tree is the client's tab number (1-4) holding the most points, 0 for none; p4 is a path's own tree (tab 4).
namespace UnitSpec
{
inline constexpr char const* AddonPrefix = "SoloWowSpec";
inline constexpr std::size_t MaxQueryGuids = 8;  // one whisper asks for a party's worth; more is dropped
inline constexpr std::size_t Trees = 4;

using TreePoints = std::array<uint32_t, Trees>;

// The tab (1-4) with the most points; a tie goes to the earlier tab, as the core's own GetMostPointsTalentTree does.
constexpr uint8_t TopTree(TreePoints const& points)
{
    uint8_t top = 0;
    uint32_t most = 0;
    for (std::size_t i = 0; i < Trees; ++i)
        if (points[i] > most)
        {
            most = points[i];
            top = uint8_t(i + 1);
        }
    return top;
}

// "W 12 345" -> {12, 345}; anything else, or a guid of 0, makes an empty list. At most MaxQueryGuids guids.
inline std::vector<uint32_t> ParseQuery(std::string_view body)
{
    std::vector<uint32_t> guids;
    if (body.size() < 3 || body[0] != 'W' || body[1] != ' ')
        return guids;
    body.remove_prefix(2);
    while (!body.empty() && guids.size() < MaxQueryGuids)
    {
        std::size_t const end = body.find(' ');
        std::string_view const word = body.substr(0, end);
        if (word.empty() || word.size() > 10)
            return {};
        uint64_t value = 0;
        for (char const c : word)
        {
            if (c < '0' || c > '9')
                return {};
            value = value * 10 + uint64_t(c - '0');
        }
        if (value == 0 || value > 0xFFFFFFFFull)
            return {};
        guids.push_back(uint32_t(value));
        if (end == std::string_view::npos)
            break;
        body.remove_prefix(end + 1);
    }
    return guids;
}

inline std::string FormatSpec(uint32_t guid, TreePoints const& points, uint16_t pathId, std::string_view pathName)
{
    std::string line = "S1 " + std::to_string(guid) + ' ' + std::to_string(TopTree(points));
    for (uint32_t const p : points)
        line.append(" ").append(std::to_string(p));
    line.append(" ").append(std::to_string(pathId));
    if (pathId != 0 && !pathName.empty())
        line.append(" ").append(pathName);
    return line;
}
}

#endif
