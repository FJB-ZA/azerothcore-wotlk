/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "NPCPackets.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Subclass.h"
#include "SubclassPolicy.h"
#include "Trainer.h"
#include "WorldPacket.h"
#include "WorldSession.h"

// Sub-class paths (rules in SubclassPolicy.h, data in data/sql/custom/db_world/solo_wow_subclass.sql).
//
// A path belongs to a spec slot (talent group). The core already keeps every learned spell with a mask of the talent
// groups it is in, and switching groups shows the client exactly the difference. So a path is applied by masks: in a
// group on a path, the swapped originals leave that group and their mirrors join it. Slot swaps then need no work of
// their own, and a crash can't leave a slot half on a path: the masks are saved with the character's spells.
namespace
{
using Subclass::Swap;

struct PathData
{
    uint8 id = 0;
    std::string name;
    uint8 baseClass = 0;
    uint8 unlockLevel = 0;
    uint8 unlockExpansion = 0;
    uint8 botSharePct = 0;
    float pveDamage = 1.0f;
    float pveHealing = 1.0f;
    float pvpDamage = 1.0f;
    float pvpHealing = 1.0f;
    std::vector<Swap> spells;
    std::vector<std::pair<uint32, uint32>> creatures;  // original entry, replacement entry
};

using PathMap = std::map<uint8, PathData>;

std::mutex pathsLock;
std::shared_ptr<PathMap const> paths = std::make_shared<PathMap const>();
std::atomic<bool> anyModifier = false;

// Each online character's path per talent group, guarded: the hooks run on map threads
std::mutex charactersLock;
std::unordered_map<ObjectGuid::LowType, std::array<uint8, MAX_TALENT_SPECS>> characters;

std::shared_ptr<PathMap const> Paths()
{
    std::lock_guard<std::mutex> guard(pathsLock);
    return paths;
}

void LoadPaths()
{
    auto loaded = std::make_shared<PathMap>();
    if (QueryResult result =
            WorldDatabase.Query("SELECT id, name, base_class, unlock_level, unlock_expansion, bot_share_pct, "
                                "pve_damage_mod, pve_healing_mod, pvp_damage_mod, pvp_healing_mod FROM subclass_path"))
    {
        do
        {
            Field* fields = result->Fetch();
            PathData path;
            path.id = fields[0].Get<uint8>();
            path.name = fields[1].Get<std::string>();
            path.baseClass = fields[2].Get<uint8>();
            path.unlockLevel = fields[3].Get<uint8>();
            path.unlockExpansion = fields[4].Get<uint8>();
            path.botSharePct = fields[5].Get<uint8>();
            path.pveDamage = fields[6].Get<float>();
            path.pveHealing = fields[7].Get<float>();
            path.pvpDamage = fields[8].Get<float>();
            path.pvpHealing = fields[9].Get<float>();
            if (path.id == 0)
            {
                LOG_ERROR("sql.sql", "subclass_path: id 0 is the plain class; row skipped.");
                continue;
            }
            (*loaded)[path.id] = path;
        } while (result->NextRow());
    }

    if (QueryResult result =
            WorldDatabase.Query("SELECT path_id, original_spell, replacement_spell FROM subclass_spell_swap "
                                "ORDER BY path_id, original_spell"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const pathId = fields[0].Get<uint8>();
            uint32 const original = fields[1].Get<uint32>();
            uint32 const replacement = fields[2].Get<uint32>();
            auto path = loaded->find(pathId);
            if (path == loaded->end())
                LOG_ERROR("sql.sql", "subclass_spell_swap: path {} doesn't exist; row skipped.", pathId);
            else if (!sSpellMgr->GetSpellInfo(original) || !sSpellMgr->GetSpellInfo(replacement))
                LOG_ERROR("sql.sql",
                          "subclass_spell_swap: path {} swaps {} for {}, and one of them is not a known spell "
                          "(run .\\wow.ps1 spells in solo-wow); row skipped.",
                          pathId, original, replacement);
            else
                path->second.spells.push_back({original, replacement});
        } while (result->NextRow());
    }

    if (QueryResult result =
            WorldDatabase.Query("SELECT path_id, original_entry, replacement_entry FROM subclass_creature_swap"))
    {
        do
        {
            Field* fields = result->Fetch();
            auto path = loaded->find(fields[0].Get<uint8>());
            if (path != loaded->end())
                path->second.creatures.emplace_back(fields[1].Get<uint32>(), fields[2].Get<uint32>());
        } while (result->NextRow());
    }

    bool modifier = false;
    for (auto const& [id, path] : *loaded)
    {
        modifier = modifier || path.pveDamage != 1.0f || path.pveHealing != 1.0f || path.pvpDamage != 1.0f ||
                   path.pvpHealing != 1.0f;
        LOG_INFO("server.loading", ">> Sub-class path {} ({}): {} spell swaps, {} pet swaps", id, path.name,
                 path.spells.size(), path.creatures.size());
    }

    std::lock_guard<std::mutex> guard(pathsLock);
    paths = loaded;
    anyModifier = modifier;
}

std::array<uint8, MAX_TALENT_SPECS> GroupsOf(Player* player)
{
    std::lock_guard<std::mutex> guard(charactersLock);
    auto found = characters.find(player->GetGUID().GetCounter());
    return found != characters.end() ? found->second : std::array<uint8, MAX_TALENT_SPECS>{};
}

uint8 ActivePath(Player* player) { return GroupsOf(player)[player->GetActiveSpec()]; }

uint8 SpecMaskOf(Player* player, uint32 spellId)
{
    PlayerSpellMap const& spells = player->GetSpellMap();
    auto found = spells.find(spellId);
    if (found == spells.end() || found->second->State == PLAYERSPELL_REMOVED)
        return 0;
    return found->second->specMask;
}

// Give `spellId` exactly the talent groups in `want`, telling the client only about its active group
void SetSpecMask(Player* player, uint32 spellId, uint8 current, uint8 want)
{
    uint8 const active = player->GetActiveSpecMask();
    if (uint8 const remove = current & ~want)
        player->removeSpell(spellId, remove, false, (remove & active) != 0);
    if (uint8 const add = want & ~current)
        if (player->addSpell(spellId, add, true) && (add & active) && player->IsInWorld())
            player->SendLearnPacket(spellId, true);
}

// Normalize adds and removes spells itself, which must not start another Normalize (per map thread)
thread_local bool reentry = false;

// Bring every path's spells in line with the character's groups: in a group on path P, P's originals are absent and
// their mirrors present; in every other group the reverse. Idempotent; runs on login, slot swaps, path changes and
// whenever a swapped spell is learned.
void Normalize(Player* player)
{
    if (reentry)
        return;
    reentry = true;
    std::shared_ptr<PathMap const> const all = Paths();
    std::array<uint8, MAX_TALENT_SPECS> const groups = GroupsOf(player);
    for (auto const& [id, path] : *all)
    {
        uint8 onPath = 0;
        for (uint8 group = 0; group < MAX_TALENT_SPECS; ++group)
            if (groups[group] == id && path.baseClass == player->getClass())
                onPath |= uint8(1 << group);

        // A mirror shared by several originals (Entomb for Fear's ranks) can't tell which ranks to give back; when
        // none of them is still known anywhere, the ranks the character's level allows come back.
        std::map<uint32, bool> anyOriginalKnown;
        for (Swap const& swap : path.spells)
            anyOriginalKnown[swap.replacement] |= SpecMaskOf(player, swap.original) != 0;
        std::map<uint32, uint8> mirrorWant;

        for (Swap const& swap : path.spells)
        {
            uint8 const original = SpecMaskOf(player, swap.original);
            uint8 const mirror = SpecMaskOf(player, swap.replacement);
            if (!original && !mirror)
                continue;
            uint8 wantOriginal = uint8(SPEC_MASK_ALL & ~onPath);
            if (!original && anyOriginalKnown[swap.replacement])
                wantOriginal = 0;  // another rank of a shared mirror is still known; leave this one unlearned
            else if (!original)
            {
                SpellInfo const* info = sSpellMgr->GetSpellInfo(swap.original);
                if (!info || info->SpellLevel > player->GetLevel())
                    wantOriginal = 0;
            }
            SetSpecMask(player, swap.original, original, wantOriginal);
            mirrorWant[swap.replacement] |= onPath;
        }
        for (auto const& [mirror, want] : mirrorWant)
            SetSpecMask(player, mirror, SpecMaskOf(player, mirror), want);
    }
    reentry = false;
}

// Action buttons of the active group: originals to mirrors on the path, mirrors back to originals off it
void MapActionBars(Player* player, uint8 fromPath, uint8 toPath)
{
    std::shared_ptr<PathMap const> const all = Paths();
    std::vector<uint32> known;
    for (auto const& [spellId, spell] : player->GetSpellMap())
        if (spell->State != PLAYERSPELL_REMOVED && spell->IsInSpec(player->GetActiveSpec()))
            known.push_back(spellId);

    for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
    {
        ActionButton const* action = player->GetActionButton(button);
        if (!action || action->GetType() != ACTION_BUTTON_SPELL)
            continue;
        uint32 spellId = action->GetAction();
        if (auto from = all->find(fromPath); fromPath && from != all->end())
            spellId = Subclass::MapAction(from->second.spells, spellId, false, known);
        if (auto to = all->find(toPath); toPath && to != all->end())
            spellId = Subclass::MapAction(to->second.spells, spellId, true, known);
        if (spellId != action->GetAction())
            player->addActionButton(button, spellId, ACTION_BUTTON_SPELL);
    }
    player->SendActionButtons(1);
}

// A pet that belongs to another path (or to the plain class while on a path) goes away; the warlock summons anew
void DismissForeignPet(Player* player)
{
    Pet* pet = player->GetPet();
    if (!pet)
        return;
    uint32 const entry = pet->GetEntry();
    uint8 const active = ActivePath(player);
    std::shared_ptr<PathMap const> const all = Paths();
    for (auto const& [id, path] : *all)
        for (auto const& [original, replacement] : path.creatures)
            if ((id == active && entry == original) || (id != active && entry == replacement))
            {
                player->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
                return;
            }
}

void LoadCharacter(Player* player)
{
    std::array<uint8, MAX_TALENT_SPECS> groups{};
    if (QueryResult result =
            CharacterDatabase.Query("SELECT talent_group, path_id FROM character_subclass_path WHERE guid = {}",
                                    player->GetGUID().GetCounter()))
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const group = fields[0].Get<uint8>();
            if (group < MAX_TALENT_SPECS)
                groups[group] = fields[1].Get<uint8>();
        } while (result->NextRow());
    }
    std::lock_guard<std::mutex> guard(charactersLock);
    characters[player->GetGUID().GetCounter()] = groups;
}

