/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _SUBCLASS_PATHS_H
#define _SUBCLASS_PATHS_H

#include <string>
#include <vector>

#include "Define.h"
#include "SubclassPathPolicy.h"

class Player;
class Unit;

// Sub-class path data shared by the subclass_*.cpp scripts. Loaded by subclass_paths.cpp from the world tables
// subclass_path, subclass_spell_swap and subclass_creature_swap; a path's talent tree comes from talenttab_dbc.
namespace SubclassPaths
{
struct SpellSwap
{
    uint32 original;
    uint32 replacement;
    uint8 rank;
};

struct Path
{
    uint16 id;
    std::string name;
    PathRules rules;
    uint32 unlockQuest = 0;  // rewarded quest a character needs before choosing the path, 0 for none
    uint32 talentTab = 0;    // the path's own talent tree (TalentTab.dbc), open only on the path; 0 for none
    float botShare = 0.0f;   // share of the class's random bots that take the path once they meet its rules
    Modifiers modifiers;
    std::vector<SpellSwap> swaps;  // highest rank first, because removing a rank also removes the ranks above it
};

struct CreatureSwap
{
    uint16 pathId;
    uint32 original;
    uint32 replacement;
    uint8 bodies;  // the pet plus bodies - 1 guardians, sharing the original's health and damage
};

// The path the player's active spec slot follows, or nullptr for the plain class
Path const* ActivePath(Player const* player);

// The active path's replacement for an original spell, or 0
uint32 ActiveReplacement(Player const* player, uint32 originalSpellId);

// The original spell an active-path replacement stands for, or 0
uint32 ActiveOriginal(Player const* player, uint32 replacementSpellId);

// The swap whose replacement creature is `entry`, or nullptr
CreatureSwap const* SwapForReplacementCreature(uint32 entry);

// True when some path changes damage or healing, or splits a pet into bodies; combat hooks skip work otherwise
bool AffectsCombat();

// The loaded paths of a class, by id
std::vector<Path const*> ClassPaths(uint8 classId);

// The path id the player's active spec slot follows, PlainClass for none
uint16 ActivePathId(Player const* player);

// What the player's next path change costs, in copper
uint32 NextChangeFee(Player* player);

// Moves the active slot to `pathId` (PlainClass to leave its path), charging the fee; returns why it refused, or empty
std::string ChangeActivePath(Player* player, uint16 pathId);

// How many bodies share this minion's health and melee: the pet and the extra bodies summoned with it, or 1
uint8 MeleeBodies(Unit const* minion);
}

#endif
