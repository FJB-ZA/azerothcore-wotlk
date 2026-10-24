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
#include "GameTime.h"
#include "MythicPlus.h"
#include "MythicProtocolPolicy.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"

// Mythic+ addon messages (MythicProtocolPolicy.h): the keystone, crests and week at login and on request, the run
// every second inside a run, and the result at the end. The addon's Crest Forge asks for upgrades the same way.
namespace MythicPlus
{
namespace
{
constexpr float ForgeRange = 10.0f;

void SendToClient(Player* player, std::string const& body)
{
    if (IsBot(player))
        return;
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
                                 std::string(AddonPrefix) + '\t' + body);
    player->SendDirectMessage(&data);
}

void SendAll(Player* player)
{
    SendKeystone(player);
    SendCrests(player);
    SendWeek(player);
    if (MythicRun* run = FindRun(player->GetMap()))
        SendRun(player, *run);
}
}

void SendKeystone(Player* player)
{
    Keystone const key = GetKeystone(player);
    SendToClient(player, FormatKeystone(key.map, key.level));
}

void SendCrests(Player* player)
{
    SendToClient(player, FormatCrests(Crests(player), EarnedThisWeek(player)));
}

void SendWeek(Player* player)
{
    uint32 const week = WeekIndex(uint64(GameTime::GetGameTime().count()));
    SendToClient(player, FormatWeek(week, AffixesForWeek(week)));
}

void SendRun(Player* player, MythicRun const& run)
{
    RunSnapshot snapshot;
    snapshot.state = run.state;
    snapshot.map = run.dungeon ? run.dungeon->map : 0;
    snapshot.level = run.level;
    snapshot.elapsedSeconds = uint32((run.state == RunState::Completed ? run.finalMs : run.TotalMs()) / 1000);
    snapshot.limitSeconds = run.dungeon ? run.dungeon->timerSeconds : 0;
    snapshot.deaths = run.deaths;
    snapshot.forces = run.forces;
    snapshot.quota = run.quota;
    snapshot.bosses = run.bosses;
    snapshot.requiredBosses = run.dungeon ? run.dungeon->requiredBosses : 0;
    SendToClient(player, FormatRun(snapshot));
}

void SendResult(Player* player, MythicRun const& run, Keystone const& newKey, Crest crest, uint32 crests)
{
    SendToClient(player, FormatResult(run.dungeon ? run.dungeon->map : 0, run.level, uint32(run.finalMs / 1000),
                                      run.upgrade, newKey.map, newKey.level, crest, crests));
}
}

using namespace MythicPlus;

class MythicAddonPlayerScript : public PlayerScript
{
public:
    MythicAddonPlayerScript()
        : PlayerScript("MythicAddonPlayerScript", {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT})
    {
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!IsBot(player))
            SendAll(player);
    }

    // "SoloWowMythic\tQ" asks for everything; "SoloWowMythic\tU <bag> <slot>" upgrades an item at the Keystone Master
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        std::string const prefix = std::string(AddonPrefix) + '\t';
        if (language != LANG_ADDON || type != CHAT_MSG_WHISPER || receiver != player || msg.rfind(prefix, 0) != 0)
            return true;

        std::string_view const body = std::string_view(msg).substr(prefix.size());
        if (IsQuery(body))
            SendAll(player);
        else if (std::optional<UpgradeRequest> const request = ParseUpgrade(body))
        {
            ChatHandler handler(player->GetSession());
            if (!player->FindNearestCreature(KeystoneMasterEntry, ForgeRange))
                handler.SendNotification("Visit the Keystone Master in Dalaran to upgrade items.");
            else if (std::string const reason = UpgradeItem(player, request->bag, request->slot); !reason.empty())
                handler.SendNotification(reason);
        }
        return false;
    }
};

void AddSC_mythic_addon()
{
    new MythicAddonPlayerScript();
}
