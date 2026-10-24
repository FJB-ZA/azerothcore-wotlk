/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <bit>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Duration.h"
#include "GameObject.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MythicPlus.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"

// Mythic+ runs: the Font of Power takes the key, then a countdown, the timer, deaths, enemy forces and boss credit
// decide the run. The finish pays crests, sets new keys and leaves a cache with one roll per player, bots included.
namespace MythicPlus
{
namespace
{
constexpr char const* RunKey = "MythicPlusRun";
constexpr uint32 SnapshotMs = 1000;
constexpr uint32 FontSender = GOSSIP_SENDER_MAIN + 41;
constexpr uint32 CacheLifeSeconds = 2 * 3600;
constexpr Milliseconds BotForgeDelay = 5s;
constexpr uint8 BotForgeTries = 12;
constexpr char const* BotStartKey = "MythicPlusBotStart";
constexpr uint32 BotStartCheckMs = 2000;

struct BotStartTimer : public DataMap::Base
{
    uint32 ms = 0;
};

void Announce(Map* map, std::string const& text, bool notify)
{
    map->DoForAllPlayers([&](Player* player)
    {
        if (IsBot(player))
            return;
        ChatHandler handler(player->GetSession());
        handler.SendSysMessage(text);
        if (notify)
            handler.SendNotification(text);
    });
}

std::string AffixLine(MythicRun const& run)
{
    std::string text;
    for (Affix affix : {run.affixes.plus4, run.affixes.plus7, run.affixes.plus10})
    {
        if (!run.Has(affix))
            continue;
        if (!text.empty())
            text += ", ";
        text += AffixName(affix);
    }
    return text.empty() ? "no affixes" : text;
}

std::string Clock(uint64 ms)
{
    uint64 const seconds = ms / 1000;
    return std::to_string(seconds / 60) + (seconds % 60 < 10 ? ":0" : ":") + std::to_string(seconds % 60);
}

std::string KeyName(Keystone const& key)
{
    DungeonInfo const* dungeon = FindDungeon(key.map);
    return std::string(dungeon ? dungeon->name : "an unknown dungeon") + " +" + std::to_string(key.level);
}

uint16 RandomDungeon()
{
    return Dungeons[urand(0, uint32(Dungeons.size() - 1))].map;
}

// Trash a run counts: hostile, attackable, not a boss, a trigger or a critter
bool CountsAsForces(CreatureTemplate const* info)
{
    if (info->rank == CREATURE_ELITE_WORLDBOSS || info->npcflag || FindBossMechanic(info->Entry))
        return false;
    uint32 const skipped =
        CREATURE_FLAG_EXTRA_TRIGGER | CREATURE_FLAG_EXTRA_CIVILIAN | CREATURE_FLAG_EXTRA_DUNGEON_BOSS;
    if (info->flags_extra & skipped)
        return false;
    if (info->type == CREATURE_TYPE_CRITTER || info->type == CREATURE_TYPE_NON_COMBAT_PET)
        return false;
    if (info->unit_flags & (UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE))
        return false;
    FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(info->faction);
    return faction && faction->IsHostileToPlayers();
}

void CollectForces(Map* map, MythicRun& run)
{
    for (auto const& [cell, guids] : sObjectMgr->GetMapObjectGuids(uint16(map->GetId()), map->GetSpawnMode()))
    {
        for (ObjectGuid::LowType spawnId : guids.creatures)
        {
            CreatureData const* data = sObjectMgr->GetCreatureData(spawnId);
            CreatureTemplate const* info = data ? sObjectMgr->GetCreatureTemplate(data->id) : nullptr;
            if (!info || !CountsAsForces(info))
                continue;
            run.forceSpawns.insert(spawnId);
            if (run.Has(Affix::Teeming) && urand(1, 100) <= TeemingPercent)
                run.teeming.insert(spawnId);
        }
    }
    run.quota = ForcesQuota(uint32(run.forceSpawns.size() + run.teeming.size()), run.dungeon->forcesPercent);
}

// Why this map's Font takes no key now, or empty
std::string FontRefusal(Map* map)
{
    DungeonInfo const* dungeon = FindDungeon(map->GetId());
    if (!dungeon || !map->IsDungeon() || !map->IsHeroic())
        return "The Font of Power only answers on heroic difficulty.";
    if (MythicRun* run = FindRun(map); run && run->state != RunState::None)
        return "A keystone is already in the Font.";
    InstanceScript* instance = map->ToInstanceMap()->GetInstanceScript();
    if (instance && instance->GetCompletedEncounterMask())
        return "A keystone needs a fresh dungeon. Reset this one first.";
    bool fighting = false;
    map->DoForAllPlayers([&](Player* member) { fighting = fighting || member->IsInCombat(); });
    if (fighting)
        return "Not while anyone is in combat.";
    return {};
}

bool HoldsKeyFor(Player* holder, Map* map)
{
    return GetKeystone(holder).map == map->GetId();
}

// Whose keys `player` may put in: their own, and those of their group's bots in the dungeon, who can't use the Font
std::vector<Player*> KeyHolders(Player* player, Map* map)
{
    std::vector<Player*> holders;
    if (HoldsKeyFor(player, map))
        holders.push_back(player);
    Group* group = player->GetGroup();
    if (!group)
        return holders;
    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (member && member->GetMap() == map && IsBot(member) && HoldsKeyFor(member, map))
            holders.push_back(member);
    }
    return holders;
}

std::string NoKeyReason(Player* player)
{
    Keystone const key = GetKeystone(player);
    if (!key.map)
        return "No one here holds a keystone. The Keystone Master in Dalaran gives you one.";
    return "Your keystone is for " + KeyName(key) + ", and no bot in your group holds one for this dungeon.";
}

// Completion runs inside Unit::Kill, so swapping gear waits; the event dies with the bot if it logs out first
void ScheduleBotForge(Player* bot, uint8 triesLeft)
{
    bot->m_Events.AddEventAtOffset([bot, triesLeft]
    {
        if (!bot->IsInCombat())
            SpendCrestsOnGear(bot);
        else if (triesLeft)
            ScheduleBotForge(bot, uint8(triesLeft - 1));
    }, BotForgeDelay);
}

// One cache roll per participant; returns why it gave nothing, or empty
std::string RollCache(Player* player, MythicRun& run, uint16 map)
{
    if (!run.participants.count(player->GetGUID()))
        return "Only those who started the run can open the cache.";
    if (!run.looted.insert(player->GetGUID()).second)
        return "You have already opened this cache.";
    if (urand(1, 100) > ChestItemChancePercent(run.upgrade > 0))
        return "The cache held nothing for you this time.";
    uint32 const item = RollItem(player, map);
    if (!item)
        return "The cache held nothing for your class.";
    GiveItem(player, item);
    return {};
}

// `player` is whose key goes in, a bot when the group leader puts in a bot's key
void Start(Player* player, Map* map, Keystone const& key)
{
    MythicRun* run = map->CustomData.GetDefault<MythicRun>(RunKey);
    run->dungeon = FindDungeon(map->GetId());
    run->level = ClampLevel(key.level);
    run->week = WeekIndex(uint64(GameTime::GetGameTime().count()));
    run->affixes = AffixesForWeek(run->week);
    run->keyOwner = player->GetGUID();
    run->countdownMs = CountdownMs;
    run->state = RunState::Countdown;
    map->DoForAllPlayers([&](Player* member) { run->participants.insert(member->GetGUID()); });
    CollectForces(map, *run);
    BringTeemingCopies(map, *run);
    // The key depletes now and comes back upgraded only if the run is timed
    SetKeystone(player, key.map, NextKeyLevel(key.level, 0));
    ScaleMap(map, *run);
    Announce(map, player->GetName() + " starts " + KeyName(key) + " (" + AffixLine(*run) + "). The timer starts in " +
                      std::to_string(CountdownMs / 1000) + " seconds.", true);
}

// A whole group of bots, alone in the dungeon, alive and out of combat; the leader's key goes first
Player* BotKeyHolder(Map* map)
{
    Group* group = nullptr;
    bool ready = true;
    uint32 present = 0;
    map->DoForAllPlayers([&](Player* player)
    {
        if (player->IsGameMaster())
            return;
        ++present;
        Group* own = player->GetGroup();
        if (!IsBot(player) || !own || (group && own != group) || !player->IsAlive() || player->IsInCombat())
            ready = false;
        group = own;
    });
    if (!ready || !group || present != group->GetMembersCount())
        return nullptr;
    Player* holder = nullptr;
    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (member && member->GetMap() == map && HoldsKeyFor(member, map) &&
            (!holder || member->GetGUID() == group->GetLeaderGUID()))
            holder = member;
    }
    return holder;
}