float Modifier(Unit* source, Unit* target, bool healing)
{
    Player* player = source ? source->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    if (!player)
        return 1.0f;
    uint8 const active = ActivePath(player);
    if (!active)
        return 1.0f;
    std::shared_ptr<PathMap const> const all = Paths();
    auto path = all->find(active);
    if (path == all->end())
        return 1.0f;
    bool const pvp = target && target->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (healing)
        return pvp ? path->second.pvpHealing : path->second.pveHealing;
    return pvp ? path->second.pvpDamage : path->second.pveDamage;
}
}  // namespace

// Shared with the unlock NPC and the SpecSlots addon message (Subclass.h)
uint8 Subclass_GetPath(Player* player, uint8 group) { return group < MAX_TALENT_SPECS ? GroupsOf(player)[group] : 0; }

std::string Subclass_PathName(uint8 pathId)
{
    std::shared_ptr<PathMap const> const all = Paths();
    auto path = all->find(pathId);
    return path != all->end() ? path->second.name : std::string();
}

std::vector<uint8> Subclass_PathsFor(Player* player)
{
    std::vector<uint8> list;
    std::shared_ptr<PathMap const> const all = Paths();
    for (auto const& [id, path] : *all)
        if (path.baseClass == player->getClass())
            list.push_back(id);
    return list;
}

