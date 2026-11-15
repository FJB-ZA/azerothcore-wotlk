-- Ranger trainers, unlock quests and lore book (path 3). Re-applicable.
--
--   990570 Farstrider Lanaria: Ironforge's Hall of Arms, between the hunter trainers (Alliance)
--   990571 Dark Ranger Velara: Orgrimmar's Valley of Honor, by the hunter trainers (Horde)
-- Both are neutral, so either faction can use either one. Move a spawn in game with .npc move if it stands badly.
-- They use the core's npc_subclass_path gossip: a hunter who has done the quests sets the active spec slot's path
-- there, for the path change fee.
--
--   990570 Keen Edges      bring one Rough Sharpening Stone
--   990571 The Lone Trail  kill 8 beasts worth experience with no pet out (credited as creature 990575 by
--                          Custom/Subclasses/Ranger/ranger_quests.cpp); rewards A Ranger's Oath (item 990570) and
--                          puts the active spec slot on the path for free. subclass_path.unlock_quest names it.

DELETE FROM `npc_text` WHERE `ID` BETWEEN 990570 AND 990573;
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(990570, CONCAT('A beast at your heel is a fine thing, $c. It''s also one more mouth to feed, and one more set of ',
  'tracks for your quarry to find.$B$BIn Quel''Thalas we learned to walk alone. Show me you can do the same, and I''ll ',
  'teach each of your spec slots to hunt without a companion. You can always go back to the beasts later. It costs a ',
  'little more each time, so think before you choose.'), '', 1),
(990571, 'You walk too loudly for a ranger, $c. Go and find a teacher of your own kind.', '', 1),
(990572, CONCAT('The living hunt with a beast because they fear the dark alone. My rangers stopped fearing anything a ',
  'long time ago.$B$BIf you want to hunt as we do, prove you can manage without a pet. Then each of your spec slots ',
  'can walk the path, and you can go back to your beasts whenever you like. Each time you go back costs a little ',
  'more.'), '', 1),
(990573, 'You are no hunter, $c. Leave me.', '', 1);

-- Hunters get the teaching text, everyone else the refusal; each trainer has her own voice
DELETE FROM `gossip_menu` WHERE `MenuID` IN (990570, 990571);
INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES (990570, 990570), (990570, 990571), (990571, 990572), (990571, 990573);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 14 AND `SourceGroup` IN (990570, 990571);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `ConditionTypeOrReference`,
  `ConditionValue1`, `NegativeCondition`, `Comment`) VALUES
(14, 990570, 990570, 15, 4, 0, 'Ranger trainer Lanaria: teaching text for hunters'),
(14, 990570, 990571, 15, 4, 1, 'Ranger trainer Lanaria: refusal for other classes'),
(14, 990571, 990572, 15, 4, 0, 'Ranger trainer Velara: teaching text for hunters'),
(14, 990571, 990573, 15, 4, 1, 'Ranger trainer Velara: refusal for other classes');

DELETE FROM `creature_template` WHERE `entry` IN (990570, 990571);
DROP TEMPORARY TABLE IF EXISTS `ranger_trainer`;
CREATE TEMPORARY TABLE `ranger_trainer` AS SELECT * FROM `creature_template` WHERE `entry` = 5117;  -- Regnus Thundergranite
UPDATE `ranger_trainer` SET `entry` = 990570, `name` = 'Farstrider Lanaria', `subname` = 'Ranger',
  `gossip_menu_id` = 990570;
INSERT INTO `ranger_trainer` SELECT * FROM `creature_template` WHERE `entry` = 5117;
UPDATE `ranger_trainer` SET `entry` = 990571, `name` = 'Dark Ranger Velara', `subname` = 'Ranger',
  `gossip_menu_id` = 990571 WHERE `entry` = 5117;
UPDATE `ranger_trainer` SET `faction` = 35, `npcflag` = 3, `ScriptName` = 'npc_subclass_path';
INSERT INTO `creature_template` SELECT * FROM `ranger_trainer`;
DROP TEMPORARY TABLE `ranger_trainer`;

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (990570, 990571);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(990570, 0, 16069, 1, 1),  -- Ranger Valanna's model
(990571, 0, 21746, 1, 1);  -- a Dark Ranger's model

