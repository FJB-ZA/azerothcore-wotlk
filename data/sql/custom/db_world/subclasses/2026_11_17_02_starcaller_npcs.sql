-- Starcaller trainers, unlock quests and lore book (path 5). Re-applicable.
--
--   990650 Priestess Ilyssa Moonveil: Darnassus, the Temple of the Moon, beside Priestess A'moora (Alliance)
--   990651 Moonseer Tahala: Thunder Bluff's Elder Rise, among the druid trainers (Horde)
-- The Cenarion Circle teaches both peoples, so both are neutral and either faction can use either one. Move a spawn in
-- game with .npc move if it stands badly. They use the core's npc_subclass_path gossip: a druid who has done the
-- quests sets the active spec slot's path there, for the path change fee.
--
--   990650 Feathers for the Watch  bring 6 Light Feathers (Darkshore's moonkin and the plains' birds carry them)
--   990651 By Moonlight            kill 8 creatures worth experience while the druid's own Moonfire burns them
--                                  (credited as creature 990655 by Custom/Subclasses/Starcaller/starcaller_quests.cpp);
--                                  rewards The Moon's Watch (item 990650) and puts the active spec slot on the path
--                                  for free. subclass_path.unlock_quest names it.

DELETE FROM `npc_text` WHERE `ID` BETWEEN 990650 AND 990653;
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(990650, CONCAT('Elune''s light finds you, $c. You know the wild, its wrath and its storms. There is another way to ',
  'keep the balance, older than the Circle itself.$B$BThe Sisterhood has watched the night since the War of the ',
  'Ancients. Prove yourself and I will teach each of your spec slots to call the moon and the stars in place of the ',
  'storm. If you want the storm back, come to me again. It costs a little more each time.'), '', 1),
(990651, 'The moon shines on you too, $c, but she has nothing for you to learn here.', '', 1),
(990652, CONCAT('Be welcome, $c. You walk with the Earth Mother, but have you looked up at night? Mu''sha watches ',
  'us from the sky, the Earth Mother''s left eye, and her light is a weapon for those who learn to hold it.$B$BThe ',
  'night elves call it Elune. The name does not matter to the moon. Show me you are ready and I will teach each of ',
  'your spec slots to call her light. You may go back to the storm later, though it costs a little more each time.'), '', 1),
(990653, 'Mu''sha has nothing to teach you through me, $c. Walk with the Earth Mother.', '', 1);

-- Druids get the teaching text, everyone else the refusal; each trainer has their own voice
DELETE FROM `gossip_menu` WHERE `MenuID` IN (990650, 990651);
INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES (990650, 990650), (990650, 990651), (990651, 990652), (990651, 990653);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 14 AND `SourceGroup` IN (990650, 990651);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `ConditionTypeOrReference`,
  `ConditionValue1`, `NegativeCondition`, `Comment`) VALUES
(14, 990650, 990650, 15, 1024, 0, 'Starcaller trainer Ilyssa: teaching text for druids'),
(14, 990650, 990651, 15, 1024, 1, 'Starcaller trainer Ilyssa: refusal for other classes'),
(14, 990651, 990652, 15, 1024, 0, 'Starcaller trainer Tahala: teaching text for druids'),
(14, 990651, 990653, 15, 1024, 1, 'Starcaller trainer Tahala: refusal for other classes');

DELETE FROM `creature_template` WHERE `entry` IN (990650, 990651);
DROP TEMPORARY TABLE IF EXISTS `starcaller_trainer`;
CREATE TEMPORARY TABLE `starcaller_trainer` AS SELECT * FROM `creature_template` WHERE `entry` = 7313;  -- Priestess A'moora
UPDATE `starcaller_trainer` SET `entry` = 990650, `name` = 'Priestess Ilyssa Moonveil', `subname` = 'Starcaller',
  `gossip_menu_id` = 990650;
INSERT INTO `starcaller_trainer` SELECT * FROM `creature_template` WHERE `entry` = 3034;  -- Sheal Runetotem
UPDATE `starcaller_trainer` SET `entry` = 990651, `name` = 'Moonseer Tahala', `subname` = 'Starcaller',
  `gossip_menu_id` = 990651 WHERE `entry` = 3034;
