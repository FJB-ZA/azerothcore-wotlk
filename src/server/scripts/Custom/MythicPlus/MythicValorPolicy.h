/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_VALOR_POLICY_H
#define _MYTHIC_VALOR_POLICY_H

#include <algorithm>
#include <cstdint>

#include "MythicCrestPolicy.h"

// Mythic+ Valorstones (solo-wow): the end cache's currency. They buy crests, up to the tier of the best key a player
// has finished and within the weekly crest cap, or, with a Blacksmith's Prismatic Socket Kit, prismatic sockets on
// Mythic+ items, up to what the ICC 25 normal item for that slot has.
namespace MythicPlus
{
inline constexpr uint32_t ValorPerKeyLevel = 5;
inline constexpr uint32_t ValorTimedBonus = 10;
// One rank's worth of crests per purchase
inline constexpr uint32_t CrestBundle = CrestsPerRank;
inline constexpr uint32_t ValorPerBundleTier = 30;
inline constexpr uint32_t ValorPerSocket = 100;
// Gem slots an item has, its own sockets and prismatic ones together (the core's MAX_GEM_SOCKETS)
inline constexpr uint8_t MaxSockets = 3;

// Every participant's Valorstones from opening the cache
constexpr uint32_t ValorForRun(uint8_t level, bool timed)
{
    return ValorPerKeyLevel * level + (timed ? ValorTimedBonus : 0);
}

constexpr uint32_t CrestBundlePrice(Crest crest)
{
    return ValorPerBundleTier * uint32_t(crest);
}

// The lowest key whose run awards `crest`
constexpr uint8_t KeyLevelForCrest(Crest crest)
{
    switch (crest)
    {
        case Crest::Titansteel: return 15;
        case Crest::Titanium: return 10;
        case Crest::Saronite: return 5;
        case Crest::Cobalt: return 2;
        default: return 0;
    }
}

// Tiers above the best finished key aren't for sale, so low keys can't buy the top ranks
constexpr bool CanBuyCrest(Crest crest, uint8_t bestKeyLevel)
{
    return crest != Crest::None && uint8_t(crest) <= uint8_t(CrestForKeyLevel(bestKeyLevel));
}

// A bundle goes through only whole, inside the weekly cap
constexpr bool BundleFitsWeek(uint32_t earnedThisWeek)
{
    return CappedAward(CrestBundle, earnedThisWeek) == CrestBundle;
}

// The price of the next prismatic socket, when the item already has `prismatic` of them
constexpr uint32_t SocketPrice(uint8_t prismatic)
{
    return ValorPerSocket * (prismatic + 1u);
}

// InventoryType values (ItemTemplate.h)
enum SocketSlot : uint8_t
{
    SlotHead = 1,
    SlotNeck = 2,
    SlotShoulders = 3,
    SlotChest = 5,
    SlotWaist = 6,
    SlotLegs = 7,
    SlotFeet = 8,
    SlotWrists = 9,
    SlotHands = 10,
    SlotFinger = 11,
    SlotWeapon = 13,
    SlotShield = 14,
    SlotCloak = 16,
    SlotTwoHand = 17,
    SlotRobe = 20,
    SlotMainHand = 21,
    SlotOffHand = 22
};

// The usual socket count of item level 264 (ICC 25 normal) gear in each slot; a belt also takes its buckle
constexpr uint8_t SocketCap(uint32_t inventoryType)
{
    switch (inventoryType)
    {
        case SlotChest:
        case SlotRobe:
        case SlotLegs:
        case SlotWaist:
            return 3;
        case SlotHead:
        case SlotFeet:
        case SlotHands:
        case SlotTwoHand:
            return 2;
        case SlotNeck:
        case SlotShoulders:
        case SlotWrists:
        case SlotFinger:
        case SlotWeapon:
        case SlotShield:
        case SlotCloak:
        case SlotMainHand:
        case SlotOffHand:
            return 1;
        default:
            return 0;
    }
}

// Whether one more prismatic socket fits: `own` sockets from the item itself, `prismatic` already added
constexpr bool CanAddSocket(uint32_t inventoryType, uint8_t own, uint8_t prismatic)
{
    return own + prismatic < std::min(SocketCap(inventoryType), MaxSockets);
}
}

#endif
