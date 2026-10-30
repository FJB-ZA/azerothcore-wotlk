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
#include <cmath>
#include <list>
#include <vector>

#include "Chat.h"
#include "CommandScript.h"
#include "Creature.h"
#include "DBCEnums.h"
#include "Group.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"

using namespace Acore::ChatCommands;

// The nameplate test lineup for the solo-wow Nameplates addon: `.nameplates test` summons one creature per thing a
// nameplate shows (data/sql/custom/db_world/2026_10_23_03_nameplate_test.sql) in rows in front of the GM, and this
// AI keeps each one showing it: casting, channelling, wearing its debuffs and buffs, or staying at low health.
namespace
{
enum NameplateTestCreature : uint32
{
    NPC_TRIVIAL = 990900,
    NPC_EASY = 990901,
    NPC_EVEN = 990902,
    NPC_TOUGH = 990903,
    NPC_DEADLY = 990904,
    NPC_ELITE = 990905,
    NPC_RARE = 990906,
    NPC_RARE_ELITE = 990907,
    NPC_BOSS = 990908,
    NPC_HEALER = 990910,
    NPC_CHANNELER = 990911,
    NPC_DUELIST = 990912,
    NPC_AFFLICTED = 990915,
    NPC_WARDED = 990916,
    NPC_WOUNDED = 990917,
    NPC_DYING = 990918,
    NPC_NEUTRAL = 990920,
    NPC_FRIENDLY = 990921,
    NPC_MARKED = 990922
};

// Spells as the world's own creatures cast them
enum NameplateTestSpell : uint32
{
    SPELL_HEAL = 2054,               // 3 s cast
    SPELL_EVOCATION = 12051,         // 8 s channel
    SPELL_FROSTBOLT = 9672,
    SPELL_FAERIE_FIRE = 6950,        // magic
    SPELL_CURSE_OF_WEAKNESS = 702,   // curse
    SPELL_POISON = 744,              // poison
    SPELL_RABIES = 3150,             // disease
    SPELL_SUNDER_ARMOR = 7386,       // no type, stacks
    SPELL_THORNS = 467,
    SPELL_FORTITUDE = 1243,
    SPELL_RENEW = 139,
    SPELL_LIGHTNING_SHIELD = 12550
};

constexpr uint8 SunderStacks = 3;
constexpr uint8 SkullIcon = 7;
constexpr float RowDistance = 7.0f;
constexpr float ColumnSpacing = 5.0f;
constexpr float FirstRowDistance = 10.0f;
constexpr float SearchRange = 80.0f;
constexpr uint32 LifetimeMs = 30 * MINUTE * IN_MILLISECONDS;

struct LineupSlot
{
    uint32 entry;
    int8 levelOffset;  // against the GM's level
    uint8 row;
    uint8 column;
};

// Row 1: level colours, row 2: classifications, row 3: casts and health, row 4: auras and reactions
constexpr std::array<LineupSlot, 19> Lineup = {{
    {NPC_TRIVIAL, -10, 0, 0}, {NPC_EASY, -4, 0, 1}, {NPC_EVEN, 0, 0, 2}, {NPC_TOUGH, 3, 0, 3}, {NPC_DEADLY, 7, 0, 4},
    {NPC_ELITE, 0, 1, 0}, {NPC_RARE, 0, 1, 1}, {NPC_RARE_ELITE, 0, 1, 2}, {NPC_BOSS, 3, 1, 3}, {NPC_MARKED, 0, 1, 4},
    {NPC_HEALER, 0, 2, 0}, {NPC_CHANNELER, 0, 2, 1}, {NPC_DUELIST, 0, 2, 2}, {NPC_WOUNDED, 0, 2, 3},
    {NPC_DYING, 0, 2, 4},
    {NPC_AFFLICTED, 0, 3, 0}, {NPC_WARDED, 0, 3, 1}, {NPC_NEUTRAL, 0, 3, 2}, {NPC_FRIENDLY, 0, 3, 3}
}};

constexpr std::array<uint32, 4> CasterEntries = {NPC_HEALER, NPC_CHANNELER, NPC_DUELIST, NPC_BOSS};

std::vector<uint32> LineupEntries()
{
    std::vector<uint32> entries;
    for (LineupSlot const& slot : Lineup)
        entries.push_back(slot.entry);
    return entries;
}

uint8 ColumnsInRow(uint8 row)
{
    uint8 columns = 0;
    for (LineupSlot const& slot : Lineup)
        if (slot.row == row)
            ++columns;
    return columns;
}

// Adds the aura when it's missing, so it counts down and comes back; skips spells the server doesn't have
void KeepAura(Creature* me, uint32 spellId, uint8 stacks = 1)
{
    if (!sSpellMgr->GetSpellInfo(spellId))
        return;
    Aura* aura = me->GetAura(spellId);
    if (!aura)
        me->AddAura(spellId, me);
    else if (aura->GetStackAmount() < stacks)
        aura->ModStackAmount(1);
}

void KeepHealthAt(Creature* me, float pct)
{
    uint32 const health = me->CountPctFromMaxHealth(pct);
    if (me->GetHealth() > health + 1)
        me->SetHealth(health);
}

}  // namespace

struct npc_nameplate_test : public ScriptedAI
{
    npc_nameplate_test(Creature* creature) : ScriptedAI(creature) { }

