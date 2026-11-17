/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Creature.h"
#include "Player.h"
#include "ScriptMgr.h"

// The Starcaller's unlock quests (data/sql/custom/db_world/subclasses/2026_11_17_02_starcaller_npcs.sql).
namespace
{
// "By Moonlight": creatures worth experience, killed while the druid's own Moonfire burns them
constexpr uint32 QuestByMoonlight = 990651;
constexpr uint32 NpcSlainByMoonlight = 990655;
// Moonfire's family flag
constexpr uint32 MoonfireFamilyFlag = 0x2;
}

class starcaller_quests_player : public PlayerScript
{
public:
    starcaller_quests_player() : PlayerScript("starcaller_quests_player", {PLAYERHOOK_ON_GIVE_EXP}) { }

    // Experience is handed to each group member before the victim's auras go with its death, so the burn is still
    // there to see. Only kills worth experience come here, as the quest asks.
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource) override
    {
        if (xpSource != PlayerXPSource::XPSOURCE_KILL || !amount || !victim || !victim->IsCreature() ||
            player->GetQuestStatus(QuestByMoonlight) != QUEST_STATUS_INCOMPLETE)
            return;
        if (victim->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_DRUID, MoonfireFamilyFlag, 0, 0,
                                  player->GetGUID()))
            player->KilledMonsterCredit(NpcSlainByMoonlight);
    }
};

void AddSC_starcaller_quests()
{
    new starcaller_quests_player();
}