uint8 Subclass_UnlockLevel(uint8 pathId)
{
    std::shared_ptr<PathMap const> const all = Paths();
    auto path = all->find(pathId);
    return path != all->end() ? path->second.unlockLevel : 0;
}

uint32 Subclass_NextFee(Player* player)
{
    uint32 lastFee = 0;
    uint32 months = 0;
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT last_fee, last_change FROM character_subclass_fee WHERE guid = {}", player->GetGUID().GetCounter()))
    {
        Field* fields = result->Fetch();
        lastFee = fields[0].Get<uint32>();
        uint32 const lastChange = fields[1].Get<uint32>();
        uint32 const now = uint32(GameTime::GetGameTime().count());
        months = now > lastChange ? (now - lastChange) / MONTH : 0;
    }
    return Subclass::NextChangeFee(lastFee, months);
}

Subclass::ChangeCheck Subclass_ChangePath(Player* player, uint8 target, bool charge)
{
    uint8 const group = player->GetActiveSpec();
    uint8 const current = Subclass_GetPath(player, group);
    std::shared_ptr<PathMap const> const all = Paths();
    auto path = all->find(target);
    bool const fits = target == 0 || (path != all->end() && path->second.baseClass == player->getClass());
    uint8 const unlock = path != all->end() ? path->second.unlockLevel : 0;
    uint32 const fee = charge ? Subclass_NextFee(player) : 0;
    Subclass::ChangeCheck const check = Subclass::CanChange(current, target, fits, player->GetLevel(), unlock,
                                                            player->IsInCombat(), player->GetMoney(), fee);
    if (check != Subclass::ChangeCheck::Ok)
        return check;

    {
        std::lock_guard<std::mutex> guard(charactersLock);
        characters[player->GetGUID().GetCounter()][group] = target;
    }
    if (charge)
        player->ModifyMoney(-int32(fee));
    Normalize(player);
    MapActionBars(player, current, target);
    DismissForeignPet(player);

    // The path row, the fee counter and the character's spells and bars go to the database together
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
    if (target)
        trans->Append("REPLACE INTO character_subclass_path (guid, talent_group, path_id) VALUES ({}, {}, {})", guid,
                      group, target);
    else
        trans->Append("DELETE FROM character_subclass_path WHERE guid = {} AND talent_group = {}", guid, group);
    if (charge)
        trans->Append("REPLACE INTO character_subclass_fee (guid, last_fee, last_change) VALUES ({}, {}, {})", guid,
                      fee, uint32(GameTime::GetGameTime().count()));
    player->SaveToDB(trans, false, false);
    CharacterDatabase.CommitTransaction(trans);
    return check;
}

