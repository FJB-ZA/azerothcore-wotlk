/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <string>

#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "MythicPlus.h"
#include "Player.h"
#include "QueryResult.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

// Mythic+ character data (characters SQL 2026_10_24_00_mythic_plus.sql), bots' as well as players': the keystone,
// crests, weekly runs for the vault and best runs. Loaded at login and kept on the player; changes are written at once.
namespace MythicPlus
{
namespace
{
constexpr char const* DataKey = "MythicPlusPlayer";

struct PlayerMythicData : public DataMap::Base
{
    bool loaded = false;
    Keystone key;
    CrestWallet owned{};
    CrestWallet earned{};
    uint32 earnedWeek = 0;
};

uint32 CurrentWeek()
{
    return WeekIndex(uint64(GameTime::GetGameTime().count()));
}

void Load(Player* player, PlayerMythicData* data)
{
    uint32 const guid = player->GetGUID().GetCounter();
    uint32 const week = CurrentWeek();
    if (QueryResult result =
            CharacterDatabase.Query("SELECT map, level FROM mythic_plus_keystone WHERE guid = {}", guid))
    {
        Field* fields = result->Fetch();
        data->key = Keystone{fields[0].Get<uint16>(), fields[1].Get<uint8>()};
    }
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT tier, amount, week, earned_week FROM mythic_plus_crests WHERE guid = {}", guid))
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const tier = fields[0].Get<uint8>();
            if (!tier || tier > CrestTiers)
                continue;
            data->owned[tier - 1] = fields[1].Get<uint32>();
            if (fields[2].Get<uint32>() == week)
                data->earned[tier - 1] = fields[3].Get<uint32>();
        } while (result->NextRow());
    }
    data->earnedWeek = week;
    data->loaded = true;
}

PlayerMythicData* DataOf(Player* player)
{
    PlayerMythicData* data = player->CustomData.GetDefault<PlayerMythicData>(DataKey);
    if (!data->loaded)
        Load(player, data);
    // The weekly cap starts over at the reset
    if (uint32 const week = CurrentWeek(); data->earnedWeek != week)
    {
        data->earned = {};
        data->earnedWeek = week;
    }
    return data;
}

void SaveCrest(Player* player, PlayerMythicData const* data, uint8 tier)
{
    CharacterDatabase.Execute("REPLACE INTO mythic_plus_crests (guid, tier, amount, week, earned_week) VALUES "
                              "({}, {}, {}, {}, {})",
                              player->GetGUID().GetCounter(), tier, data->owned[tier - 1], data->earnedWeek,
                              data->earned[tier - 1]);
}

// The keystone item in the bags mirrors the key; it is only there to see and hold
void SyncKeystoneItem(Player* player, Keystone const& key)
{
    bool const has = player->HasItemCount(KeystoneItem, 1, true);
    if (key.map && !has)
    {
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, KeystoneItem, 1) == EQUIP_ERR_OK)
            if (Item* item = player->StoreNewItem(dest, KeystoneItem, true))
                player->SendNewItem(item, 1, true, false);
    }
    else if (!key.map && has)
        player->DestroyItemCount(KeystoneItem, 1, true);
}
}

Keystone GetKeystone(Player* player)
{
    return DataOf(player)->key;
}

void SetKeystone(Player* player, uint16 map, uint8 level)
{
    PlayerMythicData* data = DataOf(player);
    data->key = map ? Keystone{map, ClampLevel(level)} : Keystone{};
    uint32 const guid = player->GetGUID().GetCounter();
    if (map)
        CharacterDatabase.Execute("REPLACE INTO mythic_plus_keystone (guid, map, level) VALUES ({}, {}, {})", guid, map,
                                  data->key.level);
    else
        CharacterDatabase.Execute("DELETE FROM mythic_plus_keystone WHERE guid = {}", guid);
    SyncKeystoneItem(player, data->key);
    SendKeystone(player);
}

CrestWallet Crests(Player* player)
{
    return DataOf(player)->owned;
}

CrestWallet EarnedThisWeek(Player* player)
{
    return DataOf(player)->earned;
}

uint32 AwardCrests(Player* player, Crest crest, uint32 amount)
{
    if (crest == Crest::None)
        return 0;
    PlayerMythicData* data = DataOf(player);
    uint8 const tier = uint8(crest);
    uint32 const added = CappedAward(amount, data->earned[tier - 1]);
    ChatHandler handler(player->GetSession());
    if (!added)
    {
        handler.SendSysMessage(std::string("You have earned this week's ") + CrestName(crest) + " Crests already.");
        return 0;
    }
    data->owned[tier - 1] += added;
    data->earned[tier - 1] += added;
    SaveCrest(player, data, tier);
    handler.SendSysMessage("You receive " + std::to_string(added) + " " + CrestName(crest) + " Crests.");
    SendCrests(player);
    return added;
}

