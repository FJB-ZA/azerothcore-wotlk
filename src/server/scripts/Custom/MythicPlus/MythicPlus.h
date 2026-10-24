/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _MYTHIC_PLUS_H
#define _MYTHIC_PLUS_H

#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "DataMap.h"
#include "Define.h"
#include "MythicAffixPolicy.h"
#include "MythicCrestPolicy.h"
#include "MythicDungeonTable.h"
#include "MythicKeystonePolicy.h"
#include "ObjectGuid.h"

class Creature;
class Map;
class Player;
class Unit;
class WorldObject;
struct Position;

// Mythic+ engine (solo-wow): the run each instance map carries and the calls between the Mythic+ scripts. The rules
// live in the policy headers next to this one.
namespace MythicPlus
{
// Creatures, gameobjects and the keystone item (world SQL 2026_10_24_00_mythic_plus.sql)
inline constexpr uint32 KeystoneMasterEntry = 990920;
inline constexpr uint32 ExplosiveOrbEntry = 990921;
inline constexpr uint32 ServantEntry = 990922;
inline constexpr uint32 RuneEntry = 990923;
inline constexpr uint32 FontOfPowerEntry = 990920;
inline constexpr uint32 CacheEntry = 990921;
inline constexpr uint32 KeystoneItem = 990300;

// Spells (solo-wow patches/client/spell-data/40-mythic-plus.json)
inline constexpr uint32 SpellShatteringGround = 990300;
inline constexpr uint32 SpellVolcanicPlume = 990301;
inline constexpr uint32 SpellCreepingShadow = 990302;
inline constexpr uint32 SpellSanguineIchor = 990303;
inline constexpr uint32 SpellBolstered = 990304;
inline constexpr uint32 SpellRaging = 990305;
inline constexpr uint32 SpellNecroticRot = 990306;
inline constexpr uint32 SpellGrievousWound = 990307;
inline constexpr uint32 SpellBurst = 990308;
inline constexpr uint32 SpellExplosion = 990309;
inline constexpr uint32 SpellQuake = 990310;
inline constexpr uint32 SpellShadowNova = 990312;
inline constexpr uint32 SpellEmpowered = 990313;

// How long each ground spell lasts, so the rune casting it outlives it
inline constexpr uint32 CircleMs = 3500;
inline constexpr uint32 PoolMs = 20000;

struct MythicRun : public DataMap::Base
{
    RunState state = RunState::None;
    DungeonInfo const* dungeon = nullptr;
    uint8 level = 0;
    uint32 week = 0;
    WeekAffixes affixes{};
    ObjectGuid keyOwner;
    uint32 countdownMs = 0;
    // Time in the run, without death penalties
    uint64 elapsedMs = 0;
    uint32 deaths = 0;
    uint32 forces = 0;
    uint32 quota = 0;
    uint8 bosses = 0;
    // The dungeon's last encounter is down; the boss count alone could be met by an optional boss
    bool finalBoss = false;
    // Spawns that count as enemy forces, those already counted, and those that bring a Teeming copy
    std::unordered_set<ObjectGuid::LowType> forceSpawns;
    std::unordered_set<ObjectGuid::LowType> counted;
    std::unordered_set<ObjectGuid::LowType> teeming;
    // Real players and bots present when the key went in
    std::set<ObjectGuid> participants;
    uint32 snapshotMs = 0;
    uint64 finalMs = 0;
    uint8 upgrade = 0;
    std::set<ObjectGuid> looted;