UPDATE `starcaller_trainer` SET `faction` = 35, `npcflag` = 3, `ScriptName` = 'npc_subclass_path', `AIName` = '';
INSERT INTO `creature_template` SELECT * FROM `starcaller_trainer`;
DROP TEMPORARY TABLE `starcaller_trainer`;

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (990650, 990651);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(990650, 0, 15585, 1, 1),  -- a Priestess of the Moon's model (Moonglade's Lunar Festival)
(990651, 0, 2121, 1, 1);   -- Sheal Runetotem's model

DELETE FROM `creature` WHERE `guid` IN (9990650, 9990651);
DROP TEMPORARY TABLE IF EXISTS `starcaller_spawn`;
CREATE TEMPORARY TABLE `starcaller_spawn` AS SELECT * FROM `creature` WHERE `guid` = 46721;  -- Priestess A'moora
UPDATE `starcaller_spawn` SET `guid` = 9990650, `id` = 990650, `position_x` = 9634.9, `position_y` = 2547.4,
  `position_z` = 1354.89, `orientation` = 4.8;
INSERT INTO `starcaller_spawn` SELECT * FROM `creature` WHERE `guid` = 26662;  -- Sheal Runetotem
UPDATE `starcaller_spawn` SET `guid` = 9990651, `id` = 990651, `position_x` = -1047.0, `position_y` = -292.0,
  `position_z` = 159.11, `orientation` = 1.6 WHERE `guid` = 26662;
UPDATE `starcaller_spawn` SET `equipment_id` = 0, `wander_distance` = 0, `MovementType` = 0;
INSERT INTO `creature` SELECT * FROM `starcaller_spawn`;
DROP TEMPORARY TABLE `starcaller_spawn`;

DELETE FROM `creature_template` WHERE `entry` = 990655;
DROP TEMPORARY TABLE IF EXISTS `starcaller_credit`;
CREATE TEMPORARY TABLE `starcaller_credit` AS SELECT * FROM `creature_template` WHERE `entry` = 1;
UPDATE `starcaller_credit` SET `entry` = 990655, `name` = 'Slain by moonlight', `subname` = '', `ScriptName` = '';
INSERT INTO `creature_template` SELECT * FROM `starcaller_credit`;
DROP TEMPORARY TABLE `starcaller_credit`;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 990655;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(990655, 0, 10045, 1, 1);  -- entry 1's model: a credit creature is never seen, but every creature needs one

DELETE FROM `quest_template` WHERE `ID` IN (990650, 990651);
DROP TEMPORARY TABLE IF EXISTS `starcaller_quest`;
CREATE TEMPORARY TABLE `starcaller_quest` AS SELECT * FROM `quest_template` WHERE `ID` = 1473;  -- Creature of the Void
INSERT INTO `starcaller_quest` SELECT * FROM `quest_template` WHERE `ID` = 1473;
UPDATE `starcaller_quest` SET `ID` = 990650 LIMIT 1;
UPDATE `starcaller_quest` SET `ID` = 990651 WHERE `ID` = 1473;
UPDATE `starcaller_quest` SET `QuestType` = 2, `QuestLevel` = -1, `MinLevel` = 10, `QuestSortID` = -263, `Flags` = 0,
  `AllowableRaces` = 0, `RewardXPDifficulty` = 4, `RewardMoney` = 0, `RewardMoneyDifficulty` = 0,
  `RewardNextQuest` = 0, `StartItem` = 0, `RewardItem1` = 0, `RewardAmount1` = 0,
  `RequiredItemId1` = 0, `RequiredItemCount1` = 0, `RequiredNpcOrGo1` = 0, `RequiredNpcOrGoCount1` = 0,
  `ItemDrop1` = 0, `ItemDropQuantity1` = 0, `ObjectiveText1` = '', `AreaDescription` = '', `QuestCompletionLog` = '';

UPDATE `starcaller_quest` SET
  `LogTitle` = 'Feathers for the Watch',
  `RewardNextQuest` = 990651,
  `RequiredItemId1` = 17056,
  `RequiredItemCount1` = 6,
  `LogDescription` = CONCAT('Bring 6 Light Feathers to Priestess Ilyssa Moonveil in Darnassus or Moonseer Tahala in ',
    'Thunder Bluff.'),
  `QuestDescription` = CONCAT(
    'The Sentinels do not keep the night watch alone. The owls keep it with them, and every Sentinel carries a ',
    'feather as a promise that she will not sleep while the forest needs her.$B$B',
    'Bring me six light feathers, $N. The moonkin of Darkshore shed them, and so do the birds of the open plains. ',
    'Hunt for them under the open sky, and you will have started to learn what the watch is.')
WHERE `ID` = 990650;

