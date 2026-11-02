/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "DatabaseEnv.h"
#include "MythicItemPolicy.h"
#include "MythicPlus.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "ScriptMgr.h"

// The Great Vault (MythicVaultPolicy.h), as retail's: rows Raids, Dungeons and World of three slots, unlocked by the
// week's activity. The week after, each unlocked slot offers one item, rolled the first time the vault is looked at
// and kept in mythic_plus_vault_option (characters SQL 2026_11_02_00_mythic_plus_vault.sql); the player takes one of
// them at the Keystone Master. The Dungeons row counts the week's keystones (mythic_plus_runs); Raids and World stay
// locked until they count anything (mythic_plus_vault_activity).
namespace MythicPlus
{
namespace
{
constexpr char const* DataKey = "MythicPlusVault";

// The week the player is known to have chosen in, so a second choice is refused without asking the database, and the
// week whose options the player was last shown, so a choice from before the reset is refused
struct VaultData : public DataMap::Base
{
    bool chosen = false;
    uint32 chosenWeek = 0;
    uint32 shownWeek = VaultNotShown;
};

VaultData* DataOf(Player* player)
{
    return player->CustomData.GetDefault<VaultData>(DataKey);
}

// A week's keystones: how many there were, and each Dungeons slot's key level (0 while locked)
struct DungeonWeek
{
    uint32 runs = 0;
    std::array<uint16, VaultSlots> levels{};
};

DungeonWeek ReadDungeonWeek(Player* player, uint32 week)
{
    std::vector<uint16> levels;
    if (week == CurrentWeek())
    {
        for (RunRecord const& run : WeekRuns(player))
            levels.push_back(run.level);
    }
    else if (QueryResult result = CharacterDatabase.Query(
                 "SELECT level FROM mythic_plus_runs WHERE guid = {} AND week = {}", player->GetGUID().GetCounter(),
                 week))
    {
        do
            levels.push_back(result->Fetch()[0].Get<uint8>());
        while (result->NextRow());
    }
    DungeonWeek dungeons;
    dungeons.runs = uint32(levels.size());
    dungeons.levels = NthBest(std::move(levels), VaultRow::Dungeon);
    return dungeons;
}

// A week whose vault was opened the old way, a single item, counts as chosen too
bool Chosen(Player* player, uint32 week)
{
    VaultData* data = DataOf(player);
    if (data->chosen && data->chosenWeek == week)
        return true;
    uint32 const guid = player->GetGUID().GetCounter();
    bool const chosen =
        CharacterDatabase.Query("SELECT 1 FROM mythic_plus_vault_option WHERE guid = {} AND week = {} AND chosen = 1 "
                                "LIMIT 1",
                                guid, week) ||
        CharacterDatabase.Query(
            "SELECT 1 FROM mythic_plus_week WHERE guid = {} AND week = {} AND vault_claimed = 1", guid, week);
    if (chosen)
    {
        data->chosen = true;
        data->chosenWeek = week;
    }
    return chosen;
}

std::vector<uint16> WeekMaps(uint32 guid, uint32 week)
{
    std::vector<uint16> maps;
    if (QueryResult result =
            CharacterDatabase.Query("SELECT map FROM mythic_plus_week_maps WHERE guid = {} AND week = {}", guid, week))
    {
        do
            maps.push_back(result->Fetch()[0].Get<uint16>());
        while (result->NextRow());
    }
    return maps;
}

std::vector<VaultOption> StoredOptions(uint32 guid, uint32 week)
{
    std::vector<VaultOption> options;
    if (QueryResult result = CharacterDatabase.Query("SELECT `row`, slot, item FROM mythic_plus_vault_option "
                                                     "WHERE guid = {} AND week = {} ORDER BY `row`, slot",
                                                     guid, week))
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const row = fields[0].Get<uint8>();
            uint8 const slot = fields[1].Get<uint8>();
            if (row >= 1 && row <= VaultRows && slot >= 1 && slot <= VaultSlots)
                options.push_back(VaultOption{VaultRow(row), slot, fields[2].Get<uint32>()});
        } while (result->NextRow());
    }
    return options;
}

bool HasOption(std::vector<VaultOption> const& options, VaultRow row, uint8 slot)
{
    return std::any_of(options.begin(), options.end(),
                       [&](VaultOption const& option) { return option.row == row && option.slot == slot; });
}

uint32 ItemLevelOf(uint32 itemId)
{
    ItemTemplate const* item = sObjectMgr->GetItemTemplate(itemId);
    return item ? item->ItemLevel : 0;
}

// Raids and World count nothing yet, so only a keystone unlocks a slot; the best key fills slot 1 first
bool AnyUnlocked(DungeonWeek const& dungeons)
{
    return dungeons.levels[0] != 0;
}

