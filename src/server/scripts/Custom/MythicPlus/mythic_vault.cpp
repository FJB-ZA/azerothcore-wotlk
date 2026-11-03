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
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "Map.h"
#include "MythicItemPolicy.h"
#include "MythicPlus.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "ScriptMgr.h"

// The Great Vault (MythicVaultPolicy.h), as retail's: rows Raids, Dungeons and World of three slots, unlocked by the
// week's activity. The week after, each unlocked slot offers one item, rolled the first time the vault is looked at
// and kept in mythic_plus_vault_option (characters SQL 2026_11_02_00_mythic_plus_vault.sql); the player takes one of
// them at the Keystone Master. The Dungeons row counts the week's keystones (mythic_plus_runs); the Raids row the
// week's Wrath raid bosses, each once (mythic_plus_vault_activity), and gives a Raid 0 item from the raid and
// difficulty of its slot's kill. The World row counts the week's heroic dungeons cleared without a key, each clear
// once (the same dungeon cleared twice counts twice), and gives a Mythic 0 item from their loot.
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

// A week's raid bosses: how many, each Raids slot's item level (0 while locked), and each counted kill's raid and
// difficulty
struct RaidWeek
{
    uint32 kills = 0;
    std::array<uint16, VaultSlots> levels{};
    std::vector<RaidLoot> loot;
};

RaidWeek ReadRaidWeek(Player* player, uint32 week)
{
    std::vector<RaidKill> rows;
    if (QueryResult result = CharacterDatabase.Query("SELECT map, difficulty, ref FROM mythic_plus_vault_activity "
                                                     "WHERE guid = {} AND week = {} AND `row` = {}",
                                                     player->GetGUID().GetCounter(), week, uint32(VaultRow::Raid)))
    {
        do
        {
            Field* fields = result->Fetch();
            rows.push_back(RaidKill{fields[0].Get<uint16>(), fields[1].Get<uint8>(), fields[2].Get<uint32>()});
        } while (result->NextRow());
    }
    // A row per boss and difficulty; the row counts each boss once, at its best difficulty
    RaidWeek raids;
    std::vector<uint16> levels;
    for (RaidKill const& kill : BestKillPerBoss(rows))
    {
        levels.push_back(RaidItemLevel(kill.map, kill.difficulty));
        raids.loot.push_back(RaidLoot{kill.map, kill.difficulty});
    }
    raids.kills = uint32(levels.size());
    raids.levels = NthBest(std::move(levels), VaultRow::Raid);
    return raids;
}

// The raids and difficulties of the week's kills that give `itemLevel`, a Raids slot's loot: the slot's kill and any
// other kill tied with it
std::vector<RaidLoot> LootAt(RaidWeek const& raids, uint16 itemLevel)
{
    std::vector<RaidLoot> loot;
    for (RaidLoot const& kill : raids.loot)
        if (RaidItemLevel(kill.map, kill.difficulty) == itemLevel &&
            std::none_of(loot.begin(), loot.end(), [&](RaidLoot const& known)
                         { return known.map == kill.map && known.difficulty == kill.difficulty; }))
            loot.push_back(kill);
    return loot;
}

// A week's heroic dungeons cleared without a key: how many, each World slot's item level (0 while locked), and the
// dungeons, whose loot the slots roll from
struct WorldWeek
{
    uint32 completed = 0;
    std::array<uint16, VaultSlots> levels{};
    std::vector<uint16> maps;
};

WorldWeek ReadWorldWeek(Player* player, uint32 week)
{
    WorldWeek world;
    if (QueryResult result = CharacterDatabase.Query("SELECT map FROM mythic_plus_vault_activity "
                                                     "WHERE guid = {} AND week = {} AND `row` = {}",
                                                     player->GetGUID().GetCounter(), week, uint32(VaultRow::World)))
    {
        do
        {
            // A row per clear (RecordWorldDungeon), so the same dungeon twice counts twice but is one place to take
            // loot from
            uint16 const map = result->Fetch()[0].Get<uint16>();
            ++world.completed;
            if (std::find(world.maps.begin(), world.maps.end(), map) == world.maps.end())
                world.maps.push_back(map);
        } while (result->NextRow());
    }
    for (uint8 slot = 1; slot <= VaultSlots; ++slot)
        if (world.completed >= VaultThreshold(VaultRow::World, slot))
            world.levels[slot - 1] = ItemLevelForRank(WorldRank);
    return world;
}

