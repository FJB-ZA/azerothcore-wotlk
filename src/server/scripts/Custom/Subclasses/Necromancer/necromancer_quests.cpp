/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Creature.h"
#include "Group.h"
#include "Player.h"
#include "ScriptMgr.h"

// The Necromancer's unlock quests (data/sql/custom/db_world/subclasses/2026_10_23_02_necromancer_npcs.sql).
namespace
{
// "The Last Breath": every kill worth experience counts
constexpr uint32 QuestLastBreath = 990371;
constexpr uint32 NpcLastBreath = 990375;
}

class necromancer_quests_player : public PlayerScript
{
public:
    necromancer_quests_player() : PlayerScript("necromancer_quests_player", {PLAYERHOOK_ON_CREATURE_KILL_CREDIT}) { }

    // Everyone in the group near the kill hears it, as with any kill quest
    void OnPlayerCreatureKillCredit(Player* player, Creature* killed) override
    {
        auto credit = [killed](Player* member)
        {
            if (member->GetQuestStatus(QuestLastBreath) == QUEST_STATUS_INCOMPLETE && member->isHonorOrXPTarget(killed))
                member->KilledMonsterCredit(NpcLastBreath);
        };
        Group* group = player->GetGroup();
        if (!group)
        {
            credit(player);
            return;
        }
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource())
                if (member->IsAtGroupRewardDistance(killed))
                    credit(member);
    }
};

void AddSC_necromancer_quests()
{
    new necromancer_quests_player();
}
