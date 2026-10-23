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
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AccountMgr.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "QueryResult.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SubclassPaths.h"
#include "WorldPacket.h"
#include "WorldSession.h"

using namespace Acore::ChatCommands;
using SubclassPaths::Path;

// Sub-class paths: a spec slot may follow a path (subclass_path) whose replacement spells (subclass_spell_swap) take
// the originals' place in that slot only. The core already stores each learned spell's slots in
// character_spell.specMask and swaps spells on a slot switch, so a path only moves spells between slot masks.
namespace
{
std::unordered_map<uint16, Path> paths;
std::unordered_set<uint32> swappedOriginals;
std::unordered_map<uint32, SubclassPaths::CreatureSwap> creatureSwaps;  // by replacement entry
std::unordered_set<uint32> bonusSchoolSpells;
std::unordered_map<uint32, uint16> treeOwners;  // a path's own talent tree (TalentTab id) to the path
bool affectsCombat = false;

// A character's path per talent group and its path change fee, loaded at login.
struct CharacterPaths : DataMap::Base
{
    std::array<uint16, MAX_TALENT_SPECS> group{};
    uint32 lastFee = 0;
    uint32 lastChange = 0;
    bool loaded = false;
    bool randomBot = false;
    bool dirty = false;  // reconcile on the next update, once whatever learned a spell has sent its own packets
    uint32 sinceSweep = 0;
};

constexpr char const* DataKey = "SubclassPaths";
// Rank-ups learned outside the trainer (level-up, bots, scripts) fire no hook; a sweep catches them.
constexpr uint32 SweepIntervalMs = 5000;

CharacterPaths* PathsOf(Player* player) { return player->CustomData.GetDefault<CharacterPaths>(DataKey); }

Path const* FindPath(uint16 id)
{
    auto const itr = paths.find(id);
    return itr != paths.end() ? &itr->second : nullptr;
}

std::string PathName(uint16 id)
{
    Path const* path = FindPath(id);
    return path ? path->name : "the plain class";
}

uint16 TreeOwner(uint32 talentTabId)
{
    auto const itr = treeOwners.find(talentTabId);
    return itr != treeOwners.end() ? itr->second : SubclassPaths::PlainClass;
}

// Bot sessions have no client socket, and the core names them "headless".
bool IsBot(Player* player) { return player->GetSession()->GetRemoteAddress() == "headless"; }

// Talent points a talent group has spent in one tree
uint32 PointsIn(Player* player, uint32 talentTabId, uint8 group)
{
    uint32 points = 0;
    for (auto const& [spellId, talent] : player->GetTalentMap())
    {
        if (talent->State == PLAYERSPELL_REMOVED || !talent->IsInSpec(group))
            continue;
        TalentSpellPos const* pos = GetTalentSpellPos(spellId);
        TalentEntry const* entry = pos ? sTalentStore.LookupEntry(pos->talent_id) : nullptr;
        if (entry && entry->TalentTab == talentTabId)
            points += pos->rank + 1;
    }
    return points;
}

bool HasInSlot(Player* player, uint32 spellId, uint8 group)
{
    PlayerSpellMap const& spells = player->GetSpellMap();
    auto const itr = spells.find(spellId);
    return itr != spells.end() && itr->second->State != PLAYERSPELL_REMOVED && itr->second->IsInSpec(group);
}

void MoveSpell(Player* player, uint32 from, bool hasFrom, uint32 to, bool hasTo, uint8 mask)
{
    if (hasFrom)
        player->removeSpell(from, mask, false);
    if (!hasTo && player->addSpell(to, mask, true))
        player->SendLearnPacket(to, true);
}

// Puts the active slot's action bar on the spells the slot now knows: replacements for originals on a path, and
// back again off it.
void SwapActionButtons(Player* player)
{
    Path const* current = SubclassPaths::ActivePath(player);
    bool changed = false;
    for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
    {
        ActionButton const* action = player->GetActionButton(button);
        if (!action || action->GetType() != ACTION_BUTTON_SPELL)
            continue;
        uint32 const spellId = action->GetAction();
        uint32 wanted = spellId;
        for (auto const& [id, path] : paths)
            for (SubclassPaths::SpellSwap const& swap : path.swaps)
            {
                if (&path == current && swap.original == spellId)
                    wanted = swap.replacement;
                else if (&path != current && swap.replacement == spellId)
                    wanted = swap.original;
            }
        if (wanted != spellId)
        {
            player->addActionButton(button, wanted, ACTION_BUTTON_SPELL);
            changed = true;
        }
    }
    if (changed)
        player->SendActionButtons(1);
}

// Puts the active slot's spells in line with its path: other paths' replacements go back to the originals first,
// then the slot's own path is applied. Only the active slot is touched, so spell auras and packets stay right; the
// other slots are put in line when they become active.
void Reconcile(Player* player)
{
    CharacterPaths* data = PathsOf(player);
    if (!data->loaded)
        return;

    uint8 const group = player->GetActiveSpec();
    uint16 const current = data->group[group];
    uint8 const mask = SubclassPaths::SlotMask(group);

    auto apply = [&](Path const& path, bool onPath)
    {
        for (SubclassPaths::SpellSwap const& swap : path.swaps)
        {
            bool const hasOriginal = HasInSlot(player, swap.original, group);
            bool const hasReplacement = HasInSlot(player, swap.replacement, group);
            switch (SubclassPaths::Move(onPath, hasOriginal, hasReplacement))
            {
                case SubclassPaths::SpellMove::ToReplacement:
                    MoveSpell(player, swap.original, hasOriginal, swap.replacement, hasReplacement, mask);
                    break;
                case SubclassPaths::SpellMove::ToOriginal:
                    MoveSpell(player, swap.replacement, hasReplacement, swap.original, hasOriginal, mask);
                    break;
                case SubclassPaths::SpellMove::None:
                    break;
            }
        }
    };

    for (auto const& [id, path] : paths)
        if (id != current && path.rules.classId == player->getClass())
            apply(path, false);
    if (Path const* path = FindPath(current))
        apply(*path, true);

    for (auto const& [tab, owner] : treeOwners)
    {
        if (SubclassPaths::TreeOpen(owner, current))
            continue;
        if (uint32 const refunded = player->ResetTalentTab(tab); refunded && !IsBot(player))
            ChatHandler(player->GetSession()).PSendSysMessage(
                "This spec slot no longer walks the {} path, so its {} points in that tree are yours to spend again.",
                PathName(owner), refunded);
    }
}

// Tells the SubclassPaths addon (SpecSlots) which path and tree each owned slot has.
void SendSlotPaths(Player* player)
{
    if (IsBot(player))
        return;
    CharacterPaths* data = PathsOf(player);
    for (uint8 group = 0; group < player->GetSpecsCount() && group < MAX_TALENT_SPECS; ++group)
    {
        Path const* path = FindPath(data->group[group]);
        TalentTabEntry const* tab = path && path->talentTab ? sTalentTabStore.LookupEntry(path->talentTab) : nullptr;
        std::string const line = SubclassPaths::FormatSlotPath(
            group + 1, path ? path->id : SubclassPaths::PlainClass, tab ? uint8(tab->tabpage + 1) : 0,
            tab ? PointsIn(player, tab->TalentTabID, group) : 0, path ? path->name : std::string());
        WorldPacket packet;
        ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
                                     std::string(SubclassPaths::AddonPrefix) + '\t' + line);
        player->SendDirectMessage(&packet);
    }
}

