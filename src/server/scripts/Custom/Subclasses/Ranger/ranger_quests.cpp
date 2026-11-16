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

// The Ranger's unlock quests (data/sql/custom/db_world/subclasses/2026_11_15_02_ranger_npcs.sql).
namespace
{
// "The Lone Trail": beasts worth experience, killed with no pet out
constexpr uint32 QuestLoneTrail = 990571;
constexpr uint32 NpcLoneTrail = 990575;
}

class ranger_quests_player : public PlayerScript
{
public:
    ranger_quests_player() : PlayerScript("ranger_quests_player", {PLAYERHOOK_ON_CREATURE_KILL_CREDIT}) { }

    // Everyone in the group near the kill counts it, as with any kill quest, but only a hunter hunting alone
    void OnPlayerCreatureKillCredit(Player* player, Creature* killed) override
    {
        if (killed->GetCreatureType() != CREATURE_TYPE_BEAST)
            return;
        auto credit = [killed](Player* member)
        {
            if (member->GetQuestStatus(QuestLoneTrail) == QUEST_STATUS_INCOMPLETE && !member->GetPet() &&
                member->isHonorOrXPTarget(killed))
                member->KilledMonsterCredit(NpcLoneTrail);
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

void AddSC_ranger_quests()
{
    new ranger_quests_player();
}
