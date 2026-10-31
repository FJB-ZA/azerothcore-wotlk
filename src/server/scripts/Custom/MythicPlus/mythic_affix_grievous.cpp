/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Map.h"
#include "MythicPlus.h"
#include "Player.h"
#include "ScriptMgr.h"

// Grievous: a player in combat below 90% health gains a stack of Grievous Wound every 3 seconds, each taking 2% of
// maximum health per tick, until healed above 90%.
using namespace MythicPlus;

namespace
{
struct GrievousTimer : public DataMap::Base
{
    uint32 ms = 0;
};
}

class MythicGrievousMapScript : public AllMapScript
{
public:
    MythicGrievousMapScript() : AllMapScript("MythicGrievousMapScript", {ALLMAPHOOK_ON_MAP_UPDATE}) { }

    void OnMapUpdate(Map* map, uint32 diff) override
    {
        MythicRun* run = ActiveRun(map);
        if (!run || !run->Has(Affix::Grievous))
            return;
        GrievousTimer* timer = map->CustomData.GetDefault<GrievousTimer>("MythicPlusGrievous");
        timer->ms += diff;
        if (timer->ms < GrievousTickMs)
            return;
        timer->ms = 0;
        for (Player* player : RunPlayers(map, false))
        {
            if (player->HealthAbovePct(GrievousHealthPercent))
                player->RemoveAurasDueToSpell(SpellGrievousWound);
            else if (player->IsInCombat())
                player->AddAura(SpellGrievousWound, player);
        }
    }
};

void AddSC_mythic_affix_grievous()
{
    new MythicGrievousMapScript();
}