Crest PayCrests(Player* player, uint8 rank)
{
    PlayerMythicData* data = DataOf(player);
    CrestWallet wallet = data->owned;
    Crest const paid = PayForRank(wallet, rank);
    if (paid == Crest::None)
        return paid;
    data->owned = wallet;
    SaveCrest(player, data, uint8(paid));
    SendCrests(player);
    return paid;
}

void RecordRun(Player* player, uint16 map, uint8 level, uint32 seconds, uint8 upgrade)
{
    uint32 const guid = player->GetGUID().GetCounter();
    uint32 const week = CurrentWeek();
    CharacterDatabase.Execute("INSERT INTO mythic_plus_week (guid, week, runs, best_level, vault_claimed) VALUES "
                              "({}, {}, 1, {}, 0) ON DUPLICATE KEY UPDATE runs = runs + 1, "
                              "best_level = GREATEST(best_level, VALUES(best_level))",
                              guid, week, level);
    CharacterDatabase.Execute("INSERT IGNORE INTO mythic_plus_week_maps (guid, week, map) VALUES ({}, {}, {})", guid,
                              week, map);
    // A higher key, or the same key faster, replaces the best run; `level` is assigned last because MySQL applies
    // the assignments in order
    CharacterDatabase.Execute(
        "INSERT INTO mythic_plus_best (guid, map, level, seconds, upgrade, completed) VALUES ({}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "seconds = IF(VALUES(level) > level OR (VALUES(level) = level AND VALUES(seconds) < seconds), "
        "VALUES(seconds), seconds), "
        "upgrade = IF(VALUES(level) > level OR (VALUES(level) = level AND VALUES(seconds) = seconds), "
        "VALUES(upgrade), upgrade), "
        "completed = IF(VALUES(level) > level OR (VALUES(level) = level AND VALUES(seconds) = seconds), "
        "VALUES(completed), completed), "
        "level = GREATEST(level, VALUES(level))",
        guid, map, level, seconds, upgrade, uint32(GameTime::GetGameTime().count()));
}

std::optional<VaultReward> PendingVault(Player* player)
{
    uint32 const week = CurrentWeek();
    if (!week)
        return std::nullopt;
    uint32 const guid = player->GetGUID().GetCounter();
    // Like Legion's, the vault holds last week's best and is gone once a newer week ends
    QueryResult result = CharacterDatabase.Query(
        "SELECT best_level FROM mythic_plus_week WHERE guid = {} AND week = {} AND runs > 0 AND vault_claimed = 0",
        guid, week - 1);
    if (!result)
        return std::nullopt;
    VaultReward reward;
    reward.week = week - 1;
    reward.bestLevel = result->Fetch()[0].Get<uint8>();
    if (QueryResult maps = CharacterDatabase.Query(
            "SELECT map FROM mythic_plus_week_maps WHERE guid = {} AND week = {}", guid, week - 1))
    {
        do
            reward.maps.push_back(maps->Fetch()[0].Get<uint16>());
        while (maps->NextRow());
    }
    return reward;
}

void ClaimVault(Player* player, uint32 week)
{
    CharacterDatabase.Execute("UPDATE mythic_plus_week SET vault_claimed = 1 WHERE guid = {} AND week = {}",
                              player->GetGUID().GetCounter(), week);
}
}

using namespace MythicPlus;

class MythicStoragePlayerScript : public PlayerScript
{
public:
    MythicStoragePlayerScript()
        : PlayerScript("MythicStoragePlayerScript", {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_DELETE})
    {
    }

    // Loads the data here on the world thread, so runs on map threads never query; a bot does its Dalaran errands
    void OnPlayerLogin(Player* player) override
    {
        SyncKeystoneItem(player, GetKeystone(player));
        if (!IsBot(player))
            return;
        if (player->GetLevel() >= HeroicLevel && !GetKeystone(player).map)
            SetKeystone(player, Dungeons[urand(0, uint32(Dungeons.size() - 1))].map, MinKeyLevel);
        OpenVault(player);
        SpendCrestsOnGear(player);
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        uint32 const counter = guid.GetCounter();
        CharacterDatabase.Execute("DELETE FROM mythic_plus_keystone WHERE guid = {}", counter);
        CharacterDatabase.Execute("DELETE FROM mythic_plus_crests WHERE guid = {}", counter);
        CharacterDatabase.Execute("DELETE FROM mythic_plus_week WHERE guid = {}", counter);
        CharacterDatabase.Execute("DELETE FROM mythic_plus_week_maps WHERE guid = {}", counter);
        CharacterDatabase.Execute("DELETE FROM mythic_plus_best WHERE guid = {}", counter);
    }
};

void AddSC_mythic_storage()
{
    new MythicStoragePlayerScript();
}