void Load(Player* player)
{
    CharacterPaths* data = PathsOf(player);
    uint32 const guid = player->GetGUID().GetCounter();
    if (QueryResult result = CharacterDatabase.Query("SELECT talent_group, path_id FROM character_subclass_path WHERE guid = {}", guid))
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const group = fields[0].Get<uint8>();
            if (group < MAX_TALENT_SPECS)
                data->group[group] = fields[1].Get<uint16>();
        } while (result->NextRow());
    }
    if (QueryResult result = CharacterDatabase.Query("SELECT last_fee, last_change FROM character_subclass_fee WHERE guid = {}", guid))
    {
        Field* fields = result->Fetch();
        data->lastFee = fields[0].Get<uint32>();
        data->lastChange = fields[1].Get<uint32>();
    }
    // Only random bots pick their own path; a player's own characters played as bots keep what the player chose.
    std::string account;
    if (IsBot(player) && AccountMgr::GetName(player->GetSession()->GetAccountId(), account))
    {
        std::string prefix = sConfigMgr->GetOption<std::string>("Playerbots.RandomBotAccountPrefix", "rndbot", false);
        data->randomBot = account.size() >= prefix.size() &&
                          std::equal(prefix.begin(), prefix.end(), account.begin(),
                                     [](char a, char b) { return std::tolower(uint8(a)) == std::tolower(uint8(b)); });
    }
    data->loaded = true;
}

