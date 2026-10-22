-- solo-wow sub-class path trainers (src/server/scripts/Custom/subclass.cpp, npc_subclass_path_trainer). Re-applicable.
-- A path trainer puts the active spec slot on its path, or back on the plain class, for the path-change fee.
--
--   990170 Kelvar Grimm, Necromancer: Undercity, by the warlock trainers in the Magic Quarter (Horde)
--   990171 Aldric Blackmoor, Outcast Necromancer: Raven Hill in Duskwood, by Abercrombie's shack (Alliance)
-- Both are neutral, so either faction can use either one. Move a spawn in game with .npc move if it stands badly.

CREATE TABLE IF NOT EXISTS `subclass_path_trainer` (
  `creature_entry` INT UNSIGNED NOT NULL,
  `path_id` TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`creature_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELETE FROM `subclass_path_trainer` WHERE `creature_entry` IN (990170, 990171);
INSERT INTO `subclass_path_trainer` (`creature_entry`, `path_id`) VALUES (990170, 1), (990171, 1);

DELETE FROM `npc_text` WHERE `ID` BETWEEN 990170 AND 990172;
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(990170, CONCAT('The living fear what lies beneath their feet. I stopped fearing it a long time ago.$B$B',
  'I can teach you to call bone, frost and plague where you once called fel fire. Each of your spec slots keeps its ',
  'own path, and you can always walk back to the old ways. That costs a little more each time, so choose with care.'),
  '', 1),
(990171, 'You have no gift for this, $c. The dead will not answer you.', '', 1),
(990172, 'Not yet, $N. The dead do not answer the untrained. Come back when you have reached level 10.', '', 1);

DELETE FROM `creature_template` WHERE `entry` IN (990170, 990171);
DROP TEMPORARY TABLE IF EXISTS `solo_wow_trainer`;
CREATE TEMPORARY TABLE `solo_wow_trainer` AS SELECT * FROM `creature_template` WHERE `entry` = 4563;
UPDATE `solo_wow_trainer` SET `entry` = 990170, `name` = 'Kelvar Grimm', `subname` = 'Necromancer';
INSERT INTO `solo_wow_trainer` SELECT * FROM `creature_template` WHERE `entry` = 4563;
UPDATE `solo_wow_trainer` SET `entry` = 990171, `name` = 'Aldric Blackmoor', `subname` = 'Outcast Necromancer'
WHERE `entry` = 4563;
UPDATE `solo_wow_trainer` SET `faction` = 35, `npcflag` = 1, `gossip_menu_id` = 0,
  `ScriptName` = 'npc_subclass_path_trainer';
INSERT INTO `creature_template` SELECT * FROM `solo_wow_trainer`;
DROP TEMPORARY TABLE `solo_wow_trainer`;

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (990170, 990171);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(990170, 0, 2582, 1, 1),   -- Scourge Necromancer's model
(990171, 0, 10427, 1, 1);  -- a Cult of the Damned necromancer's model

DELETE FROM `creature` WHERE `guid` IN (9990170, 9990171);
DROP TEMPORARY TABLE IF EXISTS `solo_wow_spawn`;
CREATE TEMPORARY TABLE `solo_wow_spawn` AS SELECT * FROM `creature` WHERE `guid` = 41834;  -- Kaal Soulreaper
UPDATE `solo_wow_spawn` SET `guid` = 9990170, `id1` = 990170, `position_x` = 1783.5, `position_y` = 47.5,
  `position_z` = -61.41, `orientation` = 2.2;
INSERT INTO `solo_wow_spawn` SELECT * FROM `creature` WHERE `guid` = 4199;  -- Abercrombie
UPDATE `solo_wow_spawn` SET `guid` = 9990171, `id1` = 990171, `position_x` = -10279.5, `position_y` = 77.5,
  `position_z` = 38.93, `orientation` = 3.9 WHERE `guid` = 4199;
UPDATE `solo_wow_spawn` SET `id2` = 0, `id3` = 0, `equipment_id` = 0, `wander_distance` = 0, `MovementType` = 0;
INSERT INTO `creature` SELECT * FROM `solo_wow_spawn`;
DROP TEMPORARY TABLE `solo_wow_spawn`;
