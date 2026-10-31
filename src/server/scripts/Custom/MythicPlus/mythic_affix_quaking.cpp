/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Map.h"
#include "MythicMechanicPolicy.h"
#include "MythicPlus.h"
#include "Player.h"
#include "ScriptMgr.h"

// Quaking: every 20 seconds in combat the ground quakes under each player, hurting and interrupting everyone still
// within 5 yards three seconds later, so the group spreads out and casters time their casts.
using namespace MythicPlus;

namespace
{
struct QuakingTimer : public DataMap::Base
{
    uint32 ms = 0;
};
}

class MythicQuakingMapScript : public AllMapScript
{
public:
    MythicQuakingMapScript() : AllMapScript("MythicQuakingMapScript", {ALLMAPHOOK_ON_MAP_UPDATE}) { }

    void OnMapUpdate(Map* map, uint32 diff) override
    {
        MythicRun* run = ActiveRun(map);
        if (!run || !run->Has(Affix::Quaking))
            return;
        QuakingTimer* timer = map->CustomData.GetDefault<QuakingTimer>("MythicPlusQuaking");
        timer->ms += diff;
        if (timer->ms < QuakingIntervalMs)
            return;
        timer->ms = 0;
        uint32 const damage = ScaledDamage(QuakeDamage, run->level);
        for (Player* player : RunPlayers(map, true))
            CastGround(player, player->GetPosition(), SpellQuake, damage, CircleMs);
    }
};

void AddSC_mythic_affix_quaking()
{
    new MythicQuakingMapScript();
}
