/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// This is where scripts' loading functions should be declared:
// void MyExampleScript()
void AddSC_mentor_mode();
void AddSC_spec_slots();
void AddSC_boost();
void AddSC_client_status();
void AddSC_spell_charges();
void AddSC_subclass_paths();
void AddSC_subclass_trainer();
void AddSC_subclass_combat();
void AddSC_subclass_minions();
void AddSC_subclass_npc();
void AddSC_gladiator_spells();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddCustomScripts()
{
    // MyExampleScript()
    AddSC_mentor_mode();
    AddSC_spec_slots();
    AddSC_boost();
    AddSC_client_status();
    AddSC_spell_charges();
    AddSC_subclass_paths();
    AddSC_subclass_trainer();
    AddSC_subclass_combat();
    AddSC_subclass_minions();
    AddSC_subclass_npc();
    AddSC_gladiator_spells();
}
