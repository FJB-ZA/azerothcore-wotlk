/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_PROTOCOL_POLICY_H
#define _MYTHIC_PROTOCOL_POLICY_H

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "MythicAffixPolicy.h"
#include "MythicCrestPolicy.h"
#include "MythicKeystonePolicy.h"

// Mythic+ addon messages (solo-wow): hidden whispers from the player to themself with this prefix, read by the
// MythicPlus addon. Numbers only, space separated, so any client locale reads them.
namespace MythicPlus
{
inline constexpr char const* AddonPrefix = "SoloWowMythic";

// "K1 <map> <level>", map 0 when the player holds no keystone
inline std::string FormatKeystone(uint32_t map, uint8_t level)
{
    return "K1 " + std::to_string(map) + ' ' + std::to_string(map ? level : 0);
}

// "W1 <week> <+4 affix> <+7 affix> <+10 affix>"
inline std::string FormatWeek(uint32_t week, WeekAffixes const& affixes)
{
    return "W1 " + std::to_string(week) + ' ' + std::to_string(uint32_t(affixes.plus4)) + ' ' +
           std::to_string(uint32_t(affixes.plus7)) + ' ' + std::to_string(uint32_t(affixes.plus10));
}

struct RunSnapshot
{
    RunState state = RunState::None;
    uint32_t map = 0;
    uint8_t level = 0;
    uint32_t elapsedSeconds = 0;
    uint32_t limitSeconds = 0;
    uint32_t deaths = 0;
    uint32_t forces = 0;
    uint32_t quota = 0;
    uint8_t bosses = 0;
    uint8_t requiredBosses = 0;
};

// "R1 <state> <map> <level> <elapsed s, penalties included> <limit s> <deaths> <forces> <quota> <bosses> <required>"
inline std::string FormatRun(RunSnapshot const& run)
{
    return "R1 " + std::to_string(uint32_t(run.state)) + ' ' + std::to_string(run.map) + ' ' +
           std::to_string(run.level) + ' ' + std::to_string(run.elapsedSeconds) + ' ' +
           std::to_string(run.limitSeconds) + ' ' + std::to_string(run.deaths) + ' ' + std::to_string(run.forces) +
           ' ' + std::to_string(run.quota) + ' ' + std::to_string(run.bosses) + ' ' +
           std::to_string(run.requiredBosses);
}

// "C1 <owned per tier, Cobalt first> <earned this week per tier> <weekly cap>"
inline std::string FormatCrests(CrestWallet const& owned, CrestWallet const& earned)
{
    std::string text = "C1";
    for (uint32_t amount : owned)
        text += ' ' + std::to_string(amount);
    for (uint32_t amount : earned)
        text += ' ' + std::to_string(amount);
    return text + ' ' + std::to_string(WeeklyCrestCap);
}

// "E1 <map> <level> <seconds> <upgrade 0-3> <new key map> <new key level> <crest tier> <crests>"
inline std::string FormatResult(uint32_t map, uint8_t level, uint32_t seconds, uint8_t upgrade, uint32_t keyMap,
                                uint8_t keyLevel, Crest crest, uint32_t crests)
{
    return "E1 " + std::to_string(map) + ' ' + std::to_string(level) + ' ' + std::to_string(seconds) + ' ' +
           std::to_string(upgrade) + ' ' + std::to_string(keyMap) + ' ' + std::to_string(keyLevel) + ' ' +
           std::to_string(uint32_t(crest)) + ' ' + std::to_string(crests);
}

// Client to server: "Q" asks for everything again
constexpr bool IsQuery(std::string_view body)
{
    return body == "Q";
}

struct UpgradeRequest
{
    uint8_t bag;
    uint8_t slot;
};

// Client to server: "U <bag> <slot>" upgrades the item there by one rank, in the inventory's own bag and slot numbers
inline std::optional<UpgradeRequest> ParseUpgrade(std::string_view body)
{
    if (body.size() < 5 || body.substr(0, 2) != "U ")
        return std::nullopt;
    std::string_view rest = body.substr(2);
    std::size_t const space = rest.find(' ');
    if (space == std::string_view::npos)
        return std::nullopt;
    unsigned bag = 0;
    unsigned slot = 0;
    std::string_view const bagText = rest.substr(0, space);
    std::string_view const slotText = rest.substr(space + 1);
    auto const [bagEnd, bagError] = std::from_chars(bagText.data(), bagText.data() + bagText.size(), bag);
    auto const [slotEnd, slotError] = std::from_chars(slotText.data(), slotText.data() + slotText.size(), slot);
    if (bagError != std::errc() || slotError != std::errc() || bagEnd != bagText.data() + bagText.size() ||
        slotEnd != slotText.data() + slotText.size() || bag > 255 || slot > 255)
        return std::nullopt;
    return UpgradeRequest{uint8_t(bag), uint8_t(slot)};
}
}

#endif
