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

// The Witch Doctor's unlock quests (data/sql/custom/db_world/subclasses/2026_11_16_02_witch_doctor_npcs.sql).
namespace
{
// "Feeding the Loa": creatures worth experience, killed while the shaman's own Flame Shock burns them
constexpr uint32 QuestFeedingTheLoa = 990801;
constexpr uint32 NpcLoaOffering = 990805;
// Flame Shock's family flag, which Voodoo Shock keeps; Lava Burst reads the same one
constexpr uint32 FlameShockFamilyFlag = 0x10000000;
}

class witch_doctor_quests_player : public PlayerScript
{
public:
    witch_doctor_quests_player() : PlayerScript("witch_doctor_quests_player", {PLAYERHOOK_ON_GIVE_EXP}) { }

    // Experience is handed to each group member before the victim's auras go with its death, so the burn is still
    // there to see. Only kills worth experience come here, as the quest asks.
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource) override
    {
        if (xpSource != PlayerXPSource::XPSOURCE_KILL || !amount || !victim || !victim->IsCreature() ||
            player->GetQuestStatus(QuestFeedingTheLoa) != QUEST_STATUS_INCOMPLETE)
            return;
        if (victim->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_SHAMAN, FlameShockFamilyFlag, 0, 0,
                                  player->GetGUID()))
            player->KilledMonsterCredit(NpcLoaOffering);
    }
};

void AddSC_witch_doctor_quests()
{
    new witch_doctor_quests_player();
}