std::string Refusal(SubclassPaths::ChangeCheck check, Path const* wanted, uint32 fee)
{
    switch (check)
    {
        case SubclassPaths::ChangeCheck::SamePath: return "This spec slot already follows that path.";
        case SubclassPaths::ChangeCheck::WrongClass: return "That path belongs to another class.";
        case SubclassPaths::ChangeCheck::TooLow:
            return "That path opens at level " + std::to_string(wanted ? wanted->rules.unlockLevel : 0) + ".";
        case SubclassPaths::ChangeCheck::NotYetInEra: return "That path hasn't reached this world yet.";
        case SubclassPaths::ChangeCheck::Locked: return "You haven't unlocked that path yet.";
        case SubclassPaths::ChangeCheck::InCombat: return "You can't change your path in combat.";
        case SubclassPaths::ChangeCheck::InArena: return "You can't change your path in an arena.";
        case SubclassPaths::ChangeCheck::NoMoney:
            return "Changing your path costs " + std::to_string(fee / SubclassPaths::Gold) + " gold.";
        case SubclassPaths::ChangeCheck::Ok: break;
    }
    return {};
}

void MarkDirty(Player* player)
{
    if (CharacterPaths* data = player->CustomData.Get<CharacterPaths>(DataKey))
        data->dirty = true;
}

uint32 FeeFor(CharacterPaths const* data, uint32 now)
{
    return SubclassPaths::NextFee(data->lastFee, now > data->lastChange ? now - data->lastChange : 0);
}

// Moves the active slot to `wanted`, charging the path change fee unless `free`. Returns why it didn't, or empty.
std::string ChangePath(Player* player, uint16 wanted, bool free)
{
    CharacterPaths* data = PathsOf(player);
    if (!data->loaded)
        return "Your paths are still loading. Try again in a moment.";

    Path const* path = FindPath(wanted);
    if (wanted != SubclassPaths::PlainClass && !path)
        return "There is no path " + std::to_string(wanted) + ".";

    uint8 const group = player->GetActiveSpec();
    uint32 const now = uint32(GameTime::GetGameTime().count());
    uint32 const fee = free ? 0 : FeeFor(data, now);
    bool const unlocked = free || !path || !path->unlockQuest || player->GetQuestRewardStatus(path->unlockQuest);
    SubclassPaths::ChangeCheck const check = SubclassPaths::CanChangePath(
        data->group[group], wanted, path ? path->rules : SubclassPaths::PathRules{}, player->getClass(), player->GetLevel(),
        player->GetSession()->Expansion(), unlocked, player->IsInCombat(), player->InArena(), player->GetMoney(), fee);
    if (check != SubclassPaths::ChangeCheck::Ok)
        return Refusal(check, path, fee);

    if (fee)
    {
        player->ModifyMoney(-int32(fee));
        data->lastFee = fee;
        data->lastChange = now;
    }
    data->group[group] = wanted;

    // Like a slot switch: the pet goes, since the new path may summon another body for it.
    if (Pet* pet = player->GetPet())
        player->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
    Reconcile(player);
    SwapActionButtons(player);

    uint32 const guid = player->GetGUID().GetCounter();
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (wanted == SubclassPaths::PlainClass)
        trans->Append("DELETE FROM character_subclass_path WHERE guid = {} AND talent_group = {}", guid, group);
    else
        trans->Append("REPLACE INTO character_subclass_path (guid, talent_group, path_id) VALUES ({}, {}, {})", guid,
                      group, wanted);
    if (fee)
        trans->Append("REPLACE INTO character_subclass_fee (guid, last_fee, last_change) VALUES ({}, {}, {})", guid,
                      data->lastFee, data->lastChange);
    // Written before returning, so a quick relog can't load the old path and undo the swap.
    CharacterDatabase.DirectCommitTransaction(trans);
    // A crash before this save is undone at login, where the path table wins.
    player->SaveToDB(false, false);
    SendSlotPaths(player);
    return {};
}