    // Stands where it was put and fights back only when attacked, so the lineup doesn't pull itself
    void Reset() override
    {
        me->SetReactState(REACT_DEFENSIVE);
        if (me->GetEntry() == NPC_WOUNDED || me->GetEntry() == NPC_DYING)
            me->SetRegeneratingHealth(false);

        scheduler.CancelAll();
        scheduler.Schedule(1s, [this](TaskContext context)
        {
            Show();
            context.Repeat(me->GetEntry() == NPC_CHANNELER ? 10s : 4s);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        if (!UpdateVictim())
            return;
        DoMeleeAttackIfReady();
    }

private:
    void CastSelf(uint32 spellId)
    {
        if (sSpellMgr->GetSpellInfo(spellId) && !me->HasUnitState(UNIT_STATE_CASTING))
            DoCastSelf(spellId);
    }

    void Show()
    {
        switch (me->GetEntry())
        {
            case NPC_HEALER:
                CastSelf(SPELL_HEAL);
                break;
            case NPC_CHANNELER:
                CastSelf(SPELL_EVOCATION);
                break;
            case NPC_DUELIST:
            case NPC_BOSS:
                if (me->GetVictim() && !me->HasUnitState(UNIT_STATE_CASTING))
                    DoCastVictim(SPELL_FROSTBOLT);
                break;
            case NPC_AFFLICTED:
                KeepAura(me, SPELL_FAERIE_FIRE);
                KeepAura(me, SPELL_CURSE_OF_WEAKNESS);
                KeepAura(me, SPELL_POISON);
                KeepAura(me, SPELL_RABIES);
                KeepAura(me, SPELL_SUNDER_ARMOR, SunderStacks);
                break;
            case NPC_WARDED:
                KeepAura(me, SPELL_THORNS);
                KeepAura(me, SPELL_FORTITUDE);
                KeepAura(me, SPELL_RENEW);
                KeepAura(me, SPELL_LIGHTNING_SHIELD);
                KeepAura(me, SPELL_CURSE_OF_WEAKNESS);
                break;
            case NPC_FRIENDLY:
                // A friendly plate shows debuffs only: the buff must not show
                KeepAura(me, SPELL_FAERIE_FIRE);
                KeepAura(me, SPELL_THORNS);
                break;
            case NPC_WOUNDED:
                KeepHealthAt(me, 35.0f);
                break;
            case NPC_DYING:
                KeepHealthAt(me, 8.0f);
                break;
            default:
                break;
        }
    }
};

class nameplate_test_commandscript : public CommandScript
{
public:
    nameplate_test_commandscript() : CommandScript("nameplate_test_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable nameplatesTable = {
            {"test", HandleTest, SEC_GAMEMASTER, Console::No},
            {"clear", HandleClear, SEC_GAMEMASTER, Console::No},
            {"interrupt", HandleInterrupt, SEC_GAMEMASTER, Console::No}};
        static ChatCommandTable commandTable = {{"nameplates", nameplatesTable}};
        return commandTable;
    }

    // Rows of test creatures in front of the GM, facing them
    static bool HandleTest(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        ClearAround(player);

        float const facing = player->GetOrientation();
        float const forwardX = std::cos(facing);
        float const forwardY = std::sin(facing);
        uint32 summoned = 0;
        for (LineupSlot const& slot : Lineup)
        {
            float const across = (slot.column - (ColumnsInRow(slot.row) - 1) / 2.0f) * ColumnSpacing;
            float const ahead = FirstRowDistance + slot.row * RowDistance;
            float const x = player->GetPositionX() + forwardX * ahead + forwardY * across;
            float const y = player->GetPositionY() + forwardY * ahead - forwardX * across;
            float z = player->GetPositionZ();
            player->UpdateGroundPositionZ(x, y, z);

            float const towardPlayer = Position::NormalizeOrientation(facing + float(M_PI));
            TempSummon* creature = player->SummonCreature(slot.entry, x, y, z, towardPlayer,
                                                          TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, LifetimeMs);
            if (!creature)
                continue;
            ++summoned;

            int32 const level = std::clamp<int32>(int32(player->GetLevel()) + slot.levelOffset, 1, DEFAULT_MAX_LEVEL);
            creature->SetLevel(uint8(level));
            creature->SetFullHealth();
            if (slot.entry == NPC_MARKED)
                if (Group* group = player->GetGroup())
                    group->SetTargetIcon(SkullIcon, player->GetGUID(), creature->GetGUID());
        }

        handler->PSendSysMessage("Nameplate test: {} creatures for 30 minutes. Rows from the front: level colours; "
                                 "elite, rare, rare elite, boss and a skull marker; a cast, a channel, a caster that "
                                 "fights back and low health; debuffs, buffs, neutral and friendly. "
                                 ".nameplates interrupt stops the casters, .nameplates clear removes them.", summoned);
        return true;
    }

    static bool HandleClear(ChatHandler* handler)
    {
        handler->PSendSysMessage("Nameplate test: removed {} creatures.", ClearAround(handler->GetPlayer()));
        return true;
    }

    // Lets a GM without an interrupt of their own see an interrupted cast bar
    static bool HandleInterrupt(ChatHandler* handler)
    {
        uint32 interrupted = 0;
        for (uint32 entry : CasterEntries)
        {
            std::list<Creature*> casters;
            handler->GetPlayer()->GetCreatureListWithEntryInGrid(casters, entry, SearchRange);
            for (Creature* caster : casters)
            {
                if (!caster->IsNonMeleeSpellCast(false))
                    continue;
                caster->InterruptNonMeleeSpells(false);
                ++interrupted;
            }
        }
        handler->PSendSysMessage("Nameplate test: interrupted {} casts.", interrupted);
        return true;
    }

private:
    static uint32 ClearAround(Player* player)
    {
        std::list<Creature*> creatures;
        player->GetCreatureListWithEntryInGrid(creatures, LineupEntries(), SearchRange);
        for (Creature* creature : creatures)
            creature->DespawnOrUnsummon();
        return uint32(creatures.size());
    }
};

void AddSC_nameplate_test()
{
    RegisterCreatureAI(npc_nameplate_test);
    new nameplate_test_commandscript();
}