DELETE FROM `creature` WHERE `guid` IN (9990570, 9990571);
DROP TEMPORARY TABLE IF EXISTS `ranger_spawn`;
CREATE TEMPORARY TABLE `ranger_spawn` AS SELECT * FROM `creature` WHERE `guid` = 82;  -- Regnus Thundergranite
UPDATE `ranger_spawn` SET `guid` = 9990570, `id` = 990570, `position_x` = -5008.5, `position_y` = -1270.0,
  `position_z` = 507.84, `orientation` = 3.79;
INSERT INTO `ranger_spawn` SELECT * FROM `creature` WHERE `guid` = 7449;  -- Ormak Grimshot
UPDATE `ranger_spawn` SET `guid` = 9990571, `id` = 990571, `position_x` = 2104.0, `position_y` = -4620.4,
  `position_z` = 59.01, `orientation` = 4.52 WHERE `guid` = 7449;
UPDATE `ranger_spawn` SET `equipment_id` = 0, `wander_distance` = 0, `MovementType` = 0;
INSERT INTO `creature` SELECT * FROM `ranger_spawn`;
DROP TEMPORARY TABLE `ranger_spawn`;

DELETE FROM `creature_template` WHERE `entry` = 990575;
DROP TEMPORARY TABLE IF EXISTS `ranger_credit`;
CREATE TEMPORARY TABLE `ranger_credit` AS SELECT * FROM `creature_template` WHERE `entry` = 1;
UPDATE `ranger_credit` SET `entry` = 990575, `name` = 'Beast hunted alone', `subname` = '', `ScriptName` = '';
INSERT INTO `creature_template` SELECT * FROM `ranger_credit`;
DROP TEMPORARY TABLE `ranger_credit`;

DELETE FROM `quest_template` WHERE `ID` IN (990570, 990571);
DROP TEMPORARY TABLE IF EXISTS `ranger_quest`;
CREATE TEMPORARY TABLE `ranger_quest` AS SELECT * FROM `quest_template` WHERE `ID` = 1473;  -- Creature of the Void
INSERT INTO `ranger_quest` SELECT * FROM `quest_template` WHERE `ID` = 1473;
UPDATE `ranger_quest` SET `ID` = 990570 LIMIT 1;
UPDATE `ranger_quest` SET `ID` = 990571 WHERE `ID` = 1473;
UPDATE `ranger_quest` SET `QuestType` = 2, `QuestLevel` = -1, `MinLevel` = 10, `QuestSortID` = -261, `Flags` = 0,
  `AllowableRaces` = 0, `RewardXPDifficulty` = 4, `RewardMoney` = 0, `RewardMoneyDifficulty` = 0,
  `RewardNextQuest` = 0, `StartItem` = 0, `RewardItem1` = 0, `RewardAmount1` = 0,
  `RequiredItemId1` = 0, `RequiredItemCount1` = 0, `RequiredNpcOrGo1` = 0, `RequiredNpcOrGoCount1` = 0,
  `ItemDrop1` = 0, `ItemDropQuantity1` = 0, `ObjectiveText1` = '', `AreaDescription` = '', `QuestCompletionLog` = '';

UPDATE `ranger_quest` SET
  `LogTitle` = 'Keen Edges',
  `RewardNextQuest` = 990571,
  `RequiredItemId1` = 2862,
  `RequiredItemCount1` = 1,
  `LogDescription` = CONCAT('Bring a Rough Sharpening Stone to Farstrider Lanaria in Ironforge or Dark Ranger Velara ',
    'in Orgrimmar.'),
  `QuestDescription` = CONCAT(
    'So you''d hunt without a beast. Most hunters who say that mean it until the first wolf gets behind them.$B$B',
    'A pet takes the blows for you and finds the prey before you do. Without one, all you have is your eyes and your ',
    'blade, so your blade had better be sharp. Bring me a rough sharpening stone, $N. It''s a small thing, but every ',
    'ranger starts with it.')
WHERE `ID` = 990570;