// A path's own tree must be one of its class's talent tabs, on a page the core and client know, and no other path's.
void LoadTree(Path& path, uint32 talentTabId)
{
    path.talentTab = 0;
    if (!talentTabId)
        return;
    TalentTabEntry const* tab = sTalentTabStore.LookupEntry(talentTabId);
    if (!tab || !path.rules.classId || !(tab->ClassMask & (1u << (path.rules.classId - 1))) ||
        tab->tabpage >= MAX_TALENT_TABS)
    {
        LOG_ERROR("sql.sql", "subclass_path: path {} owns talent tab {}, not a talent page of class {}; ignored.", path.id,
                  talentTabId, path.rules.classId);
        return;
    }
    if (auto const [itr, added] = treeOwners.emplace(talentTabId, path.id); !added)
    {
        LOG_ERROR("sql.sql", "subclass_path: path {} owns talent tab {}, which path {} already owns; ignored.", path.id,
                  talentTabId, itr->second);
        return;
    }
    path.talentTab = talentTabId;
}

// A random bot's first slot takes the first path its bot share picks, then drops its talents so the AI respecs.
void PickBotPath(Player* player)
{
    CharacterPaths* data = PathsOf(player);
    if (!data->randomBot || player->GetActiveSpec() != 0 || data->group[0] != SubclassPaths::PlainClass ||
        player->IsInCombat())
        return;
    for (Path const* path : SubclassPaths::ClassPaths(player->getClass()))
    {
        if (!SubclassPaths::BotTakesPath(player->GetGUID().GetCounter(), path->id, path->botShare))
            continue;
        if (SubclassPaths::CanChangePath(SubclassPaths::PlainClass, path->id, path->rules, player->getClass(),
                                         player->GetLevel(), player->GetSession()->Expansion(), true, false,
                                         player->InArena(), 0, 0) != SubclassPaths::ChangeCheck::Ok)
            continue;
        if (ChangePath(player, path->id, true).empty())
            player->resetTalents(true);
        return;
    }
}

void LoadPathRows()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT id, name, class, unlock_level, era, damage_pve, damage_pvp, healing_pve, healing_pvp, unlock_quest, "
        "talent_tab, bot_share FROM subclass_path");
    if (!result)
        return;
    do
    {
        Field* fields = result->Fetch();
        uint16 const id = fields[0].Get<uint16>();
        if (id == SubclassPaths::PlainClass)
        {
            LOG_ERROR("sql.sql", "subclass_path: id 0 is the plain class and can't be a path; skipped.");
            continue;
        }
        Path& path = paths[id];
        path.id = id;
        path.name = fields[1].Get<std::string>();
        path.rules = {fields[2].Get<uint8>(), fields[3].Get<uint8>(), fields[4].Get<uint8>()};
        path.modifiers = {fields[5].Get<float>(), fields[6].Get<float>(), fields[7].Get<float>(), fields[8].Get<float>()};
        path.unlockQuest = fields[9].Get<uint32>();
        if (path.unlockQuest && !sObjectMgr->GetQuestTemplate(path.unlockQuest))
            LOG_ERROR("sql.sql", "subclass_path: path {} unlocks with quest {}, which doesn't exist.", id, path.unlockQuest);
        LoadTree(path, fields[10].Get<uint32>());
        path.botShare = std::clamp(fields[11].Get<float>(), 0.0f, 1.0f);
        affectsCombat |= !path.modifiers.Neutral();
    } while (result->NextRow());
}