uint32 Subclass_MirrorOf(Player* player, uint32 spellId)
{
    uint8 const active = ActivePath(player);
    if (!active)
        return 0;
    std::shared_ptr<PathMap const> const all = Paths();
    auto path = all->find(active);
    if (path == all->end())
        return 0;
    for (Swap const& swap : path->second.spells)
        if (swap.original == spellId)
            return swap.replacement;
    return 0;
}

uint32 Subclass_BonusSpell(Unit const* caster, uint32 spellId)
{
    Player* player = caster ? caster->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    if (!player || spellId < Subclass::FirstPathSpell || spellId > Subclass::LastCustomSpell)
        return spellId;
    uint8 const active = ActivePath(player);
    std::shared_ptr<PathMap const> const all = Paths();
    auto path = all->find(active);
    return path != all->end() ? Subclass::BonusSpell(path->second.spells, spellId) : spellId;
}

std::vector<std::pair<uint32, uint32>> Subclass_AllSwaps(uint8 pathId)
{
    std::vector<std::pair<uint32, uint32>> list;
    std::shared_ptr<PathMap const> const all = Paths();
    if (auto path = all->find(pathId); path != all->end())
        for (Swap const& swap : path->second.spells)
            list.emplace_back(swap.original, swap.replacement);
    return list;
}

class SubclassWorldScript : public WorldScript
{
public:
    SubclassWorldScript() : WorldScript("SubclassWorldScript", {WORLDHOOK_ON_STARTUP}) {}

    void OnStartup() override { LoadPaths(); }
};

class SubclassPlayerScript : public PlayerScript
{
public:
    SubclassPlayerScript()
        : PlayerScript("SubclassPlayerScript",
                       {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_AFTER_SPEC_SLOT_CHANGED,
                        PLAYERHOOK_ON_LEARN_SPELL, PLAYERHOOK_ON_DELETE_FROM_DB,
                        PLAYERHOOK_ON_BEFORE_RECEIVE_SPELL_LIST_FROM_TRAINER, PLAYERHOOK_ON_GET_TRAINER_SPELL_STATE})
    {
    }