// Bots can't use the Font's gossip, so a bot group puts its key in once everyone has come in
void TryBotStart(Map* map, uint32 diff)
{
    if (!map->IsDungeon() || !FindDungeon(map->GetId()) || !map->HavePlayers())
        return;
    BotStartTimer* timer = map->CustomData.GetDefault<BotStartTimer>(BotStartKey);
    timer->ms += diff;
    if (timer->ms < BotStartCheckMs)
        return;
    timer->ms = 0;
    if (!FontRefusal(map).empty())
        return;
    if (Player* holder = BotKeyHolder(map))
        Start(holder, map, GetKeystone(holder));
}

void Snapshot(Map* map, MythicRun const& run)
{
    map->DoForAllPlayers([&](Player* player)
    {
        if (!IsBot(player))
            SendRun(player, run);
    });
}

void Complete(Map* map, MythicRun& run)
{
    run.state = RunState::Completed;
    run.finalMs = run.TotalMs();
    run.upgrade = TimerUpgrade(run.finalMs, run.LimitMs());
    bool const timed = run.upgrade > 0;
    uint16 const mapId = uint16(map->GetId());
    Crest const crest = CrestForKeyLevel(run.level);
    Player* anchor = nullptr;

    map->DoForAllPlayers([&](Player* player)
    {
        if (!anchor && player->IsAlive())
            anchor = player;
        if (!run.participants.count(player->GetGUID()))
            return;
        uint32 const crests = AwardCrests(player, crest, CrestsForRun(timed));
        RecordRun(player, mapId, run.level, uint32(run.finalMs / 1000), run.upgrade);
        Keystone key = GetKeystone(player);
        if (player->GetGUID() == run.keyOwner)
            key = timed ? Keystone{RandomDungeon(), NextKeyLevel(run.level, run.upgrade)}
                        : Keystone{mapId, NextKeyLevel(run.level, 0)};
        else if (!key.map)
            key = Keystone{RandomDungeon(), NextKeyLevel(run.level, 0)};
        SetKeystone(player, key.map, key.level);
        SendResult(player, run, key, crest, crests);
        // Bots can't click the cache, so they take their roll here and visit the forge once the fight is over
        if (IsBot(player))
        {
            RollCache(player, run, mapId);
            ScheduleBotForge(player, BotForgeTries);
        }
    });

    if (anchor)
    {
        float const angle = anchor->GetOrientation();
        float const x = anchor->GetPositionX() + 3.0f * std::cos(angle);
        float const y = anchor->GetPositionY() + 3.0f * std::sin(angle);
        float const facing = angle + std::numbers::pi_v<float>;
        map->SummonGameObject(CacheEntry, x, y, anchor->GetPositionZ(), facing, 0.0f, 0.0f, std::sin(facing / 2.0f),
                              std::cos(facing / 2.0f), CacheLifeSeconds);
    }

    std::string const verdict = timed ? "in time, keystone +" + std::to_string(run.upgrade) : "over time";
    Announce(map, std::string(run.dungeon->name) + " +" + std::to_string(run.level) + " complete in " +
                      Clock(run.finalMs) + " (" + verdict + "). The Mythic Cache holds your reward.", true);
    Snapshot(map, run);
}
}

