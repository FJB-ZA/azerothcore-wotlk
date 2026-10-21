/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MENTOR_MODE_POLICY_H
#define _MENTOR_MODE_POLICY_H

#include <cstdint>

// Mentor mode (solo-wow): a player grouped with much lower-level members fights as their level.
namespace MentorMode
{
inline constexpr uint8_t MinGap = 5;   // mentoring starts this many levels above the lowest member nearby
inline constexpr uint8_t Margin = 2;   // the mentor then fights this many levels above that member
inline constexpr uint32_t CheckIntervalMs = 2000;
// The Mentoring debuff (custom spell; solo-wow patches/client/spell-data). Its stack count is the mentored level.
inline constexpr uint32_t AuraSpell = 990001;

// The level to fight at, or 0 when no mentoring applies.
constexpr uint8_t TargetLevel(uint8_t level, uint8_t lowestMember)
{
    return lowestMember && level >= lowestMember + MinGap ? static_cast<uint8_t>(lowestMember + Margin) : 0;
}

// Creature base stats are tuned per level so mobs are a fair fight for that level. Scaling a mentor's
// health, damage, healing and armour by the ratio of those stats between the mentored and the real level
// keeps the mentor as strong against the mobs as at the real level, never stronger.
constexpr float Scale(float atMentorLevel, float atRealLevel)
{
    return atMentorLevel > 0.0f && atRealLevel > 0.0f && atMentorLevel < atRealLevel ? atMentorLevel / atRealLevel
                                                                                      : 1.0f;
}

// Which expansion column of creature_classlevelstats describes a level's own content.
constexpr uint8_t StatColumn(uint8_t level) { return level <= 60 ? 0 : level <= 70 ? 1 : 2; }
}  // namespace MentorMode

#endif