UPDATE `ranger_quest` SET
  `LogTitle` = 'The Lone Trail',
  `RequiredNpcOrGo1` = 990575,
  `RequiredNpcOrGoCount1` = 8,
  `ObjectiveText1` = 'Beast hunted alone',
  `RewardItem1` = 990570,
  `RewardAmount1` = 1,
  `LogDescription` = CONCAT('Hunt 8 beasts that are worth your while with no pet at your side, then return to ',
    'Farstrider Lanaria in Ironforge or Dark Ranger Velara in Orgrimmar.'),
  `QuestDescription` = CONCAT(
    'Now the hard part. Send your companion away. Not to the stable down the road, $N. I mean gone from your side. ',
    'Then go out and hunt.$B$BEight beasts, and none so weak they aren''t worth your time. Nothing will take a blow for ',
    'you out there, and nothing will warn you when something comes up behind. Learn to hear it yourself.$B$B',
    'Come back when you''ve done it alone.')
WHERE `ID` = 990571;

INSERT INTO `quest_template` SELECT * FROM `ranger_quest`;
DROP TEMPORARY TABLE `ranger_quest`;

DELETE FROM `quest_template_addon` WHERE `ID` IN (990570, 990571);
INSERT INTO `quest_template_addon` (`ID`, `AllowableClasses`, `PrevQuestID`) VALUES
(990570, 4, 0),
(990571, 4, 990570);

DELETE FROM `quest_details` WHERE `ID` IN (990570, 990571);
INSERT INTO `quest_details` (`ID`, `Emote1`) VALUES (990570, 1), (990571, 1);

DELETE FROM `quest_request_items` WHERE `ID` IN (990570, 990571);
INSERT INTO `quest_request_items` (`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `CompletionText`) VALUES
(990570, 1, 1, 'Well? A dull blade is worse than none, $N.'),
(990571, 1, 1, 'Did you do it alone? Your pet can''t lie for you, $N, and neither can you.');

DELETE FROM `quest_offer_reward` WHERE `ID` IN (990570, 990571);
INSERT INTO `quest_offer_reward` (`ID`, `Emote1`, `RewardText`) VALUES
(990570, 1, CONCAT('Good. Keep it close and use it every night, whether you''ve fought or not. A ranger who waits ',
  'until the blade is dull has already waited too long.')),
(990571, 1, CONCAT('You came back, and you came back alone. That''s most of it.$B$BTake the oath. Every ranger ',
  'copies it out by hand once, and now you have yours. From today your shots carry what your beast used to, and the ',
  'mending you learned is for your own wounds. If you want your companion back for one of your spec slots, come and ',
  'see me. It''ll cost you, but I won''t stand in your way.'));

DELETE FROM `creature_queststarter` WHERE `quest` IN (990570, 990571);
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
(990570, 990570), (990571, 990570), (990570, 990571), (990571, 990571);
DELETE FROM `creature_questender` WHERE `quest` IN (990570, 990571);
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
(990570, 990570), (990571, 990570), (990570, 990571), (990571, 990571);

-- A Ranger's Oath (item 990570) is in solo-wow's spell data (patches/client/spell-data/subclasses/ranger.json): its
-- generated SQL owns every item in 990000-990999. Its pages stay here.

DELETE FROM `page_text` WHERE `ID` BETWEEN 990570 AND 990572;
INSERT INTO `page_text` (`ID`, `Text`, `NextPageID`) VALUES
(990570, CONCAT(
  'The Farstriders of Quel''Thalas guarded the borders of the elven kingdom for thousands of years. Most of them kept ',
  'hawks or lynxes, but the oldest kept nothing at all. A ranger alone leaves half the tracks, they said, and makes ',
  'half the noise.\n\nWhen the Scourge came through Quel''Thalas, the rangers who had learned to fight alone were ',
  'among the few still standing at the end.'), 990571),
(990571, CONCAT(
  'Sylvanas Windrunner was Ranger-General then. She died defending Quel''Thalas and came back as something else, and ',
  'in time she raised rangers of her own from the fallen. The dark rangers of the Undercity still hunt the way she ',
  'taught them, without a beast and without warning, and they waste nothing.\n\nThe Farstriders who lived went on ',
  'fighting for Quel''Thalas, and a few of them now teach hunters of other races in the cities of the Alliance.'), 990572),
(990572, CONCAT(
  'The oath is short.\n\nI hunt alone. My eyes are my hound, my blade is my claw, and my wounds are mine to bind.\n\n',
  'I waste no arrow and no breath. I leave no trail I did not mean to leave.\n\nI owe my quarry a clean death, and ',
  'I owe nothing else to anyone.'), 0);
