/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _SUBCLASS_H
#define _SUBCLASS_H

#include <string>
#include <utility>
#include <vector>

#include "Define.h"
#include "SubclassPolicy.h"

class Player;
class Unit;

// Sub-class paths, implemented in subclass.cpp, for the other solo-wow scripts (spec slots, the path trainers).

// The path a talent group is on; 0 is the plain class
uint8 Subclass_GetPath(Player* player, uint8 group);
std::string Subclass_PathName(uint8 pathId);
// The paths the player's class can take
std::vector<uint8> Subclass_PathsFor(Player* player);
uint8 Subclass_UnlockLevel(uint8 pathId);
// The fee for the player's next path change, in copper
uint32 Subclass_NextFee(Player* player);
// Puts the active talent group on `target` (0: the plain class), charging the fee when `charge` is set
Subclass::ChangeCheck Subclass_ChangePath(Player* player, uint8 target, bool charge);
// The mirror of `spellId` on the player's active path, or 0
uint32 Subclass_MirrorOf(Player* player, uint32 spellId);
// The original behind a mirror the caster casts, for school bonuses; any other spell is returned as is
uint32 Subclass_BonusSpell(Unit const* caster, uint32 spellId);
// A path's spell swaps: original, replacement
std::vector<std::pair<uint32, uint32>> Subclass_AllSwaps(uint8 pathId);

#endif