uint32 LoadSpellSwaps()
{
    uint32 count = 0;
    QueryResult result = WorldDatabase.Query("SELECT path_id, spell, replacement FROM subclass_spell_swap");
    if (!result)
        return count;
    do
    {
        Field* fields = result->Fetch();
        uint16 const pathId = fields[0].Get<uint16>();
        uint32 const original = fields[1].Get<uint32>();
        uint32 const replacement = fields[2].Get<uint32>();
        auto const path = paths.find(pathId);
        SpellInfo const* originalInfo = sSpellMgr->GetSpellInfo(original);
        if (path == paths.end() || !originalInfo || !sSpellMgr->GetSpellInfo(replacement))
        {
            LOG_ERROR("sql.sql", "subclass_spell_swap: path {} swaps {} for {}, but the path or a spell is missing; skipped.",
                      pathId, original, replacement);
            continue;
        }
        if (GetTalentSpellCost(sSpellMgr->GetFirstSpellInChain(original)) || GetTalentSpellPos(original))
        {
            LOG_ERROR("sql.sql", "subclass_spell_swap: path {} swaps talent spell {}, which slots can't swap by mask; skipped.",
                      pathId, original);
            continue;
        }
        if (bonusSchoolSpells.count(replacement) || swappedOriginals.count(replacement))
        {
            LOG_ERROR("sql.sql", "subclass_spell_swap: path {} uses {} as a replacement, but another row already swaps it; skipped.",
                      pathId, replacement);
            continue;
        }
        path->second.swaps.push_back({original, replacement, originalInfo->GetRank()});
        swappedOriginals.insert(original);
        // Talents, spell power and crit bonuses that count the original's school count the replacement too.
        sSpellMgr->SetBonusSchoolMask(replacement, originalInfo->SchoolMask);
        bonusSchoolSpells.insert(replacement);
        ++count;
    } while (result->NextRow());

    for (auto& [id, path] : paths)
    {
        std::stable_sort(path.swaps.begin(), path.swaps.end(),
                         [](SubclassPaths::SpellSwap const& a, SubclassPaths::SpellSwap const& b) { return a.rank > b.rank; });
        // Removing a rank also removes the ranks above it, so every higher rank needs its own row.
        for (SubclassPaths::SpellSwap const& swap : path.swaps)
            if (uint32 const next = sSpellMgr->GetNextSpellInChain(swap.original))
                if (std::none_of(path.swaps.begin(), path.swaps.end(),
                                 [next](SubclassPaths::SpellSwap const& s) { return s.original == next; }))
                    LOG_ERROR("sql.sql", "subclass_spell_swap: path {} swaps {} but not its next rank {}.", id,
                              swap.original, next);
    }
    return count;
}

void LoadCreatureSwaps()
{
    QueryResult result = WorldDatabase.Query("SELECT path_id, entry, replacement, bodies FROM subclass_creature_swap");
    if (!result)
        return;
    do
    {
        Field* fields = result->Fetch();
        SubclassPaths::CreatureSwap const swap{fields[0].Get<uint16>(), fields[1].Get<uint32>(), fields[2].Get<uint32>(),
                                               std::max<uint8>(1, fields[3].Get<uint8>())};
        if (!paths.count(swap.pathId) || !sObjectMgr->GetCreatureTemplate(swap.original) ||
            !sObjectMgr->GetCreatureTemplate(swap.replacement))
        {
            LOG_ERROR("sql.sql", "subclass_creature_swap: path {} swaps creature {} for {}, but the path or a creature is missing; skipped.",
                      swap.pathId, swap.original, swap.replacement);
            continue;
        }
        if (sObjectMgr->GetCreatureTemplate(swap.original)->family != sObjectMgr->GetCreatureTemplate(swap.replacement)->family)
            LOG_ERROR("sql.sql", "subclass_creature_swap: creature {} has another pet family than {}, so pet talents and spells differ.",
                      swap.replacement, swap.original);
        creatureSwaps[swap.replacement] = swap;
        affectsCombat |= swap.bodies > 1;
    } while (result->NextRow());
}

