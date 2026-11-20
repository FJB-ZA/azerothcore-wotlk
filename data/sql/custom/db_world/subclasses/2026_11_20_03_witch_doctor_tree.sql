-- Hexcraft, the Witch Doctor path's own talent tree (path 4, talent tab 993), beyond its spell data (solo-wow
-- patches/client/spell-data/subclasses/witch-doctor-tree.json; design in solo-wow's
-- docs/superpowers/specs/2026-11-20-subclasses-witch-doctor-tree-design.md). Spells 994000-994999 are the tree's, and
-- creatures 990810-990813 sit in the path's own 990700-990899 block. The talents that need more than spell data are in
-- src/server/scripts/Custom/Subclasses/WitchDoctor/witch_doctor_hexcraft.cpp.
--
-- Shade Puppet, Loa's Spite and Witchfire rank up with level: their higher ranks are spell_ranks chains the spell data
-- writes, and the core gives each rank to a slot on the path at its SpellLevel (subclass_paths.cpp), as Blight's
-- diseases. Loa's Ride's and Marionette Dance's bursts have ranks no talent teaches; the script picks the rank for the
-- shaman's level. The tree has no spell given with the path, so it has no subclass_spell_grant rows.

-- The tree opens on the path; it plays as a caster.
UPDATE `subclass_path` SET `talent_tab` = 993, `role` = 3 WHERE `id` = 4;

-- Spell power per hit, per tick for Shade Puppet: each the same share of Lightning Bolt's 0.714 as its damage is of a
-- bolt. The copies (Hex Doll's payout, the echo, Pins and Needles, Splintering Hex, Hexed Lightning's strikes and hex)
-- take no bonus: they repeat damage that had its bonuses already. The balance harness tunes the rest (solo-wow's
-- BALANCE.md).
DELETE FROM `spell_bonus_data` WHERE `entry` IN (994000, 994001, 994002, 994011, 994014, 994020, 994021, 994023, 994024, 994030, 994031);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(994000, 0, 0.107, 0, 0, 'Witch Doctor - Shade Puppet, per tick'),
(994001, 0.643, 0, 0, 0, 'Witch Doctor - Loa''s Spite'),
(994002, 0.536, 0, 0, 0, 'Witch Doctor - Witchfire'),
(994011, 0, 0, 0, 0, 'Witch Doctor - Hex Doll payout'),
(994014, 0, 0, 0, 0, 'Witch Doctor - Echo Ward''s echo'),
(994020, 0, 0, 0, 0, 'Witch Doctor - Pins and Needles'),
(994021, 0, 0, 0, 0, 'Witch Doctor - Splintering Hex'),
(994023, 0, 0, 0, 0, 'Witch Doctor - Hexed Lightning''s strikes'),
(994024, 0, 0, 0, 0, 'Witch Doctor - Hexed Lightning''s hex'),
(994030, 0.143, 0, 0, 0, 'Witch Doctor - Loa''s Ride burst'),
(994031, 0.357, 0, 0, 0, 'Witch Doctor - Marionette Dance burst');

-- Stacking with the 3.3.5 buffs and debuffs they match: Exposed Soul with Totem of Wrath and Heart of the Crusader
-- (crit chance taken), Shadra's Favor with Ferocious Inspiration, Sanctified Retribution and Arcane Empowerment
-- (damage done), and Loa's Spite's healing cut with Mortal Strike and its kind (healing taken). The strongest counts.
DELETE FROM `spell_group` WHERE `spell_id` IN (994016, 994019, 994141);
INSERT INTO `spell_group` (`id`, `spell_id`) VALUES
(1058, 994019),
(1056, 994141),
(1061, 994016);