    [[nodiscard]] bool Active() const { return state == RunState::Running || state == RunState::Overtime; }
    [[nodiscard]] uint64 TotalMs() const { return elapsedMs + PenaltyMs(deaths); }
    [[nodiscard]] uint64 LimitMs() const { return dungeon ? uint64(dungeon->timerSeconds) * 1000 : 0; }
    [[nodiscard]] bool Has(Affix affix) const { return IsActive(affixes, level, affix); }
};

struct Keystone
{
    uint16 map = 0;
    uint8 level = 0;
};

// mythic_run.cpp
MythicRun* FindRun(Map* map);
// Only a run whose timer is going (running or overtime)
MythicRun* ActiveRun(Map* map);
// Bot sessions have no client socket, and the core names them "headless"
bool IsBot(Player* player);
bool IsBoss(Creature* creature);
// Hostile creatures the affixes act on (not pets, triggers, orbs or friendly NPCs), dead or alive
bool IsEnemyKind(Creature* creature);
// A living one
bool IsMythicEnemy(Creature* creature);
void CheckCompletion(Map* map);
uint8 PlayersIn(Map* map);

// mythic_scaling.cpp
void ScaleCreature(Creature* creature, MythicRun const& run);
void ScaleMap(Map* map, MythicRun const& run);

// mythic_affix_teeming.cpp: copies count as enemy forces like the spawn they came with
bool IsTeemingCopy(Creature* creature);
// The chosen spawns already in the world when the key goes in
void BringTeemingCopies(Map* map, MythicRun& run);

// mythic_affix_explosive.cpp: an orb that hits every player when its fuse runs out, unless killed first
Creature* SpawnOrb(Unit* source, Position const& pos);

// mythic_ground.cpp: a rune at `pos` casts a ground spell for `damage` (the spell's first effect) and stays for
// `lifeMs`; it takes `source`'s faction, so Sanguine heals the source's allies
Creature* CastGround(Unit* source, Position const& pos, uint32 spellId, uint32 damage, uint32 lifeMs);
// Living Mythic+ enemies within `range` of `center`, not counting `center`
std::vector<Creature*> EnemiesNear(WorldObject* center, float range);
// Players and bots in the run, alive, optionally only those in combat
std::vector<Player*> RunPlayers(Map* map, bool inCombatOnly);

// mythic_storage.cpp
Keystone GetKeystone(Player* player);
// Map 0 takes the keystone away
void SetKeystone(Player* player, uint16 map, uint8 level);
CrestWallet Crests(Player* player);
CrestWallet EarnedThisWeek(Player* player);
// Adds up to the weekly cap and returns what was added
uint32 AwardCrests(Player* player, Crest crest, uint32 amount);
// Pays for raising an item to `rank`; returns the tier paid with, or None
Crest PayCrests(Player* player, uint8 rank);
void RecordRun(Player* player, uint16 map, uint8 level, uint32 seconds, uint8 upgrade);
struct VaultReward
{
    uint32 week = 0;
    uint8 bestLevel = 0;
    std::vector<uint16> maps;
};
// The newest finished week with runs whose vault is still closed
std::optional<VaultReward> PendingVault(Player* player);
void ClaimVault(Player* player, uint32 week);

// mythic_items.cpp
bool IsMythicItem(uint32 itemId);
// A Mythic 0 item from `map`'s chest that `player` can use, or 0
uint32 RollItem(Player* player, uint16 map);
uint32 RollItem(Player* player, std::vector<uint16> const& maps);
// Into the bags, or the mailbox when they're full
void GiveItem(Player* player, uint32 itemId);
// Raises the item at bag/slot one rank; returns why it didn't, or empty
std::string UpgradeItem(Player* player, uint8 bag, uint8 slot);
// The item's next rank cost, for the forge's text: empty when it can't go higher
std::string UpgradeCostText(uint32 itemId);
// Takes last week's vault item; returns why there is none, or empty
std::string OpenVault(Player* player);
// What a bot does at the forge: raises its equipped Mythic+ items, lowest rank first, while its crests pay
void SpendCrestsOnGear(Player* player);

// mythic_addon.cpp
void SendKeystone(Player* player);
void SendCrests(Player* player);
void SendWeek(Player* player);
void SendRun(Player* player, MythicRun const& run);
void SendResult(Player* player, MythicRun const& run, Keystone const& newKey, Crest crest, uint32 crests);
}

#endif
