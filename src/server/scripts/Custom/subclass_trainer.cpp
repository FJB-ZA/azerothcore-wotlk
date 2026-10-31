/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include "NPCPackets.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SubclassPaths.h"
#include "Trainer.h"

// Sub-class paths at the class trainer: a slot on a path sees and buys its replacement spells where the trainer lists
// the originals. The trainer still teaches the original, which subclass_paths.cpp moves to the replacement on learning.
namespace
{
// Does the active slot hold the spell, or its path's replacement for it?
bool Holds(Player const* player, uint32 spellId)
{
    if (player->HasSpell(spellId))
        return true;
    uint32 const replacement = SubclassPaths::ActiveReplacement(player, spellId);
    return replacement && player->HasSpell(replacement);
}

// Trainer::GetDefaultSpellState, counting a held replacement as its original.
Trainer::SpellState PathSpellState(Player const* player, Trainer::Spell const& trainerSpell)
{
    if (Holds(player, trainerSpell.SpellId))
        return Trainer::SpellState::Known;
    if (!player->IsSpellFitByClassAndRace(trainerSpell.SpellId))
        return Trainer::SpellState::Unavailable;
    if (trainerSpell.ReqSkillLine && player->GetBaseSkillValue(trainerSpell.ReqSkillLine) < trainerSpell.ReqSkillRank)
        return Trainer::SpellState::Unavailable;
    for (uint32 reqAbility : trainerSpell.ReqAbility)
        if (reqAbility && !Holds(player, reqAbility))
            return Trainer::SpellState::Unavailable;
    if (player->GetLevel() < trainerSpell.ReqLevel)
        return Trainer::SpellState::Unavailable;

    bool hasLearnSpellEffect = false;
    bool knowsAllLearnedSpells = true;
    for (SpellEffectInfo const& effect : sSpellMgr->AssertSpellInfo(trainerSpell.SpellId)->GetEffects())
    {
        if (!effect.IsEffect(SPELL_EFFECT_LEARN_SPELL))
            continue;
        hasLearnSpellEffect = true;
        if (!Holds(player, effect.TriggerSpell))
            knowsAllLearnedSpells = false;
        if (uint32 const previousRank = sSpellMgr->GetPrevSpellInChain(effect.TriggerSpell))
            if (!Holds(player, previousRank))
                return Trainer::SpellState::Unavailable;
    }
    if (!hasLearnSpellEffect)
    {
        if (uint32 const previousRank = sSpellMgr->GetPrevSpellInChain(trainerSpell.SpellId))
            if (!Holds(player, previousRank))
                return Trainer::SpellState::Unavailable;
    }
    else if (knowsAllLearnedSpells)
        return Trainer::SpellState::Known;

    for (auto const& requirePair : sSpellMgr->GetSpellsRequiredForSpellBounds(trainerSpell.SpellId))
        if (!Holds(player, requirePair.second))
            return Trainer::SpellState::Unavailable;
    return Trainer::SpellState::Available;
}

uint32 Shown(Player const* player, uint32 spellId)
{
    uint32 const replacement = SubclassPaths::ActiveReplacement(player, spellId);
    return replacement ? replacement : spellId;
}
}  // namespace

class SubclassTrainerPlayerScript : public PlayerScript
{
public:
    SubclassTrainerPlayerScript()
        : PlayerScript("SubclassTrainerPlayerScript",
                       {PLAYERHOOK_ON_GET_TRAINER_SPELL_STATE, PLAYERHOOK_ON_BEFORE_RECEIVE_SPELL_LIST_FROM_TRAINER,
                        PLAYERHOOK_ON_RESOLVE_TRAINER_SPELL})
    {
    }

    // The default state only knows originals, so a slot holding replacements would see ranks as unlearned or locked.
    void OnPlayerGetTrainerSpellState(Player const* player, uint32 trainerId, uint32 spellId, Trainer::SpellState& state) override
    {
        if (state == Trainer::SpellState::Known || !SubclassPaths::ActivePath(player))
            return;
        Trainer::Trainer const* trainer = sObjectMgr->GetTrainer(trainerId);
        if (Trainer::Spell const* trainerSpell = trainer ? trainer->GetSpell(spellId) : nullptr)
            state = PathSpellState(player, *trainerSpell);
    }

    void OnPlayerBeforeReceiveSpellListFromTrainer(Player* player, Creature* /*trainer*/, WorldPackets::NPC::TrainerList& trainerList) override
    {
        if (!SubclassPaths::ActivePath(player))
            return;
        for (WorldPackets::NPC::TrainerListSpell& spell : trainerList.Spells)
        {
            spell.SpellID = int32(Shown(player, uint32(spell.SpellID)));
            for (int32& reqAbility : spell.ReqAbility)
                if (reqAbility)
                    reqAbility = int32(Shown(player, uint32(reqAbility)));
        }
    }

    // A client buying a listed replacement asks for its id; the trainer only knows the original.
    void OnPlayerResolveTrainerSpell(Player const* player, uint32 trainerId, uint32& spellId) override
    {
        Trainer::Trainer const* trainer = sObjectMgr->GetTrainer(trainerId);
        if (!trainer || trainer->GetSpell(spellId))
            return;
        if (uint32 const original = SubclassPaths::ActiveOriginal(player, spellId))
            spellId = original;
    }
};

void AddSC_subclass_trainer()
{
    new SubclassTrainerPlayerScript();
}