UPDATE `starcaller_quest` SET
  `LogTitle` = 'By Moonlight',
  `RequiredNpcOrGo1` = 990655,
  `RequiredNpcOrGoCount1` = 8,
  `ObjectiveText1` = 'Slain by moonlight',
  `RewardItem1` = 990650,
  `RewardAmount1` = 1,
  `LogDescription` = CONCAT('Kill 8 creatures that are worth your while as your Moonfire burns them, then return to ',
    'Priestess Ilyssa Moonveil in Darnassus or Moonseer Tahala in Thunder Bluff.'),
  `QuestDescription` = CONCAT(
    'You already hold a little of the moon''s light, $N. Your Moonfire is hers, whatever name your teachers gave it.$B$B',
    'Go out and set it on your enemies, and finish them while it still burns. Eight will do, and none so weak that ',
    'the fight teaches you nothing. When the light has done its work eight times, come back to me.')
WHERE `ID` = 990651;

INSERT INTO `quest_template` SELECT * FROM `starcaller_quest`;
DROP TEMPORARY TABLE `starcaller_quest`;

DELETE FROM `quest_template_addon` WHERE `ID` IN (990650, 990651);
INSERT INTO `quest_template_addon` (`ID`, `AllowableClasses`, `PrevQuestID`) VALUES
(990650, 1024, 0),
(990651, 1024, 990650);

DELETE FROM `quest_details` WHERE `ID` IN (990650, 990651);
INSERT INTO `quest_details` (`ID`, `Emote1`) VALUES (990650, 1), (990651, 1);

DELETE FROM `quest_request_items` WHERE `ID` IN (990650, 990651);
INSERT INTO `quest_request_items` (`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `CompletionText`) VALUES
(990650, 1, 1, 'Have you found the feathers, $N?'),
(990651, 1, 1, 'The moon is patient, $N. Are you?');

DELETE FROM `quest_offer_reward` WHERE `ID` IN (990650, 990651);
INSERT INTO `quest_offer_reward` (`ID`, `Emote1`, `RewardText`) VALUES
(990650, 1, CONCAT('Six feathers, and not one of them bought. Good.$B$BKeep one for yourself. The rest go into the ',
  'moonwell, as the first Sentinels'' did. Now let us see what you do with the light you already carry.')),
(990651, 1, CONCAT('I saw it in your face before you spoke. The light answered you.$B$BTake this book. It holds the ',
  'watch''s oldest words. From today your wrath falls as moonlight and your storms as stars. If one of your spec ',
  'slots needs the storm back, come and see me. It costs you, but the moon does not hold a grudge.'));

DELETE FROM `creature_queststarter` WHERE `quest` IN (990650, 990651);
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
(990650, 990650), (990651, 990650), (990650, 990651), (990651, 990651);
DELETE FROM `creature_questender` WHERE `quest` IN (990650, 990651);
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
(990650, 990650), (990651, 990650), (990650, 990651), (990651, 990651);

-- The Moon's Watch (item 990650) is in solo-wow's spell data (patches/client/spell-data/subclasses/starcaller.json):
-- its generated SQL owns every item in 990000-990999. Its pages stay here.

DELETE FROM `page_text` WHERE `ID` BETWEEN 990650 AND 990652;
INSERT INTO `page_text` (`ID`, `Text`, `NextPageID`) VALUES
(990650, CONCAT(
  'Before there were druids, there were the priestesses of Elune. When the Highborne''s magic tore the world apart, ',
  'the Sisterhood rode out under the moon and stood against the Burning Legion. Tyrande Whisperwind led them, and ',
  'leads them still.\n\nThe Sentinels were born from that war: the daughters of the watch, who sleep by day so that ',
  'the forest is never unguarded at night.'), 990651),
(990651, CONCAT(
  'Malfurion''s druids learned to call the storm. The Sisterhood never needed to. A priestess draws her bow by ',
  'moonlight, and when the night is darkest she asks Elune for the stars themselves.\n\nThe tauren have their own ',
  'name for the light, Mu''sha, the Earth Mother''s left eye. Their seers say she watched over the tauren long ',
  'before the night elves came down from Mount Hyjal to meet them.'), 990652),
(990652, CONCAT(
  'The watch asks three things.\n\nKeep the night, so that others may sleep. Strike only what threatens the ',
  'balance, and let the rest pass. And when the stars fall at your call, remember they are not yours. They are ',
  'lent, as all light is lent.'), 0);
