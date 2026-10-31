/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _SPELL_CHARGES_POLICY_H
#define _SPELL_CHARGES_POLICY_H

#include <cstdint>
#include <string>
#include <string_view>

// Retail's spell charges on 3.3.5: casts spend charges, one recharges at a time, only an empty spell is on cooldown.
namespace SpellCharges
{
inline constexpr char const* AddonPrefix = "SoloWowCharges";

struct State
{
    uint8_t charges = 0;
    uint64_t rechargeEnd = 0; // game time in ms when the next charge is back; 0 while full
};

constexpr State Full(uint8_t max)
{
    return {max, 0};
}

// A player who logs in with the spell on cooldown has no charge left until that cooldown ends
constexpr State FromCooldown(uint8_t max, uint64_t now, uint64_t cooldownMs)
{
    return cooldownMs ? State{0, now + cooldownMs} : Full(max);
}

// Every recharge finished by `now` returns a charge and, while still short of `max`, starts the next one.
constexpr State Refresh(State state, uint8_t max, uint32_t rechargeMs, uint64_t now)
{
    if (state.charges >= max)
        return Full(max);
    while (state.charges < max && state.rechargeEnd && now >= state.rechargeEnd)
    {
        ++state.charges;
        state.rechargeEnd = state.charges < max ? state.rechargeEnd + rechargeMs : 0;
    }
    return state;
}

// The character save drops cooldowns that end within this many ms, so a missing one that close proves nothing.
inline constexpr uint32_t SaveDropMs = 1000;

// An empty spell whose cooldown something else cleared (an arena start, a GM) is full again.
constexpr State Reconcile(State state, uint8_t max, bool cooldownKept, uint64_t now)
{
    return state.charges == 0 && !cooldownKept && state.rechargeEnd > now + SaveDropMs ? Full(max) : state;
}

// Spends one charge; a spell going below full starts recharging. An empty spell stays empty (the cast was triggered).
constexpr State Spend(State state, uint8_t max, uint32_t rechargeMs, uint64_t now)
{
    state = Refresh(state, max, rechargeMs, now);
    if (!state.charges)
        return state;
    --state.charges;
    if (!state.rechargeEnd)
        state.rechargeEnd = now + rechargeMs;
    return state;
}

// The cooldown an empty spell gets: until its next charge is back. 0 while a charge is left.
constexpr uint32_t CooldownMs(State state, uint64_t now)
{
    if (state.charges || state.rechargeEnd <= now)
        return 0;
    return uint32_t(state.rechargeEnd - now);
}

// Server to client: "C1 <spell> <charges> <max> <ms until the next charge, 0 when full> <recharge ms>"
inline std::string Format(uint32_t spellId, State state, uint8_t max, uint32_t rechargeMs, uint64_t now)
{
    uint64_t const next = state.rechargeEnd > now ? state.rechargeEnd - now : 0;
    return "C1 " + std::to_string(spellId) + ' ' + std::to_string(state.charges) + ' ' + std::to_string(max) + ' ' +
           std::to_string(next) + ' ' + std::to_string(rechargeMs);
}

// Client to server: "Q" asks for every charge spell the player knows
constexpr bool IsQuery(std::string_view body)
{
    return body == "Q";
}
}

#endif