MythicRun* FindRun(Map* map)
{
    return map ? map->CustomData.Get<MythicRun>(RunKey) : nullptr;
}

MythicRun* ActiveRun(Map* map)
{
    MythicRun* run = FindRun(map);
    return run && run->Active() ? run : nullptr;
}

bool IsBot(Player* player)
{
    return player->GetSession()->GetRemoteAddress() == "headless";
}

bool IsBoss(Creature* creature)
{
    return creature->IsDungeonBoss() || creature->isWorldBoss() || FindBossMechanic(creature->GetEntry());
}

bool IsEnemyKind(Creature* creature)
{
    if (creature->IsPet() || creature->IsTotem() || creature->IsTrigger() || creature->IsCritter() ||
        creature->IsCivilian() || creature->GetCharmerOrOwnerGUID().IsPlayer() || creature->GetNpcFlags())
        return false;
    uint32 const entry = creature->GetEntry();
    if (entry == ExplosiveOrbEntry || entry == RuneEntry)
        return false;
    return creature->IsHostileToPlayers();
}

bool IsMythicEnemy(Creature* creature)
{
    return creature->IsAlive() && IsEnemyKind(creature);
}

uint8 PlayersIn(Map* map)
{
    uint8 count = 0;
    map->DoForAllPlayers([&](Player* player) { count += player->IsGameMaster() ? 0 : 1; });
    return count;
}

