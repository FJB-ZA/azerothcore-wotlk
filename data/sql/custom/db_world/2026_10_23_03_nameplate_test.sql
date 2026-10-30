-- Nameplate test lineup for the solo-wow Nameplates addon: `.nameplates test` (src/server/scripts/Custom/
-- nameplate_test.cpp) summons these in front of a GM, one per thing a nameplate shows. Copies of Defias Thug (38)
-- that give no experience, loot or money, hit softly and have plenty of health; the script sets their level against
-- the GM's and runs their casts and auras. Entries 990900-990949.

DELETE FROM `creature_template` WHERE `entry` BETWEEN 990900 AND 990949;
DROP TEMPORARY TABLE IF EXISTS `nameplate_test`;
CREATE TEMPORARY TABLE `nameplate_test` AS SELECT * FROM `creature_template` WHERE `entry` = 38;
UPDATE `nameplate_test` SET `subname` = 'Nameplate test', `IconName` = '', `gossip_menu_id` = 0, `minlevel` = 20,
  `maxlevel` = 20, `faction` = 14, `npcflag` = 0, `rank` = 0, `unit_class` = 1, `unit_flags` = 0, `unit_flags2` = 0,
  `dynamicflags` = 0, `type_flags` = 0, `lootid` = 0, `pickpocketloot` = 0, `skinloot` = 0, `mingold` = 0,
  `maxgold` = 0, `AIName` = '', `MovementType` = 0, `HealthModifier` = 20, `ManaModifier` = 10,
  `DamageModifier` = 0.2, `ExperienceModifier` = 0, `RegenHealth` = 1, `CreatureImmunitiesId` = 0,
  `flags_extra` = 0, `KillCredit1` = 0, `KillCredit2` = 0, `difficulty_entry_1` = 0, `difficulty_entry_2` = 0,
  `difficulty_entry_3` = 0, `ScriptName` = 'npc_nameplate_test';

-- Level colours: the script sets these 10 below, 4 below, level with, 3 above and 7 above the GM
UPDATE `nameplate_test` SET `entry` = 990900, `name` = 'Trivial Kobold';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990901, `name` = 'Easy Gnoll';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990902, `name` = 'Even Bandit';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990903, `name` = 'Tough Ogre';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990904, `name` = 'Deadly Ogre';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;

-- Classifications: elite, rare, rare elite and boss
UPDATE `nameplate_test` SET `entry` = 990905, `name` = 'Elite Brute', `rank` = 1;
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990906, `name` = 'Rare Prowler', `rank` = 4;
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990907, `name` = 'Rare Elite Warlord', `rank` = 2;
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990908, `name` = 'Boss Overlord', `rank` = 3, `type_flags` = 4;
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;

-- Casters (mage class, for mana): a cast, a channel, and one that casts at whoever attacks it
UPDATE `nameplate_test` SET `rank` = 0, `type_flags` = 0, `unit_class` = 8;
UPDATE `nameplate_test` SET `entry` = 990910, `name` = 'Healing Acolyte';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990911, `name` = 'Channeling Adept';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990912, `name` = 'Frost Duelist';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;

-- Auras and health
UPDATE `nameplate_test` SET `unit_class` = 1;
UPDATE `nameplate_test` SET `entry` = 990915, `name` = 'Afflicted Thug';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990916, `name` = 'Warded Champion';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990917, `name` = 'Wounded Straggler', `RegenHealth` = 0;
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990918, `name` = 'Dying Straggler';
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;

-- Reactions and a raid marker
UPDATE `nameplate_test` SET `RegenHealth` = 1;
UPDATE `nameplate_test` SET `entry` = 990920, `name` = 'Neutral Wanderer', `faction` = 7;
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990921, `name` = 'Friendly Guard', `faction` = 35;
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
UPDATE `nameplate_test` SET `entry` = 990922, `name` = 'Marked Target', `faction` = 14;
INSERT INTO `creature_template` SELECT * FROM `nameplate_test`;
DROP TEMPORARY TABLE `nameplate_test`;

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 990900 AND 990949;
DROP TEMPORARY TABLE IF EXISTS `nameplate_test_model`;
CREATE TEMPORARY TABLE `nameplate_test_model` AS SELECT * FROM `creature_template_model` WHERE `CreatureID` = 38;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
  `VerifiedBuild`)
SELECT t.`entry`, m.`Idx`, m.`CreatureDisplayID`, m.`DisplayScale`, m.`Probability`, m.`VerifiedBuild`
FROM `creature_template` t CROSS JOIN `nameplate_test_model` m WHERE t.`entry` BETWEEN 990900 AND 990949;
DROP TEMPORARY TABLE `nameplate_test_model`;

DELETE FROM `command` WHERE `name` LIKE 'nameplates%';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('nameplates test', 2, CONCAT('Syntax: .nameplates test\r\n\r\nSummons the nameplate test lineup in front of you ',
  'for 30 minutes: level colours, elite, rare, rare elite and boss, a cast, a channel, a caster that fights back, ',
  'every debuff type with stacks, buffs, low health, neutral and friendly units and, in a group, a skull marker.')),
('nameplates clear', 2, 'Syntax: .nameplates clear\r\n\r\nRemoves the nameplate test lineup around you.'),
('nameplates interrupt', 2, CONCAT('Syntax: .nameplates interrupt\r\n\r\nInterrupts every test caster around ',
  'you, to see an interrupted cast bar without an interrupt of your own.'));
