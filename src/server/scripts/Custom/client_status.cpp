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
#include "ClientStatusPolicy.h"
#include "Config.h"
#include "DataMap.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"

// mod-individual-xp keeps each player's rate in Player::CustomData under "IndividualXP", as this class. DataMap finds
// it by type, so the declaration must match the pinned module's exactly (src/individual_xp.cpp). When it doesn't, the
// lookup finds nothing and the configured default rate is sent instead.
class PlayerXpRate : public DataMap::Base
{
public:
    PlayerXpRate() {}
    PlayerXpRate(float XPRate) : XPRate(XPRate) {}
    float XPRate = 1.0f;
};

// Client status: see ClientStatusPolicy.h. Sent at login, when the client asks, after a progression quest is
// rewarded, and after ".xp" or ".ip" commands.
namespace
{
// mod-individual-xp's settings, read once per config load
float defaultXpRate = 1.0f;
float maxXpRate = 10.0f;

// Bot sessions have no client socket, and the core names them "headless".
bool IsBot(Player* player) { return player->GetSession()->GetRemoteAddress() == "headless"; }

uint8 ProgressionStep(Player* player)
{
    uint8 step = 0;
    for (uint8 n = 1; n <= ClientStatus::MaxProgressionStep; ++n)
        if (player->GetQuestRewardStatus(ClientStatus::ProgressionQuestBase + n))
            step = n;
    return step;
}

float XpRate(Player* player)
{
    PlayerXpRate const* data = player->CustomData.Get<PlayerXpRate>("IndividualXP");
    return data ? data->XPRate : defaultXpRate;
}

// Hidden addon whisper from the player to themself
void SendToClient(Player* player, std::string const& body)
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
                                 std::string(ClientStatus::AddonPrefix) + '\t' + body);
    player->SendDirectMessage(&data);
}

void SendStatus(Player* player)
{
    if (IsBot(player))
        return;
    SendToClient(player, ClientStatus::FormatProgression(ProgressionStep(player)));
    bool const paused = player->HasPlayerFlag(PLAYER_FLAGS_NO_XP_GAIN);
    SendToClient(player, ClientStatus::FormatXpRate(XpRate(player), maxXpRate, paused));
}

// Sends the status on the player's next update, once the quest reward or command that changed it has finished
void SendStatusSoon(Player* player)
{
    if (IsBot(player))
        return;
    ObjectGuid const guid = player->GetGUID();
    player->m_Events.AddEventAtOffset([guid]
    {
        if (Player* target = ObjectAccessor::FindPlayer(guid))
            SendStatus(target);
    }, 1ms);
}
}

class ClientStatusWorldScript : public WorldScript
{
public:
    ClientStatusWorldScript()
        : WorldScript("ClientStatusWorldScript", {WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_AFTER_CONFIG_LOAD})
    {
    }

    // Scripts load after the first config read, so startup reads it too.
    void OnStartup() override { Load(); }
    void OnAfterConfigLoad(bool /*reload*/) override { Load(); }

private:
    static void Load()
    {
        defaultXpRate = sConfigMgr->GetOption<float>("IndividualXp.DefaultXPRate", 1.0f, false);
        maxXpRate = sConfigMgr->GetOption<float>("IndividualXp.MaxXPRate", 10.0f, false);
    }
};

class ClientStatusPlayerScript : public PlayerScript
{
public:
    ClientStatusPlayerScript()
        : PlayerScript("ClientStatusPlayerScript",
                       {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT,
                        PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST})
    {
    }

    void OnPlayerLogin(Player* player) override { SendStatus(player); }

    // The client asks again with a hidden whisper to the player themself: "SoloWow\tQ".
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        std::string const prefix = std::string(ClientStatus::AddonPrefix) + '\t';
        if (language != LANG_ADDON || type != CHAT_MSG_WHISPER || receiver != player || msg.rfind(prefix, 0) != 0)
            return true;

        if (ClientStatus::IsQuery(std::string_view(msg).substr(prefix.size())))
            SendStatus(player);
        return false;
    }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        if (ClientStatus::IsProgressionQuest(quest->GetQuestId()))
            SendStatusSoon(player);
    }
};

class ClientStatusCommandScript : public AllCommandScript
{
public:
    ClientStatusCommandScript()
        : AllCommandScript("ClientStatusCommandScript", {ALLCOMMANDHOOK_ON_TRY_EXECUTE_COMMAND})
    {
    }

    // Runs before the command, so the status goes out on the next update. ".ip set" changes the selected player.
    bool OnTryExecuteCommand(ChatHandler& handler, std::string_view cmdStr) override
    {
        if (handler.IsConsole() || !ClientStatus::ChangesStatus(cmdStr))
            return true;

        Player* player = handler.GetSession()->GetPlayer();
        if (!player)
            return true;
        SendStatusSoon(player);
        Player* target = handler.getSelectedPlayer();
        if (target && target != player && target->GetMap() == player->GetMap())
            SendStatusSoon(target);
        return true;
    }
};

void AddSC_client_status()
{
    new ClientStatusWorldScript();
    new ClientStatusPlayerScript();
    new ClientStatusCommandScript();
}
