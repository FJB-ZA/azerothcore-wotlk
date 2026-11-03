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
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "MythicAffixPolicy.h"
#include "MythicCrestPolicy.h"
#include "MythicKeystonePolicy.h"
#include "MythicVaultPolicy.h"

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

// "E1 <map> <level> <seconds> <upgrade 0-3> <new key map> <new key level> <crest tier> <crests> <own key 0/1>": the
// player's keystone after the run, and whether the run's key was theirs (only its owner's keystone moves)
inline std::string FormatResult(uint32_t map, uint8_t level, uint32_t seconds, uint8_t upgrade, uint32_t keyMap,
                                uint8_t keyLevel, Crest crest, uint32_t crests, bool ownKey)
{
    return "E1 " + std::to_string(map) + ' ' + std::to_string(level) + ' ' + std::to_string(seconds) + ' ' +
           std::to_string(upgrade) + ' ' + std::to_string(keyMap) + ' ' + std::to_string(keyLevel) + ' ' +
           std::to_string(uint32_t(crest)) + ' ' + std::to_string(crests) + ' ' + (ownKey ? '1' : '0');
}

// "V1 <valorstones> <highest crest tier they buy, 0 none>"
inline std::string FormatValor(uint32_t valor, Crest buyable)
{
    return "V1 " + std::to_string(valor) + ' ' + std::to_string(uint32_t(buyable));
}

// One finished run: "<map> <level> <seconds> <upgrade 0-3>"
struct RunRecord
{
    uint16_t map = 0;
    uint8_t level = 0;
    uint32_t seconds = 0;
    uint8_t upgrade = 0;
};

inline std::string RunFields(RunRecord const& run)
{
    return std::to_string(run.map) + ' ' + std::to_string(run.level) + ' ' + std::to_string(run.seconds) + ' ' +
           std::to_string(run.upgrade);
}

// "H1 <count>" starts this week's run list, then one "H2 <run>" per run
inline std::string FormatWeekRunCount(std::size_t count)
{
    return "H1 " + std::to_string(count);
}

inline std::string FormatWeekRun(RunRecord const& run)
{
    return "H2 " + RunFields(run);
}

// "B1 <run>": the best run ever in that dungeon
inline std::string FormatBestRun(RunRecord const& run)
{
    return "B1 " + RunFields(run);
}

// The Great Vault: "G1 <1 when last week's options wait at the Keystone Master>" comes first, then one
// "G3 <row> <slot> <item>" per option waiting, then nine "G2 <row> <slot> <progress> <threshold> <item level or 0>",
// the week the options came from while they wait and this week's progress after
inline std::string FormatVault(bool ready)
{
    return ready ? "G1 1" : "G1 0";
}

inline std::string FormatVaultSlot(VaultRow row, uint8_t slot, uint32_t progress, uint32_t threshold,
                                   uint32_t itemLevel)
{
    return "G2 " + std::to_string(uint32_t(row)) + ' ' + std::to_string(slot) + ' ' + std::to_string(progress) + ' ' +
           std::to_string(threshold) + ' ' + std::to_string(itemLevel);
}

inline std::string FormatVaultOption(VaultRow row, uint8_t slot, uint32_t itemId)
{
    return "G3 " + std::to_string(uint32_t(row)) + ' ' + std::to_string(slot) + ' ' + std::to_string(itemId);
}

// "F1 <count>" starts the Font of Power's key list, then one "F2 <holder guid low> <map> <level>" per key holder
inline std::string FormatFontKeys(std::size_t count)
{
    return "F1 " + std::to_string(count);
}

inline std::string FormatFontKey(uint32_t holderLow, uint32_t map, uint8_t level)
{
    return "F2 " + std::to_string(holderLow) + ' ' + std::to_string(map) + ' ' + std::to_string(level);
}

// Client to server: "Q" asks for everything again
constexpr bool IsQuery(std::string_view body)
{
    return body == "Q";
}

inline std::optional<unsigned> ParseNumber(std::string_view text)
{
    unsigned value = 0;
    auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc() || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

// Client to server: "P <crest tier>" buys one bundle of crests with Valorstones
inline std::optional<Crest> ParseCrestPurchase(std::string_view body)
{
    if (body.size() < 3 || body.substr(0, 2) != "P ")
        return std::nullopt;
    std::optional<unsigned> const tier = ParseNumber(body.substr(2));
    if (!tier || *tier < 1 || *tier > CrestTiers)
        return std::nullopt;
    return Crest(*tier);
}

struct UpgradeRequest
{
    uint8_t bag;
    uint8_t slot;
};

// Client to server: "<verb> <bag> <slot>" for the item there, in the inventory's own bag and slot numbers
inline std::optional<UpgradeRequest> ParseItemRequest(std::string_view body, char verb)
{
    if (body.size() < 5 || body[0] != verb || body[1] != ' ')
        return std::nullopt;
    std::string_view rest = body.substr(2);
    std::size_t const space = rest.find(' ');
    if (space == std::string_view::npos)
        return std::nullopt;
    std::optional<unsigned> const bag = ParseNumber(rest.substr(0, space));
    std::optional<unsigned> const slot = ParseNumber(rest.substr(space + 1));
    if (!bag || !slot || *bag > 255 || *slot > 255)
        return std::nullopt;
    return UpgradeRequest{uint8_t(*bag), uint8_t(*slot)};
}

// "A <holder guid low>" puts that holder's keystone in the Font of Power
inline std::optional<uint32_t> ParseActivate(std::string_view body)
{
    if (body.size() < 3 || body.substr(0, 2) != "A ")
        return std::nullopt;
    return ParseNumber(body.substr(2));
}

// "U <bag> <slot>" raises the item one rank
inline std::optional<UpgradeRequest> ParseUpgrade(std::string_view body)
{
    return ParseItemRequest(body, 'U');
}

// "S <bag> <slot>" adds a prismatic socket
inline std::optional<UpgradeRequest> ParseSocket(std::string_view body)
{
    return ParseItemRequest(body, 'S');
}

struct VaultChoice
{
    VaultRow row;
    uint8_t slot;
};

// "V <row 1-3> <slot 1-3>" takes the Great Vault's option in that slot
inline std::optional<VaultChoice> ParseVaultChoice(std::string_view body)
{
    std::optional<UpgradeRequest> const pair = ParseItemRequest(body, 'V');
    if (!pair || pair->bag < 1 || pair->bag > VaultRows || pair->slot < 1 || pair->slot > VaultSlots)
        return std::nullopt;
    return VaultChoice{VaultRow(pair->bag), pair->slot};
}
}

#endif