// `week`'s options, whether or not one was chosen: each unlocked slot without one yet rolls it, from the week's
// dungeons at its key's rank
std::vector<VaultOption> RollOptions(Player* player, uint32 week, DungeonWeek const& dungeons)
{
    uint32 const guid = player->GetGUID().GetCounter();
    std::vector<VaultOption> options = StoredOptions(guid, week);
    std::vector<uint16> maps;
    bool rolled = false;
    for (uint8 slot = 1; slot <= VaultSlots; ++slot)
    {
        uint8 const level = uint8(dungeons.levels[slot - 1]);
        if (!level || HasOption(options, VaultRow::Dungeon, slot))
            continue;
        if (maps.empty())
            maps = WeekMaps(guid, week);
        if (uint32 const item = RollItemAtRank(player, maps, VaultRankForKey(level)))
        {
            // IGNORE: a roll stored first stays, so the options never change once seen
            CharacterDatabase.DirectExecute("INSERT IGNORE INTO mythic_plus_vault_option (guid, week, `row`, slot, "
                                            "item, chosen) VALUES ({}, {}, {}, {}, {}, 0)",
                                            guid, week, uint32(VaultRow::Dungeon), slot, item);
            rolled = true;
        }
    }
    if (rolled)
        options = StoredOptions(guid, week);
    std::stable_sort(options.begin(), options.end(), [](VaultOption const& a, VaultOption const& b)
                     { return ItemLevelOf(a.item) > ItemLevelOf(b.item); });
    return options;
}

// `week`'s options while none is chosen; nothing is asked of the database for a week with no slot unlocked
std::vector<VaultOption> OptionsFor(Player* player, uint32 week, DungeonWeek const& dungeons)
{
    if (!AnyUnlocked(dungeons) || Chosen(player, week))
        return {};
    return RollOptions(player, week, dungeons);
}
}

std::vector<VaultOption> VaultOptions(Player* player, bool shown)
{
    uint32 const week = CurrentWeek();
    if (!week)
        return {};
    std::vector<VaultOption> options = OptionsFor(player, week - 1, ReadDungeonWeek(player, week - 1));
    if (shown && !options.empty())
        DataOf(player)->shownWeek = week - 1;
    return options;
}

std::string ClaimOption(Player* player, VaultRow row, uint8 slot)
{
    if (!IsBot(player) && !player->FindNearestCreature(KeystoneMasterEntry, ForgeRange))
        return "Visit the Great Vault in Dalaran to choose your reward.";
    uint32 const week = CurrentWeek();
    DungeonWeek const dungeons = week ? ReadDungeonWeek(player, week - 1) : DungeonWeek{};
    if (!AnyUnlocked(dungeons))
        return "The Great Vault holds nothing for you this week.";
    if (Chosen(player, week - 1))
        return "You have already chosen this week's reward.";
    VaultData* data = DataOf(player);
    // Chosen from a window or gossip list of before the reset: this week's options are shown instead. Bots choose
    // without being shown.
    if (!IsBot(player) && VaultChoiceStale(data->shownWeek, week - 1))
    {
        SendVault(player);
        return "The Great Vault has changed. Choose again.";
    }
    std::vector<VaultOption> const options = RollOptions(player, week - 1, dungeons);
    auto const option = std::find_if(options.begin(), options.end(), [&](VaultOption const& candidate)
                                     { return candidate.row == row && candidate.slot == slot; });
    if (option == options.end())
        return "That slot of the Great Vault holds no reward this week.";
    // Closed in memory and at once in the database before the item is given, so a second choice gets nothing
    data->chosen = true;
    data->chosenWeek = week - 1;
    CharacterDatabase.DirectExecute("UPDATE mythic_plus_vault_option SET chosen = 1 WHERE guid = {} AND week = {} AND "
                                    "`row` = {} AND slot = {}",
                                    player->GetGUID().GetCounter(), week - 1, uint32(row), slot);
    GiveItem(player, option->item);
    SendVault(player);
    return {};
}

void SendVault(Player* player)
{
    if (IsBot(player))
        return;
    uint32 const week = CurrentWeek();
    DungeonWeek const last = week ? ReadDungeonWeek(player, week - 1) : DungeonWeek{};
    std::vector<VaultOption> const options = week ? OptionsFor(player, week - 1, last) : std::vector<VaultOption>{};
    if (!options.empty())
        DataOf(player)->shownWeek = week - 1;
    // While options wait the rows show the week they came from, as retail's; once one is chosen, this week's
    // progress, from the runs kept in memory
    DungeonWeek const dungeons = options.empty() ? ReadDungeonWeek(player, week) : last;
    SendToClient(player, FormatVault(!options.empty()));
    for (VaultOption const& option : options)
        SendToClient(player, FormatVaultOption(option.row, option.slot, option.item));
    for (uint8 index = 1; index <= VaultRows; ++index)
    {
        VaultRow const row = VaultRow(index);
        // Raids and World count nothing yet, so their slots stay locked
        bool const counted = row == VaultRow::Dungeon;
        for (uint8 slot = 1; slot <= VaultSlots; ++slot)
        {
            uint8 const level = counted ? uint8(dungeons.levels[slot - 1]) : 0;
            uint32 const itemLevel = level ? uint32(ItemLevelForRank(VaultRankForKey(level))) : 0u;
            SendToClient(player, FormatVaultSlot(row, slot, counted ? dungeons.runs : 0u, VaultThreshold(row, slot),
                                                 itemLevel));
        }
    }
}
}

using namespace MythicPlus;

class MythicVaultPlayerScript : public PlayerScript
{
public:
    MythicVaultPlayerScript() : PlayerScript("MythicVaultPlayerScript", {PLAYERHOOK_ON_DELETE}) { }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        uint32 const counter = guid.GetCounter();
        CharacterDatabase.Execute("DELETE FROM mythic_plus_vault_activity WHERE guid = {}", counter);
        CharacterDatabase.Execute("DELETE FROM mythic_plus_vault_option WHERE guid = {}", counter);
    }
};

void AddSC_mythic_vault()
{
    new MythicVaultPlayerScript();
}
