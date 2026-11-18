-- Blight, the Necromancer path's own talent tree (path 2, talent tab 991), beyond its spell data (solo-wow
-- patches/client/spell-data/subclasses/necromancer-tree.json; design in solo-wow's
-- docs/superpowers/specs/2026-11-17-subclasses-necromancer-tree-design.md). Spells 992000-992999 and creatures
-- 990380-990381 are the tree's. The talents that need more than spell data are in
-- src/server/scripts/Custom/Subclasses/Necromancer/necromancer_blight.cpp.

-- The tree opens on the path; it plays as a caster.
UPDATE `subclass_path` SET `talent_tab` = 991, `role` = 3 WHERE `id` = 2;

-- Plague Lore: Grave Worms and Wasting Plague can crit, as long as a slot follows the path
DELETE FROM `subclass_spell_grant` WHERE `path_id` = 2;
INSERT INTO `subclass_spell_grant` (`path_id`, `spell`) VALUES (2, 992090);

-- Spell power per tick for the diseases, per hit for the rest. Set against Corruption and Immolate at the same level;
-- the balance harness tunes them (solo-wow's BALANCE.md). Rot Echo's echo takes no bonus (it repeats damage that had
-- its bonuses already).
DELETE FROM `spell_bonus_data` WHERE `entry` IN (992000, 992001, 992002, 992006, 992009, 992011, 992016, 992019, 992021);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(992000, 0, 0.11, 0, 0, 'Necromancer - Grave Worms'),
(992001, 0, 0.09, 0, 0, 'Necromancer - Plague of the Risen'),
(992002, 0.5, 0, 0, 0, 'Necromancer - Plague of the Risen burst'),
(992006, 0, 0.08, 0, 0, 'Necromancer - Wasting Plague'),
(992009, 0.3, 0, 0, 0, 'Necromancer - Plague Bile (the gargoyle, its master''s spell power)'),
(992011, 0, 0.02, 0, 0, 'Necromancer - Crypt Plague, per stack'),
(992016, 0.15, 0, 0, 0, 'Necromancer - Abomination''s Brew'),
(992019, 0.2, 0, 0, 0, 'Necromancer - Rotling Burst (its master''s spell power)'),
(992021, 0, 0, 0, 0, 'Necromancer - Rot Echo');

-- Procs. Revenant's Touch: spell crits, direct or periodic. Parasites: damaging spells. Pestilent Haste: periodic
-- damage. The chances are the spells' own (ProcChance in the spell data).
DELETE FROM `spell_proc` WHERE `SpellId` IN (992131, -992132, -992134);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(992131, 0, 0, 0, 0, 0, 0x50000, 0x1, 0x2, 0x2, 0, 0, 0, 100, 0, 0),
(-992132, 0, 0, 0, 0, 0, 0x10000, 0x1, 0x2, 0, 0, 0, 0, 0, 0, 0),
(-992134, 0, 0, 0, 0, 0, 0x40000, 0x1, 0x2, 0, 0, 0, 0, 0, 0, 0);

-- Scripts. spell_necro_disease (Mutation) goes on every disease, Plague Strike's ranks included; Plague Strike keeps
-- the scripts it mirrors from Immolate (the _00 file).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_necro_contagion', 'spell_necro_contagion_carry',
  'spell_necro_disease', 'spell_necro_plague_of_the_risen', 'spell_necro_revenants_touch', 'spell_necro_parasites',
  'spell_necro_pestilent_haste');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(992003, 'spell_necro_contagion'),
(992005, 'spell_necro_contagion_carry'),
(-990300, 'spell_necro_disease'),
(992000, 'spell_necro_disease'),
(992001, 'spell_necro_disease'),
(992006, 'spell_necro_disease'),
(992011, 'spell_necro_disease'),
(992001, 'spell_necro_plague_of_the_risen'),
(992131, 'spell_necro_revenants_touch'),
(-992132, 'spell_necro_parasites'),
(-992134, 'spell_necro_pestilent_haste');

-- The gargoyle (Raise Gargoyle) and the rotlings (Muck Summoner). The gargoyle copies the death knight's Ebon
-- Gargoyle (27829); a rotling is a Rotting Slime's body (3928) at half size on the death knight ghoul's row (26125),
-- which is built to be summoned. Both run on necromancer_blight.cpp's AIs.
DELETE FROM `creature_template` WHERE `entry` IN (990380, 990381);
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (990380, 990381);
DELETE FROM `creature_template_addon` WHERE `entry` IN (990380, 990381);
DELETE FROM `creature_template_spell` WHERE `CreatureID` IN (990380, 990381);
DELETE FROM `creature_template_movement` WHERE `CreatureId` IN (990380, 990381);

DROP TEMPORARY TABLE IF EXISTS `necromancer_tree_creature`;
CREATE TEMPORARY TABLE `necromancer_tree_creature` AS SELECT * FROM `creature_template` WHERE `entry` IN (27829, 26125);
UPDATE `necromancer_tree_creature` SET `entry` = 990380, `name` = 'Plague Gargoyle', `ScriptName` = 'npc_necro_gargoyle'
  WHERE `entry` = 27829;
UPDATE `necromancer_tree_creature` SET `entry` = 990381, `name` = 'Rotling', `ScriptName` = 'npc_necro_rotling',
  `minlevel` = 1, `maxlevel` = 80 WHERE `entry` = 26125;
INSERT INTO `creature_template` SELECT * FROM `necromancer_tree_creature`;
DROP TEMPORARY TABLE `necromancer_tree_creature`;

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
SELECT 990380, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability` FROM `creature_template_model` WHERE `CreatureID` = 27829;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(990381, 0, 2028, 0.5, 1);

INSERT INTO `creature_template_addon` (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`)
VALUES (990380, 0, 0, 0, 0, 0, 0, ''), (990381, 0, 0, 0, 0, 0, 0, '');

INSERT INTO `creature_template_movement` (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
SELECT 990380, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`
FROM `creature_template_movement` WHERE `CreatureId` = 27829;