-- Procs. The chances are the spells' own (ProcChance in the spell data) where Chance is 0.
--   Echo Ward (994003): Lightning Bolt (word 1 0x1) and Loa's Spite (word 3 0x20000, honoured only with Ward's Echo)
--     landing; Lightning Overload's copies are triggered, so they never echo.
--   Choking Jinx (994005): the holder's own spell casts, any kind; its one charge takes the jinx away.
--   Loa's Ride (994006): the shaman's periodic damage ticks.
--   Hexed Lightning's charge (994022): the next Lightning Bolt to land; one charge.
--   Hex Doll (994103): every shaman spell's damage, direct or periodic, but the copies (the echo 0x80000, the payout
--     0x100000, Hexed Lightning's hex 0x800000 and Pins and Needles 0x2000000); triggered spells too (Spirit Nova's
--     damage, the splash hits).
--   Spirit Catcher (-994106): melee, spell and periodic damage and healing.
--   Juju Surge (994115): Shade Puppet's ticks (word 3 0x10000).
--   Fevered Hex (-994125): Witchfire (word 3 0x40000) crits.
--   Exposed Soul (994135): Spirit Nova's damage (word 2 0x40000, a triggered cast) and Witchfire.
--   Hexed Lightning (994142): Witchfire landing.
--   Other Side (994144): Shade Puppet's ticks.
--   Wrathful Golem (994145): casting Mojo Golem (word 3 0x20000000).
DELETE FROM `spell_proc` WHERE `SpellId` IN (994003, 994005, 994006, 994022, 994103, -994106, 994115, -994125, 994135, 994142, 994144, 994145);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(994003, 0, 11, 0x1, 0, 0x20000, 0x10000, 0x1, 0x2, 0, 0, 0, 0, 100, 0, 0),
(994005, 0, 0, 0, 0, 0, 0x15400, 0x7, 0x1, 0, 0, 0, 0, 100, 0, 1),
(994006, 0, 0, 0, 0, 0, 0x40000, 0x1, 0x2, 0, 0, 0, 0, 100, 0, 0),
(994022, 0, 11, 0x1, 0, 0, 0x10000, 0x1, 0x2, 0, 0, 0, 0, 100, 0, 1),
(994103, 0, 11, 0xFFFFFFFF, 0xFFFFFFFF, 0xFD67FFFF, 0x50000, 0x1, 0x2, 0, 0x2, 0, 0, 100, 0, 0),
(-994106, 0, 0, 0, 0, 0, 0x54014, 0x3, 0x2, 0, 0, 0, 0, 0, 0, 0),
(994115, 0, 11, 0, 0, 0x10000, 0x40000, 0x1, 0x2, 0, 0, 0, 0, 0, 0, 0),
(-994125, 0, 11, 0, 0, 0x40000, 0x10000, 0x1, 0x2, 0x2, 0, 0, 0, 100, 0, 0),
(994135, 0, 11, 0, 0x40000, 0x40000, 0x10000, 0x1, 0x2, 0, 0x2, 0, 0, 100, 0, 0),
(994142, 0, 11, 0, 0, 0x40000, 0x10000, 0x1, 0x2, 0, 0, 0, 0, 100, 0, 0),
(994144, 0, 11, 0, 0, 0x10000, 0x40000, 0x1, 0x2, 0, 0, 0, 0, 100, 0, 0),
(994145, 0, 11, 0, 0, 0x20000000, 0x400, 0x7, 0x1, 0, 0, 0, 0, 100, 0, 0);

-- Scripts. Loa's Spite and Witchfire take every rank (the negative first rank). The wards' Venom Spit (-990750) and
-- pulse (-990780) feed their owner's Hex Doll, since a ward, not the shaman, casts them. Those two rows sit in the
-- path's 990700-990899 range, which 2026_11_16_01_witch_doctor_path.sql clears and refills: run this file again after
-- that one.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_hexcraft_hex_doll', 'spell_hexcraft_hex_doll_store',
  'spell_hexcraft_ward_hit', 'spell_hexcraft_juju_surge', 'spell_hexcraft_wandering_spirit',
  'spell_hexcraft_guiding_spirits', 'spell_hexcraft_echo_ward', 'spell_hexcraft_choking_jinx',
  'spell_hexcraft_loas_spite', 'spell_hexcraft_witchfire', 'spell_hexcraft_hexed_lightning', 'spell_hexcraft_other_side',
  'spell_hexcraft_loas_ride', 'spell_hexcraft_mojo_golem', 'spell_hexcraft_marionette_dance');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(994103, 'spell_hexcraft_hex_doll'),
