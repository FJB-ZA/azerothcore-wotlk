/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _CLIENT_STATUS_POLICY_H
#define _CLIENT_STATUS_POLICY_H

#include <cstdint>
#include <string>
#include <string_view>

// Client status (solo-wow): the player's progression step and personal XP rate, sent to their own client as hidden
// addon whispers. The dev client keeps them for Lua (Ui/SoloWow.cpp in FJB-ZA/wowdecomp); on the stock client the
// ProgressionTracker addon reads them as CHAT_MSG_ADDON.
namespace ClientStatus
{
inline constexpr char const* AddonPrefix = "SoloWow";
// mod-individual-progression marks progression step n by rewarding quest 66000 + n.
inline constexpr uint32_t ProgressionQuestBase = 66000;
inline constexpr uint8_t MaxProgressionStep = 18;

constexpr bool IsProgressionQuest(uint32_t questId)
{
    return questId >= ProgressionQuestBase && questId <= ProgressionQuestBase + MaxProgressionStep;
}

// A rate in hundredths, so the client reads whole numbers whatever its locale
constexpr uint32_t Hundredths(float rate)
{
    return rate > 0.0f ? uint32_t(rate * 100.0f + 0.5f) : 0;
}

// Server to client: "P1 <step>"
inline std::string FormatProgression(uint8_t step)
{
    return "P1 " + std::to_string(step);
}

// Server to client: "X1 <rate x 100> <maximum rate x 100> <paused 0 or 1>"
inline std::string FormatXpRate(float rate, float maxRate, bool paused)
{
    return "X1 " + std::to_string(Hundredths(rate)) + ' ' + std::to_string(Hundredths(maxRate)) +
           (paused ? " 1" : " 0");
}

// Client to server: "Q" asks for both again
constexpr bool IsQuery(std::string_view body)
{
    return body == "Q";
}

// Commands that can change the status: mod-individual-xp's ".xp ..." and mod-individual-progression's ".ip ...".
// `command` is the text after the dot; command names are matched without regard to case, like the core does.
constexpr bool ChangesStatus(std::string_view command)
{
    std::size_t const end = command.find(' ');
    std::string_view const name = command.substr(0, end);
    if (name.size() != 2)
        return false;
    char const first = static_cast<char>(name[0] | 0x20);
    char const second = static_cast<char>(name[1] | 0x20);
    return (first == 'x' || first == 'i') && second == 'p';
}
}

#endif
