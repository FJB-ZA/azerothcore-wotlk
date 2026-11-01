/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <string>

#include "Chat.h"
#include "Creature.h"
#include "GossipDef.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "SubclassPaths.h"

// npc_subclass_path: the gossip a path's unlock NPC (creature_template.ScriptName) uses to set the active spec slot's
// path for the path change fee. It offers every path of the player's class; the path's rules decide at the click.
namespace
{
constexpr uint32 GossipSender = GOSSIP_SENDER_MAIN + 40;
}

class npc_subclass_path : public CreatureScript
{
public:
    npc_subclass_path() : CreatureScript("npc_subclass_path") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        uint16 const current = SubclassPaths::ActivePathId(player);
        uint32 const fee = SubclassPaths::NextChangeFee(player);
        std::string const slot = "spec slot " + std::to_string(player->GetActiveSpec() + 1);
        for (SubclassPaths::Path const* path : SubclassPaths::ClassPaths(player->getClass()))
        {
            if (path->id == current)
                continue;
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Follow the " + path->name + " path in " + slot + ".",
                             GossipSender, path->id + 1u, "Your " + slot + " will follow the " + path->name + " path.", fee,
                             false);
        }
        if (current != SubclassPaths::PlainClass)
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Leave your path in " + slot + ".", GossipSender,
                             SubclassPaths::PlainClass + 1u, "Your " + slot + " will go back to your plain class.", fee,
                             false);

        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 sender, uint32 action) override
    {
        // Close first, so a second click on the same window finds nothing to pay for.
        CloseGossipMenuFor(player);
        if (sender != GossipSender || !action)
            return true;
        uint16 const pathId = uint16(action - 1);
        if (std::string const reason = SubclassPaths::ChangeActivePath(player, pathId); !reason.empty())
            ChatHandler(player->GetSession()).SendNotification(reason);
        return true;
    }

    // A path's unlock quest, handed in here, puts a plain active slot on that path for free
    bool OnQuestReward(Player* player, Creature* /*creature*/, Quest const* quest, uint32 /*opt*/) override
    {
        if (SubclassPaths::ActivePathId(player) != SubclassPaths::PlainClass)
            return false;
        for (SubclassPaths::Path const* path : SubclassPaths::ClassPaths(player->getClass()))
        {
            if (path->unlockQuest != quest->GetQuestId())
                continue;
            if (SubclassPaths::ChangeActivePath(player, path->id, true).empty())
                ChatHandler(player->GetSession())
                    .PSendSysMessage("Your spec slot {} now follows the {} path.", player->GetActiveSpec() + 1, path->name);
            break;
        }
        return false;
    }
};

void AddSC_subclass_npc()
{
    new npc_subclass_path();
}