void LoadPaths()
{
    for (uint32 spellId : bonusSchoolSpells)
        sSpellMgr->SetBonusSchoolMask(spellId, 0);
    paths.clear();
    swappedOriginals.clear();
    creatureSwaps.clear();
    bonusSchoolSpells.clear();
    treeOwners.clear();
    affectsCombat = false;

    LoadPathRows();
    uint32 const spellSwaps = LoadSpellSwaps();
    LoadCreatureSwaps();
    LOG_INFO("server.loading", ">> Loaded {} sub-class paths with {} spell swaps and {} creature swaps", paths.size(),
             spellSwaps, creatureSwaps.size());
}
}  // namespace

namespace SubclassPaths
{
Path const* ActivePath(Player const* player)
{
    CharacterPaths const* data = player->CustomData.Get<CharacterPaths>(DataKey);
    return data ? FindPath(data->group[player->GetActiveSpec()]) : nullptr;
}

uint32 ActiveReplacement(Player const* player, uint32 originalSpellId)
{
    if (Path const* path = ActivePath(player))
        for (SpellSwap const& swap : path->swaps)
            if (swap.original == originalSpellId)
                return swap.replacement;
    return 0;
}

uint32 ActiveOriginal(Player const* player, uint32 replacementSpellId)
{
    if (Path const* path = ActivePath(player))
        for (SpellSwap const& swap : path->swaps)
            if (swap.replacement == replacementSpellId)
                return swap.original;
    return 0;
}

CreatureSwap const* SwapForReplacementCreature(uint32 entry)
{
    auto const itr = creatureSwaps.find(entry);
    return itr != creatureSwaps.end() ? &itr->second : nullptr;
}

bool AffectsCombat() { return affectsCombat; }

std::vector<Path const*> ClassPaths(uint8 classId)
{
    std::vector<Path const*> result;
    for (auto const& [id, path] : paths)
        if (path.rules.classId == classId)
            result.push_back(&path);
    std::sort(result.begin(), result.end(), [](Path const* a, Path const* b) { return a->id < b->id; });
    return result;
}

uint16 ActivePathId(Player const* player)
{
    CharacterPaths const* data = player->CustomData.Get<CharacterPaths>(DataKey);
    return data ? data->group[player->GetActiveSpec()] : PlainClass;
}

uint32 NextChangeFee(Player* player) { return FeeFor(PathsOf(player), uint32(GameTime::GetGameTime().count())); }

std::string ChangeActivePath(Player* player, uint16 pathId, bool free) { return ChangePath(player, pathId, free); }
}

class SubclassPathsWorldScript : public WorldScript
{
public:
    SubclassPathsWorldScript() : WorldScript("SubclassPathsWorldScript", {WORLDHOOK_ON_STARTUP}) { }

    void OnStartup() override { LoadPaths(); }
};

class SubclassPathsPlayerScript : public PlayerScript
{
public:
    SubclassPathsPlayerScript()
        : PlayerScript("SubclassPathsPlayerScript",
                       {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEARN_SPELL, PLAYERHOOK_ON_AFTER_TRAIN_SPELL,
                        PLAYERHOOK_ON_LEVEL_CHANGED, PLAYERHOOK_ON_AFTER_SPEC_SLOT_CHANGED, PLAYERHOOK_ON_UPDATE,
                        PLAYERHOOK_ON_DELETE, PLAYERHOOK_CAN_LEARN_TALENT, PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT})
    {
    }

    void OnPlayerLogin(Player* player) override
    {
        Load(player);
        Reconcile(player);
        SendSlotPaths(player);
    }

    // A path's own tree takes points only while the active slot follows that path.
    bool OnPlayerCanLearnTalent(Player* player, TalentEntry const* talent, uint32 /*rank*/) override
    {
        return SubclassPaths::TreeOpen(TreeOwner(talent->TalentTab), SubclassPaths::ActivePathId(player));
    }