(994010, 'spell_hexcraft_hex_doll_store'),
(-990750, 'spell_hexcraft_ward_hit'),
(-990780, 'spell_hexcraft_ward_hit'),
(994115, 'spell_hexcraft_juju_surge'),
(994012, 'spell_hexcraft_wandering_spirit'),
(994134, 'spell_hexcraft_guiding_spirits'),
(994003, 'spell_hexcraft_echo_ward'),
(994005, 'spell_hexcraft_choking_jinx'),
(-994001, 'spell_hexcraft_loas_spite'),
(-994002, 'spell_hexcraft_witchfire'),
(994022, 'spell_hexcraft_hexed_lightning'),
(994026, 'spell_hexcraft_other_side'),
(994006, 'spell_hexcraft_loas_ride'),
(994007, 'spell_hexcraft_mojo_golem'),
(994008, 'spell_hexcraft_marionette_dance');

-- The tree's creatures, each on witch_doctor_hexcraft.cpp's AI, which gives it its summoner's faction and level:
--   990810 Shade Puppet's shade: a Voodoo Spirit's row and body (15009), running fast, never attacked or selected.
--   990811 Echo Ward: Searing Totem's row (2523) in Ward of Zum'rah's body (7785), not a totem (no totem creature type,
--          and the spell's summon takes no slot), standing still and never attacked.
--   990812 Mojo Golem: Searing Totem's row in a Drakkari Golem's body (29832) at half size, standing still. It can be
--          attacked, since a spell its magnet (994029, cast by its AI) pulls has to land on it.
--   990813 a Marionette Dance copy: Mirror Image's row and body (31216); the AI casts Clone Me! from the shaman on it.
DELETE FROM `creature_template` WHERE `entry` BETWEEN 990810 AND 990813;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 990810 AND 990813;
DELETE FROM `creature_template_addon` WHERE `entry` BETWEEN 990810 AND 990813;
DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 990810 AND 990813;
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 990810 AND 990813;

DROP TEMPORARY TABLE IF EXISTS `witch_doctor_tree_creature`;
CREATE TEMPORARY TABLE `witch_doctor_tree_creature` AS SELECT * FROM `creature_template` WHERE `entry` IN (15009, 31216);
UPDATE `witch_doctor_tree_creature` SET `entry` = 990810, `name` = 'Shade Puppet', `subname` = NULL,
  `unit_flags` = 0x02000002, `speed_walk` = 1, `speed_run` = 2, `AIName` = '', `ScriptName` = 'npc_hexcraft_shade'
  WHERE `entry` = 15009;
UPDATE `witch_doctor_tree_creature` SET `entry` = 990813, `name` = 'Marionette', `subname` = NULL, `AIName` = '',
  `ScriptName` = 'npc_hexcraft_marionette' WHERE `entry` = 31216;
INSERT INTO `witch_doctor_tree_creature` SELECT * FROM `creature_template` WHERE `entry` = 2523;
UPDATE `witch_doctor_tree_creature` SET `entry` = 990811, `name` = 'Echo Ward', `subname` = NULL, `type` = 10,
  `unit_flags` = 0x00000002, `AIName` = '', `ScriptName` = 'npc_hexcraft_echo_ward' WHERE `entry` = 2523;
INSERT INTO `witch_doctor_tree_creature` SELECT * FROM `creature_template` WHERE `entry` = 2523;
UPDATE `witch_doctor_tree_creature` SET `entry` = 990812, `name` = 'Mojo Golem', `subname` = NULL, `type` = 9,
  `unit_flags` = 0, `AIName` = '', `ScriptName` = 'npc_hexcraft_mojo_golem' WHERE `entry` = 2523;
UPDATE `witch_doctor_tree_creature` SET `minlevel` = 1, `maxlevel` = 80;
INSERT INTO `creature_template` SELECT * FROM `witch_doctor_tree_creature`;
DROP TEMPORARY TABLE `witch_doctor_tree_creature`;

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
SELECT 990810, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability` FROM `creature_template_model` WHERE `CreatureID` = 15009;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
SELECT 990811, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability` FROM `creature_template_model` WHERE `CreatureID` = 7785;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
SELECT 990812, `Idx`, `CreatureDisplayID`, 0.5, `Probability` FROM `creature_template_model` WHERE `CreatureID` = 29832;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
SELECT 990813, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability` FROM `creature_template_model` WHERE `CreatureID` = 31216;

INSERT INTO `creature_template_addon` (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`)
VALUES (990810, 0, 0, 0, 0, 0, 0, ''), (990811, 0, 0, 0, 0, 0, 0, ''), (990812, 0, 0, 0, 0, 0, 0, ''),
(990813, 0, 0, 0, 0, 0, 0, '');