struct VaultWeek
{
    DungeonWeek dungeons;
    RaidWeek raids;
    WorldWeek world;
};

VaultWeek ReadWeek(Player* player, uint32 week)
{
    return VaultWeek{ReadDungeonWeek(player, week), ReadRaidWeek(player, week), ReadWorldWeek(player, week)};
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

// A keystone, a raid boss or two heroic dungeons unlocks a slot; the best fills slot 1 first
bool AnyUnlocked(VaultWeek const& vault)
{
    return vault.dungeons.levels[0] != 0 || vault.raids.levels[0] != 0 || vault.world.levels[0] != 0;
}

void StoreOption(uint32 guid, uint32 week, VaultRow row, uint8 slot, uint32 item)
{
    // IGNORE: a roll stored first stays, so the options never change once seen
    CharacterDatabase.DirectExecute("INSERT IGNORE INTO mythic_plus_vault_option (guid, week, `row`, slot, item, "
                                    "chosen) VALUES ({}, {}, {}, {}, {}, 0)",
                                    guid, week, uint32(row), slot, item);
}

// `week`'s options, whether or not one was chosen: each unlocked slot without one yet rolls it, a Dungeons slot from
// the week's dungeons at its key's rank, a Raids slot from its kill's raid and difficulty at Raid 0, a World slot from
// the heroics cleared at Mythic 0
std::vector<VaultOption> RollOptions(Player* player, uint32 week, VaultWeek const& vault)
{
    uint32 const guid = player->GetGUID().GetCounter();
    std::vector<VaultOption> options = StoredOptions(guid, week);
    std::vector<uint16> maps;
    bool rolled = false;
    for (uint8 slot = 1; slot <= VaultSlots; ++slot)
    {
        uint8 const level = uint8(vault.dungeons.levels[slot - 1]);
        if (!level || HasOption(options, VaultRow::Dungeon, slot))
            continue;
        if (maps.empty())
            maps = WeekMaps(guid, week);
        if (uint32 const item = RollItemAtRank(player, maps, VaultRankForKey(level)))
        {
            StoreOption(guid, week, VaultRow::Dungeon, slot, item);
            rolled = true;
        }
    }
    for (uint8 slot = 1; slot <= VaultSlots; ++slot)
    {
        uint16 const itemLevel = vault.raids.levels[slot - 1];
        if (!itemLevel || HasOption(options, VaultRow::Raid, slot))
            continue;
        if (uint32 const item = RollRaidItem(player, LootAt(vault.raids, itemLevel), itemLevel))
        {
            StoreOption(guid, week, VaultRow::Raid, slot, item);
            rolled = true;
        }
    }
    for (uint8 slot = 1; slot <= VaultSlots; ++slot)
    {
        if (!vault.world.levels[slot - 1] || HasOption(options, VaultRow::World, slot))
            continue;
        if (uint32 const item = RollItemAtRank(player, vault.world.maps, WorldRank))
        {
            StoreOption(guid, week, VaultRow::World, slot, item);
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
std::vector<VaultOption> OptionsFor(Player* player, uint32 week, VaultWeek const& vault)
{
    if (!AnyUnlocked(vault) || Chosen(player, week))
        return {};
    return RollOptions(player, week, vault);
}

// Whether `player` is bound to `map`'s instance (on its difficulty) though not in it: released and running back, say
bool BoundTo(Player* player, Map* map)
{
    InstancePlayerBind const* bind =
        sInstanceSaveMgr->PlayerGetBoundInstance(player->GetGUID(), map->GetId(), map->GetDifficulty());
    return bind && bind->save && bind->save->GetInstanceId() == map->GetInstanceId();
}

// Everyone a boss in `map` credits, game masters aside: the players in it, and their group's members bound to the
// instance but outside it, so a player whose bots finish the boss while they run back still gets it. Each once.
void ForEachCredited(Map* map, std::function<void(Player*)> const& credit)
{
    std::vector<ObjectGuid> credited;
    auto const give = [&](Player* player)
    {
        if (player->IsGameMaster() ||
            std::find(credited.begin(), credited.end(), player->GetGUID()) != credited.end())
            return;
        credited.push_back(player->GetGUID());
        credit(player);
    };
    std::vector<Group*> groups;
    map->DoForAllPlayers([&](Player* player)
    {
        give(player);
        if (Group* group = player->GetGroup(); group && std::find(groups.begin(), groups.end(), group) == groups.end())
            groups.push_back(group);
    });
    for (Group* group : groups)
        for (GroupReference* member = group->GetFirstMember(); member; member = member->next())
            if (Player* player = member->GetSource(); player && player->GetMap() != map && BoundTo(player, map))
                give(player);
}

// A Wrath raid boss's kill, for everyone it credits (ForEachCredited): a row per boss and difficulty a week, which
// ReadRaidWeek counts once per boss
void RecordRaidBoss(Map* map, uint32 creditEntry)
{
    uint32 const week = CurrentWeek();
    uint16 const mapId = uint16(map->GetId());
    uint8 const difficulty = uint8(map->GetDifficulty());
    // A difficulty the vault gives nothing for, such as the 40-man Naxxramas (533, difficulty 2)
    if (!RaidItemLevel(mapId, difficulty))
        return;
    ForEachCredited(map, [&](Player* player)
    {
        // IGNORE: the same boss on the same difficulty again this week, on another lockout, adds nothing
        CharacterDatabase.Execute("INSERT IGNORE INTO mythic_plus_vault_activity (guid, week, `row`, map, difficulty, "
                                  "ref) VALUES ({}, {}, {}, {}, {}, {})",
                                  player->GetGUID().GetCounter(), week, uint32(VaultRow::Raid), mapId, difficulty,
                                  creditEntry);
    });
}

// Kept on the dungeon's map once its clear is credited, so its last boss credited again (a second credit for the same
// encounter) adds nothing
constexpr char const* WorldCreditedKey = "MythicPlusVaultWorld";

struct WorldCredited : public DataMap::Base
{
};

// The last World ref given, across all maps (map updates run on several threads)
std::mutex worldRefLock;
uint32 lastWorldRef = 0;

uint32 NextWorldRef()
{
    std::lock_guard<std::mutex> guard(worldRefLock);
    lastWorldRef = WorldClearRef(uint32(GameTime::GetGameTime().count()), lastWorldRef);
    return lastWorldRef;
}

// A heroic dungeon cleared without a keystone, for everyone it credits (ForEachCredited): a row per clear, its ref the
// completion's time (WorldClearRef), which ReadWorldWeek counts. The map is marked so the same instance counts once.
// A key run's last boss is the Dungeons row's; a run already begun or over is that.
void RecordWorldDungeon(Map* map, bool dungeonCompleted)
{
    MythicRun const* run = FindRun(map);
    if (!FindDungeon(map->GetId()) ||
        !CountsForWorld(map->IsHeroic(), run && run->state != RunState::None, dungeonCompleted,
                        map->CustomData.Get<WorldCredited>(WorldCreditedKey) != nullptr))
        return;
    map->CustomData.Set(WorldCreditedKey, new WorldCredited());
    uint32 const week = CurrentWeek();
    uint16 const mapId = uint16(map->GetId());
    uint32 const ref = NextWorldRef();
    ForEachCredited(map, [&](Player* player)
    {
        CharacterDatabase.Execute("INSERT IGNORE INTO mythic_plus_vault_activity (guid, week, `row`, map, difficulty, "
                                  "ref) VALUES ({}, {}, {}, {}, 1, {})",
                                  player->GetGUID().GetCounter(), week, uint32(VaultRow::World), mapId, ref);
    });
}

// Whether the credit is one of the map's encounters, not just any boss-flagged creature or credited spell
bool IsEncounter(std::list<DungeonEncounter const*> const* encounters, EncounterCreditType type, uint32 creditEntry)
{
    return encounters && std::any_of(encounters->begin(), encounters->end(), [&](DungeonEncounter const* encounter)
                                     { return encounter->creditType == type && encounter->creditEntry == creditEntry; });
}
}

std::vector<VaultOption> VaultOptions(Player* player, bool shown)
{
    uint32 const week = CurrentWeek();
    if (!week)
        return {};
    std::vector<VaultOption> options = OptionsFor(player, week - 1, ReadWeek(player, week - 1));
    if (shown && !options.empty())
        DataOf(player)->shownWeek = week - 1;
    return options;
}

std::string ClaimOption(Player* player, VaultRow row, uint8 slot)
{
    if (!IsBot(player) && !player->FindNearestCreature(KeystoneMasterEntry, ForgeRange))
        return "Visit the Great Vault in Dalaran to choose your reward.";
    uint32 const week = CurrentWeek();
    VaultWeek const vault = week ? ReadWeek(player, week - 1) : VaultWeek{};
    if (!AnyUnlocked(vault))
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
    std::vector<VaultOption> const options = RollOptions(player, week - 1, vault);
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
    VaultWeek const last = week ? ReadWeek(player, week - 1) : VaultWeek{};
    std::vector<VaultOption> const options = week ? OptionsFor(player, week - 1, last) : std::vector<VaultOption>{};
    if (!options.empty())
        DataOf(player)->shownWeek = week - 1;
    // While options wait the rows show the week they came from, as retail's; once one is chosen, this week's
    // progress, the runs from memory
    VaultWeek const shown = options.empty() ? ReadWeek(player, week) : last;
    SendToClient(player, FormatVault(!options.empty()));
    for (VaultOption const& option : options)
        SendToClient(player, FormatVaultOption(option.row, option.slot, option.item));
    for (uint8 index = 1; index <= VaultRows; ++index)
    {
        VaultRow const row = VaultRow(index);
        for (uint8 slot = 1; slot <= VaultSlots; ++slot)
        {
            uint32 progress = 0;
            uint32 itemLevel = 0;
            if (row == VaultRow::Dungeon)
            {
                uint8 const level = uint8(shown.dungeons.levels[slot - 1]);
                progress = shown.dungeons.runs;
                itemLevel = level ? uint32(ItemLevelForRank(VaultRankForKey(level))) : 0u;
            }
            else if (row == VaultRow::Raid)
            {
                progress = shown.raids.kills;
                itemLevel = shown.raids.levels[slot - 1];
            }
            else
            {
                progress = shown.world.completed;
                itemLevel = shown.world.levels[slot - 1];
            }
            SendToClient(player, FormatVaultSlot(row, slot, progress, VaultThreshold(row, slot), itemLevel));
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

// The Raids row's bosses: a kill credit in a Wrath raid (MythicVaultPolicy.h CountsForRaid), and the World row's
// dungeons: the last encounter of a heroic 5-man (CountsForWorld)
class MythicVaultGlobalScript : public GlobalScript
{
public:
    MythicVaultGlobalScript() : GlobalScript("MythicVaultGlobalScript", {GLOBALHOOK_ON_AFTER_UPDATE_ENCOUNTER_STATE}) { }

    void OnAfterUpdateEncounterState(Map* map, EncounterCreditType type, uint32 creditEntry, Unit* source,
                                     Difficulty /*difficulty*/,
                                     std::list<DungeonEncounter const*> const* encounters,
                                     uint32 dungeonCompleted, bool /*updated*/) override
    {
        // The source is the boss, or the caster of a credited spell
        if (CountsForRaid(uint16(map->GetId()), source ? source->GetLevel() : 0) &&
            IsEncounter(encounters, type, creditEntry))
            RecordRaidBoss(map, creditEntry);
        // The last encounter of a dungeon sets dungeonCompleted
        else if (map->IsDungeon())
            RecordWorldDungeon(map, dungeonCompleted != 0);
    }
};

void AddSC_mythic_vault()
{
    new MythicVaultPlayerScript();
    new MythicVaultGlobalScript();
}