    // The addon asks with a whisper to the player themself: "SoloWowPath\tQ".
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        std::string const prefix = std::string(SubclassPaths::AddonPrefix) + '\t';
        if (language != LANG_ADDON || type != CHAT_MSG_WHISPER || receiver != player || msg.rfind(prefix, 0) != 0)
            return true;
        if (SubclassPaths::IsQuery(std::string_view(msg).substr(prefix.size())))
            SendSlotPaths(player);
        return false;
    }

    // A trainer teaches originals to every slot; on a path the active slot takes the replacement instead. The hook
    // fires inside learnSpell, before its own learn packet, so the swap waits for the next update.
    void OnPlayerLearnSpell(Player* player, uint32 spellId) override
    {
        if (swappedOriginals.count(spellId))
            MarkDirty(player);
    }

    // A rank-up supersedes a lower rank, and learnSpell skips OnPlayerLearnSpell for it.
    void OnPlayerAfterTrainSpell(Player* player, Creature* /*trainer*/, uint32 spellId) override
    {
        if (swappedOriginals.count(spellId))
            MarkDirty(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override { MarkDirty(player); }

    void OnPlayerAfterSpecSlotChanged(Player* player, uint8 /*newSlot*/) override
    {
        Reconcile(player);
        SendSlotPaths(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (paths.empty())
            return;
        CharacterPaths* data = player->CustomData.Get<CharacterPaths>(DataKey);
        if (!data || !data->loaded)
            return;
        data->sinceSweep += diff;
        if (!data->dirty && data->sinceSweep < SweepIntervalMs)
            return;
        data->dirty = false;
        data->sinceSweep = 0;
        Reconcile(player);
        PickBotPath(player);
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CharacterDatabase.Execute("DELETE FROM character_subclass_path WHERE guid = {}", guid.GetCounter());
        CharacterDatabase.Execute("DELETE FROM character_subclass_fee WHERE guid = {}", guid.GetCounter());
    }
};

class SubclassPathsCommandScript : public CommandScript
{
public:
    SubclassPathsCommandScript() : CommandScript("SubclassPathsCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable subclassTable = {
            {"info", HandleInfo, SEC_GAMEMASTER, Console::No},
            {"set", HandleSet, SEC_GAMEMASTER, Console::No},
            {"list", HandleList, SEC_GAMEMASTER, Console::Yes},
            {"reload", HandleReload, SEC_ADMINISTRATOR, Console::Yes}};
        static ChatCommandTable commandTable = {{"subclass", subclassTable}};
        return commandTable;
    }

    // The selected player's path in each slot they own
    static bool HandleInfo(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        CharacterPaths* data = PathsOf(target);
        for (uint8 group = 0; group < target->GetSpecsCount() && group < MAX_TALENT_SPECS; ++group)
            handler->PSendSysMessage("{} slot {}{}: {} ({})", target->GetName(), group + 1,
                                     group == target->GetActiveSpec() ? " (active)" : "", PathName(data->group[group]),
                                     data->group[group]);
        return true;
    }

    // Sets the selected player's active slot to a path (0 for the plain class), free of charge
    static bool HandleSet(ChatHandler* handler, uint16 pathId)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (std::string const reason = ChangePath(target, pathId, true); !reason.empty())
        {
            handler->SendErrorMessage(reason, false);
            return false;
        }
        handler->PSendSysMessage("{} slot {} now follows {}.", target->GetName(), target->GetActiveSpec() + 1,
                                 PathName(pathId));
        return true;
    }

    static bool HandleList(ChatHandler* handler)
    {
        for (auto const& [id, path] : paths)
            handler->PSendSysMessage("{}: {} (class {}, level {}, era {}, {} spell swaps, talent tab {})", id,
                                     path.name, path.rules.classId, path.rules.unlockLevel, path.rules.era,
                                     path.swaps.size(), path.talentTab);
        return true;
    }

    // Online players are put in line on their next update; replacements of rows removed here stay learned.
    static bool HandleReload(ChatHandler* handler)
    {
        LoadPaths();
        for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
        {
            MarkDirty(player);
            SendSlotPaths(player);
        }
        handler->PSendSysMessage("Loaded {} sub-class paths.", paths.size());
        return true;
    }
};

void AddSC_subclass_paths()
{
    new SubclassPathsWorldScript();
    new SubclassPathsPlayerScript();
    new SubclassPathsCommandScript();
}