void CheckCompletion(Map* map)
{
    MythicRun* run = ActiveRun(map);
    if (run && run->finalBoss && IsComplete(run->bosses, run->dungeon->requiredBosses, run->forces, run->quota))
        Complete(map, *run);
}
}

using namespace MythicPlus;

class go_mythic_font_of_power : public GameObjectScript
{
public:
    go_mythic_font_of_power() : GameObjectScript("go_mythic_font_of_power") { }

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        ClearGossipMenuFor(player);
        if (IsBot(player))
            return true;
        Map* map = go->GetMap();
        ChatHandler handler(player->GetSession());
        if (std::string const reason = FontRefusal(map); !reason.empty())
        {
            handler.SendNotification(reason);
            return true;
        }
        std::vector<Player*> const holders = KeyHolders(player, map);
        if (holders.empty())
        {
            handler.SendNotification(NoKeyReason(player));
            return true;
        }
        MythicRun preview;
        preview.affixes = AffixesForWeek(WeekIndex(uint64(GameTime::GetGameTime().count())));
        for (Player* holder : holders)
        {
            Keystone const key = GetKeystone(holder);
            preview.level = key.level;
            std::string const whose = holder == player ? "your" : holder->GetName() + "'s";
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "Insert " + whose + " keystone: " + KeyName(key) + ".",
                             FontSender, holder->GetGUID().GetCounter(),
                             "Start " + KeyName(key) + " with " + AffixLine(preview) + "? The keystone drops a level "
                             "unless the run is timed.", 0, false);
        }
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, go->GetGUID());
        return true;
    }

    // The action is the key holder's guid, checked again because the menu may be stale
    bool OnGossipSelect(Player* player, GameObject* go, uint32 sender, uint32 action) override
    {
        CloseGossipMenuFor(player);
        if (sender != FontSender)
            return true;
        Map* map = go->GetMap();
        if (std::string const reason = FontRefusal(map); !reason.empty())
        {
            ChatHandler(player->GetSession()).SendNotification(reason);
            return true;
        }
        for (Player* holder : KeyHolders(player, map))
        {
            if (holder->GetGUID().GetCounter() == action)
            {
                Start(holder, map, GetKeystone(holder));
                return true;
            }
        }
        ChatHandler(player->GetSession()).SendNotification(NoKeyReason(player));
        return true;
    }
};

class go_mythic_cache : public GameObjectScript
{
public:
    go_mythic_cache() : GameObjectScript("go_mythic_cache") { }

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        ClearGossipMenuFor(player);
        CloseGossipMenuFor(player);
        MythicRun* run = FindRun(go->GetMap());
        if (!run || run->state != RunState::Completed || IsBot(player))
            return true;
        if (std::string const reason = RollCache(player, *run, uint16(go->GetMapId())); !reason.empty())
            ChatHandler(player->GetSession()).SendNotification(reason);
        return true;
    }

    bool OnGossipSelect(Player* player, GameObject* /*go*/, uint32 /*sender*/, uint32 /*action*/) override
    {
        CloseGossipMenuFor(player);
        return true;
    }
};

class MythicRunMapScript : public AllMapScript
{
public:
    MythicRunMapScript()
        : AllMapScript("MythicRunMapScript",
                       {ALLMAPHOOK_ON_MAP_UPDATE, ALLMAPHOOK_ON_PLAYER_ENTER_ALL, ALLMAPHOOK_ON_PLAYER_LEAVE_ALL})
    {
    }

