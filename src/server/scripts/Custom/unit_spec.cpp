/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <string>
#include <string_view>

#include "Chat.h"
#include "DBCStores.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SubclassPaths.h"
#include "UnitSpecPolicy.h"
#include "WorldPacket.h"
#include "WorldSession.h"

// Unit spec: tells the SpecSlots addon which talent tree another player leans on and which sub-class path their
// active slot walks, so a unit tooltip can say "Marksmanship Ranger" for a bot. The protocol is in UnitSpecPolicy.h.
namespace
{
// Points the player's active talent group has spent in each of its class's trees, a path's own tree fourth
UnitSpec::TreePoints PointsByTree(Player* player)
{
    UnitSpec::TreePoints points{};
    uint8 const group = player->GetActiveSpec();
    for (auto const& [spellId, talent] : player->GetTalentMap())
    {
        if (talent->State == PLAYERSPELL_REMOVED || !talent->IsInSpec(group))
            continue;
        TalentSpellPos const* pos = GetTalentSpellPos(spellId);
        TalentEntry const* entry = pos ? sTalentStore.LookupEntry(pos->talent_id) : nullptr;
        TalentTabEntry const* tab = entry ? sTalentTabStore.LookupEntry(entry->TalentTab) : nullptr;
        if (tab && tab->tabpage < UnitSpec::Trees)
            points[tab->tabpage] += pos->rank + 1;
    }
    return points;
}

// What a tooltip shows anyway: a player near the asker, or one in their group wherever they are
bool CanAskAbout(Player* asker, Player* target)
{
    if (!target->IsInWorld())
        return false;
    if (target == asker || target->GetMapId() == asker->GetMapId())
        return true;
    return asker->GetGroup() && asker->GetGroup() == target->GetGroup();
}

void Answer(Player* asker, Player* target)
{
    SubclassPaths::Path const* path = SubclassPaths::ActivePath(target);
    std::string const line = UnitSpec::FormatSpec(target->GetGUID().GetCounter(), PointsByTree(target),
                                                  path ? path->id : 0, path ? std::string_view(path->name) : "");
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, asker, asker,
                                 std::string(UnitSpec::AddonPrefix) + '\t' + line);
    asker->SendDirectMessage(&packet);
}
}  // namespace

class unit_spec_player : public PlayerScript
{
public:
    unit_spec_player() : PlayerScript("unit_spec_player", {PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT}) {}

    // The addon asks with a whisper to the player themself: "SoloWowSpec\tW <guid> ...".
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        std::string const prefix = std::string(UnitSpec::AddonPrefix) + '\t';
        if (language != LANG_ADDON || type != CHAT_MSG_WHISPER || receiver != player || msg.rfind(prefix, 0) != 0)
            return true;
        for (uint32 const guid : UnitSpec::ParseQuery(std::string_view(msg).substr(prefix.size())))
            if (Player* target = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid)))
                if (CanAskAbout(player, target))
                    Answer(player, target);
        return false;
    }
};

void AddSC_unit_spec()
{
    new unit_spec_player();
}
