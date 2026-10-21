/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _BOOST_POLICY_H
#define _BOOST_POLICY_H

#include <array>
#include <cstdint>

// Character boosts (solo-wow): a one-time boost per account and era takes a real player's character to the
// start of the era's current content, retail style.
namespace Boost
{
enum Era : uint8_t
{
    ERA_VANILLA = 0,
    ERA_TBC     = 1,
    ERA_WRATH   = 2
};

// mod-individual-progression marks each finished step by rewarding hidden quest 66000 + step.
inline constexpr uint32_t ProgressionQuestBase = 66000;
inline constexpr uint8_t ProgressionSteps = 18;
inline constexpr uint8_t TbcStep = 8;      // PROGRESSION_PRE_TBC: Outland is open
inline constexpr uint8_t WrathStep = 13;   // PROGRESSION_TBC_TIER_5: Northrend is open

inline constexpr uint32_t MenuId = 990200;  // player-sourced gossip menu of the .boost command
inline constexpr uint8_t BagCount = 4;
inline constexpr uint32_t AmmoCount = 1000;

constexpr uint8_t EraOfStep(uint8_t step)
{
    return step >= WrathStep ? ERA_WRATH : step >= TbcStep ? ERA_TBC : ERA_VANILLA;
}

// The era a boost belongs to: the player's own progression, never above the account's expansion (the
// server phase sets it). Without individual progression the account's expansion alone decides.
constexpr uint8_t EraFor(uint8_t progressionStep, bool individualProgression, uint8_t accountExpansion)
{
    uint8_t const account = accountExpansion > ERA_WRATH ? uint8_t(ERA_WRATH) : accountExpansion;
    uint8_t const era = individualProgression ? EraOfStep(progressionStep) : uint8_t(ERA_WRATH);
    return era < account ? era : account;
}

// Like retail, a boost lands at the start of the era's own content.
constexpr uint8_t BoostLevel(uint8_t era) { return era >= ERA_WRATH ? 68 : 58; }

// Which item pool (solo_wow_boost_pool.tier) gears the boost: 0 vanilla items, 1 Outland items.
constexpr uint8_t GearTier(uint8_t era) { return era >= ERA_WRATH ? 1 : 0; }

constexpr char const* EraName(uint8_t era) { return era >= ERA_WRATH ? "Wrath" : era == ERA_TBC ? "TBC" : "vanilla"; }

inline constexpr uint8_t ClassDeathKnight = 6;

// Death knights start at 55 with their own gear and quest chain, so they are never boosted.
constexpr bool CanBoost(uint8_t level, uint8_t classId, uint8_t era)
{
    return classId != ClassDeathKnight && level < BoostLevel(era);
}

struct Spot
{
    uint16_t map;
    float x, y, z, o;
    char const* name;
};

// Flight masters and quest givers at each era's starting hub, from the world database spawns.
constexpr Spot Landing(uint8_t era, bool horde)
{
    if (era >= ERA_WRATH)
        return horde ? Spot{571, 2922.39f, 6244.39f, 208.835f, 2.82743f, "Warsong Hold, Borean Tundra"}
                     : Spot{571, 2272.98f, 5171.82f, 11.246f, 1.74533f, "Valiance Keep, Borean Tundra"};
    if (era == ERA_TBC)
        return horde ? Spot{0, -11817.3f, -3187.89f, -30.5969f, 4.60767f, "the Dark Portal, Blasted Lands"}
                     : Spot{0, -11815.2f, -3195.54f, -30.9174f, 2.18166f, "the Dark Portal, Blasted Lands"};
    return horde ? Spot{0, 2328.48f, -5290.72f, 81.8744f, 2.9147f, "Light's Hope Chapel, Eastern Plaguelands"}
                 : Spot{0, 2269.85f, -5345.39f, 87.0242f, 1.76278f, "Light's Hope Chapel, Eastern Plaguelands"};
}

// Riding: apprentice in vanilla and TBC (epic riding was a level 60 goal there), journeyman in Wrath.
inline constexpr uint32_t ApprenticeRiding = 33388;
inline constexpr uint32_t JourneymanRiding = 33391;
constexpr bool EpicMount(uint8_t era) { return era >= ERA_WRATH; }

// The race's own vendor mount (normal or epic). 0 for unknown races.
constexpr uint32_t MountItem(uint8_t race, bool epic)
{
    switch (race)
    {
        case 1:  return epic ? 18776 : 2414;   // Human: Swift Palomino / Pinto Bridle
        case 2:  return epic ? 18796 : 1132;   // Orc: Horn of the Swift Brown Wolf / Horn of the Timber Wolf
        case 3:  return epic ? 18786 : 5864;   // Dwarf: Swift Brown Ram / Gray Ram
        case 4:  return epic ? 18766 : 8631;   // Night Elf: Reins of the Swift Frostsaber / Striped Frostsaber
        case 5:  return epic ? 13334 : 13331;  // Undead: Green Skeletal Warhorse / Red Skeletal Horse
        case 6:  return epic ? 18794 : 15277;  // Tauren: Great Brown Kodo / Gray Kodo
        case 7:  return epic ? 18772 : 8595;   // Gnome: Swift Green Mechanostrider / Blue Mechanostrider
        case 8:  return epic ? 18788 : 8588;   // Troll: Swift Blue Raptor / Whistle of the Emerald Raptor
        case 10: return epic ? 29223 : 29221;  // Blood Elf: Swift Green Hawkstrider / Black Hawkstrider
        case 11: return epic ? 29745 : 28481;  // Draenei: Great Blue Elekk / Brown Elekk
        default: return 0;
    }
}

// The era's bags: Runecloth Bag (vanilla, 14 slots) or Netherweave Bag (TBC, 16 slots).
constexpr uint32_t BagItem(uint8_t tier) { return tier ? 21841 : 14046; }

// Class tools the levelling quests would have given: shaman totems and rogue Thieves' Tools.
inline constexpr std::array<uint32_t, 4> ShamanTotems = {5175, 5176, 5177, 5178};
inline constexpr uint32_t ThievesTools = 5060;

enum Role : uint8_t
{
    ROLE_STRENGTH,     // plate and two-hand or dual-wield strength damage
    ROLE_TANK,         // shield tanks
    ROLE_PALADIN_TANK, // shield tank that also wants spell power
    ROLE_BEAR,         // feral tank
    ROLE_AGILITY,      // rogue
    ROLE_CAT,          // feral damage
    ROLE_HUNTER,
    ROLE_ENHANCEMENT,
    ROLE_CASTER,
    ROLE_HEALER,
    ROLE_COUNT
};

// Weapon layouts a spec may use; the best-scoring allowed one wins.
enum Layout : uint8_t
{
    LAYOUT_TWO_HAND   = 0x1,
    LAYOUT_DUAL_WIELD = 0x2,
    LAYOUT_SHIELD     = 0x4,  // one-hand and shield
    LAYOUT_OFF_HAND   = 0x8   // one-hand and held-in-off-hand
};

struct Spec
{
    char const* name;
    Role role;
    uint8_t layouts;
};

inline constexpr uint8_t MaxSpecs = 4;
using SpecList = std::array<Spec, MaxSpecs>;  // unused entries have no name

constexpr SpecList SpecsFor(uint8_t classId)
{
    constexpr uint8_t caster = LAYOUT_TWO_HAND | LAYOUT_OFF_HAND;
    switch (classId)
    {
        case 1: return {{{"Arms", ROLE_STRENGTH, LAYOUT_TWO_HAND},
                         {"Fury", ROLE_STRENGTH, LAYOUT_DUAL_WIELD | LAYOUT_TWO_HAND},
                         {"Protection", ROLE_TANK, LAYOUT_SHIELD}}};
        case 2: return {{{"Holy", ROLE_HEALER, LAYOUT_SHIELD | LAYOUT_OFF_HAND},
                         {"Protection", ROLE_PALADIN_TANK, LAYOUT_SHIELD},
                         {"Retribution", ROLE_STRENGTH, LAYOUT_TWO_HAND}}};
        case 3: return {{{"Beast Mastery", ROLE_HUNTER, LAYOUT_TWO_HAND | LAYOUT_DUAL_WIELD},
                         {"Marksmanship", ROLE_HUNTER, LAYOUT_TWO_HAND | LAYOUT_DUAL_WIELD},
                         {"Survival", ROLE_HUNTER, LAYOUT_TWO_HAND | LAYOUT_DUAL_WIELD}}};
        case 4: return {{{"Assassination", ROLE_AGILITY, LAYOUT_DUAL_WIELD},
                         {"Combat", ROLE_AGILITY, LAYOUT_DUAL_WIELD},
                         {"Subtlety", ROLE_AGILITY, LAYOUT_DUAL_WIELD}}};
        case 5: return {{{"Discipline", ROLE_HEALER, caster},
                         {"Holy", ROLE_HEALER, caster},
                         {"Shadow", ROLE_CASTER, caster}}};
        case 7: return {{{"Elemental", ROLE_CASTER, caster | LAYOUT_SHIELD},
                         {"Enhancement", ROLE_ENHANCEMENT, LAYOUT_TWO_HAND | LAYOUT_DUAL_WIELD},
                         {"Restoration", ROLE_HEALER, caster | LAYOUT_SHIELD}}};
        case 8: return {{{"Arcane", ROLE_CASTER, caster},
                         {"Fire", ROLE_CASTER, caster},
                         {"Frost", ROLE_CASTER, caster}}};
        case 9: return {{{"Affliction", ROLE_CASTER, caster},
                         {"Demonology", ROLE_CASTER, caster},
                         {"Destruction", ROLE_CASTER, caster}}};
        case 11: return {{{"Balance", ROLE_CASTER, caster},
                          {"Feral (cat)", ROLE_CAT, LAYOUT_TWO_HAND},
                          {"Feral (bear)", ROLE_BEAR, LAYOUT_TWO_HAND},
                          {"Restoration", ROLE_HEALER, caster}}};
        default: return {};
    }
}

// Item subclass of the armour a class wears at boost level (cloth 1, leather 2, mail 3, plate 4).
constexpr uint8_t ArmorSubclass(uint8_t classId)
{
    switch (classId)
    {
        case 1: case 2: return 4;
        case 3: case 7: return 3;
        case 4: case 11: return 2;
        default: return 1;
    }
}

enum Stat : uint8_t
{
    STAT_STRENGTH,
    STAT_AGILITY,
    STAT_STAMINA,
    STAT_INTELLECT,
    STAT_SPIRIT,
    STAT_ATTACK_POWER,
    STAT_RANGED_ATTACK_POWER,
    STAT_FERAL_ATTACK_POWER,
    STAT_SPELL_POWER,   // spell damage
    STAT_HEALING,
    STAT_HIT,
    STAT_CRIT,
    STAT_HASTE,
    STAT_EXPERTISE,
    STAT_ARMOR_PEN,
    STAT_DEFENSE,
    STAT_DODGE,
    STAT_PARRY,
    STAT_BLOCK,
    STAT_BLOCK_VALUE,
    STAT_MP5,
    STAT_ARMOR,
    STAT_MELEE_DPS,     // weapon damage per second, main hand or two-hand
    STAT_RANGED_DPS,
    STAT_COUNT
};

using Stats = std::array<float, STAT_COUNT>;

// Rating points worth 1% hit or crit at a level, for vanilla items whose bonuses are percentages.
constexpr float RatingPerPercent(uint8_t level) { return level <= 60 ? 10.0f : 14.0f; }

// Defense rating worth one point of defense skill, for vanilla items that raise the skill directly.
constexpr float RatingPerDefenseSkill(uint8_t level) { return level <= 60 ? 1.5f : 2.4f; }

// Value of one point of a stat for a role. Rough by design: it only has to rank items of one slot.
constexpr float Weight(Role role, Stat stat)
{
    switch (role)
    {
        case ROLE_STRENGTH:
            switch (stat)
            {
                case STAT_STRENGTH: return 2.2f;
                case STAT_AGILITY: return 1.2f;
                case STAT_STAMINA: return 0.6f;
                case STAT_ATTACK_POWER: return 1.0f;
                case STAT_HIT: return 1.8f;
                case STAT_CRIT: return 1.6f;
                case STAT_HASTE: return 1.2f;
                case STAT_EXPERTISE: return 1.6f;
                case STAT_ARMOR_PEN: return 1.2f;
                case STAT_ARMOR: return 0.005f;
                case STAT_MELEE_DPS: return 9.0f;
                case STAT_RANGED_DPS: return 0.5f;
                default: return 0.0f;
            }
        case ROLE_TANK:
        case ROLE_PALADIN_TANK:
            switch (stat)
            {
                case STAT_STAMINA: return 2.0f;
                case STAT_DEFENSE: return 2.4f;
                case STAT_DODGE: return 1.6f;
                case STAT_PARRY: return 1.4f;
                case STAT_BLOCK: return 1.0f;
                case STAT_BLOCK_VALUE: return 0.6f;
                case STAT_ARMOR: return 0.04f;
                case STAT_STRENGTH: return 1.0f;
                case STAT_AGILITY: return 1.0f;
                case STAT_HIT: return 0.6f;
                case STAT_EXPERTISE: return 0.9f;
                case STAT_ATTACK_POWER: return 0.3f;
                case STAT_SPELL_POWER: return role == ROLE_PALADIN_TANK ? 0.4f : 0.0f;
                case STAT_INTELLECT: return role == ROLE_PALADIN_TANK ? 0.3f : 0.0f;
                case STAT_MELEE_DPS: return 2.0f;
                default: return 0.0f;
            }
        case ROLE_BEAR:
            switch (stat)
            {
                case STAT_STAMINA: return 2.0f;
                case STAT_AGILITY: return 1.5f;
                case STAT_DEFENSE: return 2.0f;
                case STAT_DODGE: return 1.6f;
                case STAT_ARMOR: return 0.08f;
                case STAT_STRENGTH: return 0.8f;
                case STAT_HIT: return 0.6f;
                case STAT_EXPERTISE: return 0.9f;
                case STAT_ATTACK_POWER: return 0.3f;
                case STAT_FERAL_ATTACK_POWER: return 0.3f;
                default: return 0.0f;
            }
        case ROLE_AGILITY:
            switch (stat)
            {
                case STAT_AGILITY: return 2.2f;
                case STAT_STRENGTH: return 1.1f;
                case STAT_STAMINA: return 0.6f;
                case STAT_ATTACK_POWER: return 1.0f;
                case STAT_HIT: return 2.0f;
                case STAT_CRIT: return 1.5f;
                case STAT_HASTE: return 1.3f;
                case STAT_EXPERTISE: return 1.6f;
                case STAT_ARMOR_PEN: return 1.1f;
                case STAT_ARMOR: return 0.005f;
                case STAT_MELEE_DPS: return 9.0f;
                case STAT_RANGED_DPS: return 0.5f;
                default: return 0.0f;
            }
        case ROLE_CAT:
            switch (stat)
            {
                case STAT_AGILITY: return 2.3f;
                case STAT_STRENGTH: return 1.6f;
                case STAT_STAMINA: return 0.6f;
                case STAT_ATTACK_POWER: return 1.0f;
                case STAT_FERAL_ATTACK_POWER: return 1.0f;
                case STAT_HIT: return 1.8f;
                case STAT_CRIT: return 1.5f;
                case STAT_HASTE: return 0.8f;
                case STAT_EXPERTISE: return 1.5f;
                case STAT_ARMOR_PEN: return 1.1f;
                case STAT_ARMOR: return 0.005f;
                default: return 0.0f;
            }
        case ROLE_HUNTER:
            switch (stat)
            {
                case STAT_AGILITY: return 2.4f;
                case STAT_STRENGTH: return 0.1f;
                case STAT_STAMINA: return 0.6f;
                case STAT_INTELLECT: return 0.6f;
                case STAT_ATTACK_POWER: return 0.9f;
                case STAT_RANGED_ATTACK_POWER: return 1.0f;
                case STAT_HIT: return 2.0f;
                case STAT_CRIT: return 1.6f;
                case STAT_HASTE: return 1.1f;
                case STAT_ARMOR_PEN: return 1.1f;
                case STAT_ARMOR: return 0.005f;
                case STAT_MELEE_DPS: return 0.5f;
                case STAT_RANGED_DPS: return 12.0f;
                default: return 0.0f;
            }
        case ROLE_ENHANCEMENT:
            switch (stat)
            {
                case STAT_STRENGTH: return 1.6f;
                case STAT_AGILITY: return 1.6f;
                case STAT_STAMINA: return 0.6f;
                case STAT_INTELLECT: return 0.6f;
                case STAT_ATTACK_POWER: return 1.0f;
                case STAT_SPELL_POWER: return 0.4f;
                case STAT_HIT: return 1.8f;
                case STAT_CRIT: return 1.5f;
                case STAT_HASTE: return 1.3f;
                case STAT_EXPERTISE: return 1.4f;
                case STAT_ARMOR: return 0.005f;
                case STAT_MELEE_DPS: return 9.0f;
                default: return 0.0f;
            }
        case ROLE_CASTER:
            switch (stat)
            {
                case STAT_SPELL_POWER: return 1.0f;
                case STAT_INTELLECT: return 0.6f;
                case STAT_SPIRIT: return 0.3f;
                case STAT_STAMINA: return 0.3f;
                case STAT_HIT: return 1.4f;
                case STAT_CRIT: return 0.9f;
                case STAT_HASTE: return 1.0f;
                case STAT_MP5: return 0.6f;
                case STAT_RANGED_DPS: return 0.3f;
                default: return 0.0f;
            }
        case ROLE_HEALER:
            switch (stat)
            {
                case STAT_HEALING: return 1.0f;
                case STAT_SPELL_POWER: return 0.3f;
                case STAT_INTELLECT: return 1.0f;
                case STAT_SPIRIT: return 0.7f;
                case STAT_STAMINA: return 0.3f;
                case STAT_CRIT: return 0.5f;
                case STAT_HASTE: return 0.8f;
                case STAT_MP5: return 1.6f;
                case STAT_RANGED_DPS: return 0.1f;
                default: return 0.0f;
            }
        default:
            return 0.0f;
    }
}

// Ties go to the higher item level, so an item with only special effects (relics, some trinkets) still
// beats nothing and a newer item beats an older one.
constexpr float Score(Role role, Stats const& stats, uint32_t itemLevel)
{
    float score = 0.0f;
    for (uint8_t stat = 0; stat < STAT_COUNT; ++stat)
        score += Weight(role, Stat(stat)) * stats[stat];
    return score + itemLevel * 0.01f;
}

// An off-hand weapon swings with half the damage, so its weapon DPS counts half.
inline constexpr float OffHandDpsShare = 0.5f;
}  // namespace Boost

#endif