    void OnMapUpdate(Map* map, uint32 diff) override
    {
        MythicRun* run = FindRun(map);
        if (!run)
        {
            TryBotStart(map, diff);
            return;
        }
        switch (run->state)
        {
            case RunState::Countdown:
                if (run->countdownMs > diff)
                    run->countdownMs -= diff;
                else
                {
                    run->countdownMs = 0;
                    run->state = RunState::Running;
                    Announce(map, "The keystone is active. Go!", true);
                }
                break;
            case RunState::Running:
            case RunState::Overtime:
                run->elapsedMs += diff;
                if (run->state == RunState::Running && run->TotalMs() > run->LimitMs())
                {
                    run->state = RunState::Overtime;
                    Announce(map, "Time is up. The run goes on, but the keystone will drop a level.", true);
                }
                break;
            default:
                return;
        }
        run->snapshotMs += diff;
        if (run->snapshotMs >= SnapshotMs)
        {
            run->snapshotMs = 0;
            Snapshot(map, *run);
        }
    }

    void OnPlayerEnterAll(Map* map, Player* player) override
    {
        if (MythicRun* run = FindRun(map); run && !IsBot(player))
            SendRun(player, *run);
    }

    // A finished run frees its players' lockout as they leave, so the dungeon can be run again
    void OnPlayerLeaveAll(Map* map, Player* player) override
    {
        MythicRun* run = FindRun(map);
        if (!run)
            return;
        if (run->state == RunState::Completed && run->participants.count(player->GetGUID()))
            sInstanceSaveMgr->PlayerUnbindInstance(player->GetGUID(), map->GetId(), map->GetDifficulty(), true, player);
        if (!IsBot(player))
            SendRun(player, MythicRun());
    }
};

class MythicRunGlobalScript : public GlobalScript
{
public:
    MythicRunGlobalScript() : GlobalScript("MythicRunGlobalScript", {GLOBALHOOK_ON_AFTER_UPDATE_ENCOUNTER_STATE}) { }

    void OnAfterUpdateEncounterState(Map* map, EncounterCreditType /*type*/, uint32 /*creditEntry*/, Unit* /*source*/,
                                     Difficulty /*difficulty*/,
                                     std::list<DungeonEncounter const*> const* /*encounters*/,
                                     uint32 dungeonCompleted, bool updated) override
    {
        MythicRun* run = ActiveRun(map);
        InstanceMap* instanceMap = map->ToInstanceMap();
        InstanceScript* instance = instanceMap ? instanceMap->GetInstanceScript() : nullptr;
        if (!run || !updated || !instance)
            return;
        run->bosses = uint8(std::popcount(instance->GetCompletedEncounterMask()));
        // instance_encounters.lastEncounterDungeon marks it, for all 16 dungeons
        if (dungeonCompleted)
            run->finalBoss = true;
        CheckCompletion(map);
    }
};

class MythicRunUnitScript : public UnitScript
{
public:
    MythicRunUnitScript() : UnitScript("MythicRunUnitScript", true, {UNITHOOK_ON_UNIT_DEATH}) { }

    // Enemies drop nothing during a run; the cache holds the loot
    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        Creature* creature = unit->ToCreature();
        MythicRun* run = creature ? ActiveRun(creature->GetMap()) : nullptr;
        if (!run)
            return;
        creature->loot.clear();
        creature->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);

        ObjectGuid::LowType const spawnId = creature->GetSpawnId();
        bool const counts = spawnId ? run->forceSpawns.count(spawnId) && run->counted.insert(spawnId).second
                                    : IsTeemingCopy(creature);
        if (!counts)
            return;
        ++run->forces;
        CheckCompletion(creature->GetMap());
    }
};

class MythicRunPlayerScript : public PlayerScript
{
public:
    MythicRunPlayerScript() : PlayerScript("MythicRunPlayerScript", {PLAYERHOOK_ON_PLAYER_JUST_DIED}) { }

    void OnPlayerJustDied(Player* player) override
    {
        MythicRun* run = ActiveRun(player->GetMap());
        if (!run)
            return;
        ++run->deaths;
        Announce(player->GetMap(), player->GetName() + " died: " + std::to_string(DeathPenaltyMs / 1000) +
                                       " seconds added to the timer.", false);
    }
};

void AddSC_mythic_run()
{
    new go_mythic_font_of_power();
    new go_mythic_cache();
    new MythicRunMapScript();
    new MythicRunGlobalScript();
    new MythicRunUnitScript();
    new MythicRunPlayerScript();
}