    void OnPlayerLogin(Player* player) override
    {
        LoadCharacter(player);
        Normalize(player);
        DismissForeignPet(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        std::lock_guard<std::mutex> guard(charactersLock);
        characters.erase(player->GetGUID().GetCounter());
    }

    // The core's own switch already showed the new group's spells; this only repairs stale masks and the pet
    void OnPlayerAfterSpecSlotChanged(Player* player, uint8 /*group*/) override
    {
        Normalize(player);
        DismissForeignPet(player);
    }

    // A new rank from a trainer or a level-up joins every group, so a path group swaps it at once
    void OnPlayerLearnSpell(Player* player, uint32 spellId) override
    {
        std::shared_ptr<PathMap const> const all = Paths();
        for (auto const& [id, path] : *all)
            for (Swap const& swap : path.spells)
                if (swap.original == spellId || swap.replacement == spellId)
                {
                    Normalize(player);
                    return;
                }
    }

    // On a path, the class trainer lists the path's ranks where the originals were, at the same level and price
    void OnPlayerBeforeReceiveSpellListFromTrainer(Player* player, Creature* /*trainer*/,
                                                   WorldPackets::NPC::TrainerList& list) override
    {
        if (!ActivePath(player))
            return;
        std::vector<uint32> listed;
        auto entry = list.Spells.begin();
        while (entry != list.Spells.end())
        {
            if (uint32 const mirror = Subclass_MirrorOf(player, uint32(entry->SpellID)))
            {
                // Several originals sharing one mirror (Fear's ranks for Entomb) show it once
                if (Subclass::Contains(listed, mirror))
                {
                    entry = list.Spells.erase(entry);
                    continue;
                }
                listed.push_back(mirror);
                entry->SpellID = int32(mirror);
            }
            for (int32& ability : entry->ReqAbility)
                if (uint32 const mirror = ability > 0 ? Subclass_MirrorOf(player, uint32(ability)) : 0)
                    ability = int32(mirror);
            ++entry;
        }
    }

    // A swapped original's state on a path follows its mirror: known when the mirror is, otherwise its own level and
    // the previous rank's mirror decide. Buying it (see SubclassServerScript) teaches the original, which becomes the
    // mirror in the path's groups at once.
    void OnPlayerGetTrainerSpellState(Player const* constPlayer, uint32 /*trainerId*/, uint32 spellId,
                                      Trainer::SpellState& state) override
    {
        Player* player = const_cast<Player*>(constPlayer);
        uint32 const mirror = Subclass_MirrorOf(player, spellId);
        if (!mirror)
            return;
        if (player->HasSpell(mirror))
        {
            state = Trainer::SpellState::Known;
            return;
        }
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info || player->GetLevel() < info->SpellLevel)
        {
            state = Trainer::SpellState::Unavailable;
            return;
        }
        if (uint32 const previous = sSpellMgr->GetPrevSpellInChain(spellId))
        {
            uint32 const previousMirror = Subclass_MirrorOf(player, previous);
            if (!player->HasSpell(previousMirror ? previousMirror : previous))
            {
                state = Trainer::SpellState::Unavailable;
                return;
            }
        }
        state = Trainer::SpellState::Available;
    }

    void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        trans->Append("DELETE FROM character_subclass_path WHERE guid = {}", guid);
        trans->Append("DELETE FROM character_subclass_fee WHERE guid = {}", guid);
    }
};

// Per-path damage and healing modifiers (subclass_path), 1.0 unless balance needs them. Off entirely while every
// modifier is 1.0, since these hooks run on every hit.
class SubclassUnitScript : public UnitScript
{
public:
    SubclassUnitScript()
        : UnitScript(
              "SubclassUnitScript", true,
              {UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_MELEE_DAMAGE,
               UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_HEAL_RECEIVED, UNITHOOK_ON_SPELL_BONUS_SCHOOL_MASK})
    {
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
                                       SpellInfo const* /*spellInfo*/) override
    {
        if (anyModifier)
            damage = uint32(damage * Modifier(attacker, target, false));
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (anyModifier)
            damage = uint32(damage * Modifier(attacker, target, false));
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        if (anyModifier)
            damage = int32(damage * Modifier(attacker, target, false));
    }

    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* /*spellInfo*/) override
    {
        if (anyModifier)
            heal = uint32(heal * Modifier(healer, target, true));
    }

    // A mirror's caster bonuses see the original's school: Destruction's fire talents and fire spell power count for a
    // Necromancer's frost and disease mirrors. Resistances and immunities still see the mirror's own school.
    void OnSpellBonusSchoolMask(Unit const* caster, SpellInfo const* spellInfo, uint32& schoolMask) override
    {
        if (!spellInfo || spellInfo->Id < Subclass::FirstPathSpell || spellInfo->Id > Subclass::LastCustomSpell)
            return;
        uint32 const original = Subclass_BonusSpell(caster, spellInfo->Id);
        if (original == spellInfo->Id)
            return;
        if (SpellInfo const* originalInfo = sSpellMgr->GetSpellInfo(original))
            schoolMask = originalInfo->GetSchoolMask();
    }
};

// The trainer window on a path shows mirrors; buying one teaches its original rank (Trainer::TeachSpell, with all its
// checks and price), which Normalize turns into the mirror in the path's groups.
class SubclassServerScript : public ServerScript
{
public:
    SubclassServerScript() : ServerScript("SubclassServerScript", {SERVERHOOK_CAN_PACKET_RECEIVE}) {}

    bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
    {
        if (packet.GetOpcode() != CMSG_TRAINER_BUY_SPELL || !session || !session->GetPlayer())
            return true;
        WorldPacket copy(packet);
        ObjectGuid trainerGuid;
        uint32 spellId = 0;
        copy >> trainerGuid >> spellId;
        if (spellId < Subclass::FirstPathSpell || spellId > Subclass::LastCustomSpell)
            return true;

        Player* player = session->GetPlayer();
        uint8 const active = ActivePath(player);
        uint32 original = 0;
        for (auto const& [from, to] : Subclass_AllSwaps(active))
            if (to == spellId)
            {
                original = from;
                break;
            }
        if (!active || !original)
            return true;

        Creature* npc = player->GetNPCIfCanInteractWith(trainerGuid, UNIT_NPC_FLAG_TRAINER);
        Trainer::Trainer* trainer = npc ? sObjectMgr->GetTrainer(npc->GetEntry()) : nullptr;
        if (!trainer)
            return false;
        trainer->TeachSpell(npc, player, original);
        trainer->SendSpells(npc, player, session->GetSessionDbLocaleIndex());
        return false;
    }
};

// GM commands. Players change paths at the path's trainer NPC.
class SubclassCommandScript : public CommandScript
{
public:
    SubclassCommandScript() : CommandScript("SubclassCommandScript") {}

    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        using namespace Acore::ChatCommands;
        static ChatCommandTable subclassTable = {
            {"set", HandleSet, SEC_GAMEMASTER, Console::No},
            {"info", HandleInfo, SEC_GAMEMASTER, Console::No},
            {"reload", HandleReload, SEC_ADMINISTRATOR, Console::Yes},
        };
        static ChatCommandTable commandTable = {{"subclass", subclassTable}};
        return commandTable;
    }

    // .subclass set <path id, 0 for the plain class>: the active spec slot of your target (or you), free of charge
    static bool HandleSet(ChatHandler* handler, uint8 pathId)
    {
        Player* player = handler->getSelectedPlayerOrSelf();
        if (!player)
            return false;
        switch (Subclass_ChangePath(player, pathId, false))
        {
            case Subclass::ChangeCheck::Ok:
                handler->PSendSysMessage("{}'s spec slot {} is now {}.", player->GetName(), player->GetActiveSpec() + 1,
                                         pathId ? Subclass_PathName(pathId) : std::string("the plain class"));
                return true;
            case Subclass::ChangeCheck::SamePath:
                handler->SendErrorMessage("That slot is already on that path.", false);
                return false;
            case Subclass::ChangeCheck::WrongClass:
                handler->SendErrorMessage("No such path for that class.", false);
                return false;
            case Subclass::ChangeCheck::TooLow:
                handler->SendErrorMessage("Too low for that path.", false);
                return false;
            case Subclass::ChangeCheck::InCombat:
                handler->SendErrorMessage("Not in combat.", false);
                return false;
            default:
                return false;
        }
    }

    static bool HandleInfo(ChatHandler* handler)
    {
        Player* player = handler->getSelectedPlayerOrSelf();
        if (!player)
            return false;
        for (uint8 group = 0; group < player->GetSpecsCount() && group < MAX_TALENT_SPECS; ++group)
        {
            uint8 const pathId = Subclass_GetPath(player, group);
            handler->PSendSysMessage("Slot {}{}: {}", group + 1, group == player->GetActiveSpec() ? " (active)" : "",
                                     pathId ? Subclass_PathName(pathId) : std::string("plain class"));
        }
        handler->PSendSysMessage("Next path change at the trainer costs {} gold.",
                                 Subclass_NextFee(player) / Subclass::Gold);
        return true;
    }

    static bool HandleReload(ChatHandler* handler)
    {
        LoadPaths();
        handler->SendSysMessage("Sub-class paths reloaded.");
        return true;
    }
};

void AddSC_subclass()
{
    new SubclassWorldScript();
    new SubclassPlayerScript();
    new SubclassUnitScript();
    new SubclassServerScript();
    new SubclassCommandScript();
}
