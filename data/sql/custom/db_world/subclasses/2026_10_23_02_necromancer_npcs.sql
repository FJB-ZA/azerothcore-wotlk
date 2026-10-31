-- Necromancer trainers, unlock quests and lore book (path 2). Re-applicable.
--
--   990370 Kelvar Grimm, Necromancer: Undercity, by the warlock trainers in the Magic Quarter (Horde)
--   990371 Aldric Blackmoor, Outcast Necromancer: Raven Hill in Duskwood, by Abercrombie's shack (Alliance)
-- Both are neutral, so either faction can use either one. Move a spawn in game with .npc move if it stands badly.
-- They use the core's npc_subclass_path gossip: a warlock who has done the quests sets the active spec slot's path
-- there, for the path change fee.
--
--   990370 The Price of a Soul  bring one Soul Shard
--   990371 The Last Breath      kill 8 foes worth experience (credited as creature 990375 by
--                               Custom/Subclasses/Necromancer/necromancer_quests.cpp); rewards The Grave Ledger
--                               (item 990370) and puts the active spec slot on the path for free.
--                               subclass_path.unlock_quest names it.

DELETE FROM `npc_text` WHERE `ID` IN (990370, 990371);
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(990370, CONCAT('The living fear what lies beneath their feet. I stopped fearing it a long time ago.$B$B',
  'I can teach you to call bone, frost and plague where you once called fel fire. Show me you can hear the dead ',
  'first. After that, each of your spec slots can walk the path, and you can always go back to the old ways. That ',
  'costs a little more each time, so choose with care.'), '', 1),
(990371, 'You have no gift for this, $c. The dead will not answer you.', '', 1);

-- Warlocks get the teaching text, everyone else the refusal
DELETE FROM `gossip_menu` WHERE `MenuID` = 990370;
INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES (990370, 990370), (990370, 990371);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 14 AND `SourceGroup` = 990370;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `ConditionTypeOrReference`,
  `ConditionValue1`, `NegativeCondition`, `Comment`) VALUES
(14, 990370, 990370, 15, 256, 0, 'Necromancer trainer: teaching text for warlocks'),
(14, 990370, 990371, 15, 256, 1, 'Necromancer trainer: refusal for other classes');

DELETE FROM `creature_template` WHERE `entry` IN (990370, 990371);
DROP TEMPORARY TABLE IF EXISTS `necromancer_trainer`;
CREATE TEMPORARY TABLE `necromancer_trainer` AS SELECT * FROM `creature_template` WHERE `entry` = 4563;
UPDATE `necromancer_trainer` SET `entry` = 990370, `name` = 'Kelvar Grimm', `subname` = 'Necromancer';
INSERT INTO `necromancer_trainer` SELECT * FROM `creature_template` WHERE `entry` = 4563;
UPDATE `necromancer_trainer` SET `entry` = 990371, `name` = 'Aldric Blackmoor', `subname` = 'Outcast Necromancer'
WHERE `entry` = 4563;
UPDATE `necromancer_trainer` SET `faction` = 35, `npcflag` = 3, `gossip_menu_id` = 990370,
  `ScriptName` = 'npc_subclass_path';
INSERT INTO `creature_template` SELECT * FROM `necromancer_trainer`;
DROP TEMPORARY TABLE `necromancer_trainer`;

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (990370, 990371);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(990370, 0, 2582, 1, 1),   -- Scourge Necromancer's model
(990371, 0, 10427, 1, 1);  -- a Cult of the Damned necromancer's model

DELETE FROM `creature` WHERE `guid` IN (9990370, 9990371);
DROP TEMPORARY TABLE IF EXISTS `necromancer_spawn`;
CREATE TEMPORARY TABLE `necromancer_spawn` AS SELECT * FROM `creature` WHERE `guid` = 41834;  -- Kaal Soulreaper
UPDATE `necromancer_spawn` SET `guid` = 9990370, `id` = 990370, `position_x` = 1783.5, `position_y` = 47.5,
  `position_z` = -61.41, `orientation` = 2.2;
INSERT INTO `necromancer_spawn` SELECT * FROM `creature` WHERE `guid` = 4199;  -- Abercrombie
UPDATE `necromancer_spawn` SET `guid` = 9990371, `id` = 990371, `position_x` = -10279.5, `position_y` = 77.5,
  `position_z` = 38.93, `orientation` = 3.9 WHERE `guid` = 4199;
UPDATE `necromancer_spawn` SET `equipment_id` = 0, `wander_distance` = 0, `MovementType` = 0;
INSERT INTO `creature` SELECT * FROM `necromancer_spawn`;
DROP TEMPORARY TABLE `necromancer_spawn`;

DELETE FROM `creature_template` WHERE `entry` = 990375;
DROP TEMPORARY TABLE IF EXISTS `necromancer_credit`;
CREATE TEMPORARY TABLE `necromancer_credit` AS SELECT * FROM `creature_template` WHERE `entry` = 1;
UPDATE `necromancer_credit` SET `entry` = 990375, `name` = 'Last breath heard', `subname` = '', `ScriptName` = '';
INSERT INTO `creature_template` SELECT * FROM `necromancer_credit`;
DROP TEMPORARY TABLE `necromancer_credit`;

DELETE FROM `quest_template` WHERE `ID` IN (990370, 990371);
DROP TEMPORARY TABLE IF EXISTS `necromancer_quest`;
CREATE TEMPORARY TABLE `necromancer_quest` AS SELECT * FROM `quest_template` WHERE `ID` = 1473;  -- Creature of the Void
INSERT INTO `necromancer_quest` SELECT * FROM `quest_template` WHERE `ID` = 1473;
UPDATE `necromancer_quest` SET `ID` = 990370 LIMIT 1;
UPDATE `necromancer_quest` SET `ID` = 990371 WHERE `ID` = 1473;
UPDATE `necromancer_quest` SET `QuestType` = 2, `QuestLevel` = -1, `MinLevel` = 10, `QuestSortID` = -61, `Flags` = 0,
  `AllowableRaces` = 0, `RewardXPDifficulty` = 4, `RewardMoney` = 0, `RewardMoneyDifficulty` = 0,
  `RewardNextQuest` = 0, `StartItem` = 0, `RewardItem1` = 0, `RewardAmount1` = 0,
  `RequiredItemId1` = 0, `RequiredItemCount1` = 0, `RequiredNpcOrGo1` = 0, `RequiredNpcOrGoCount1` = 0,
  `ItemDrop1` = 0, `ItemDropQuantity1` = 0, `ObjectiveText1` = '', `AreaDescription` = '', `QuestCompletionLog` = '';

UPDATE `necromancer_quest` SET
  `LogTitle` = 'The Price of a Soul',
  `RewardNextQuest` = 990371,
  `RequiredItemId1` = 6265,
  `RequiredItemCount1` = 1,
  `LogDescription` = 'Bring a Soul Shard to Kelvar Grimm in the Undercity or Aldric Blackmoor at Raven Hill.',
  `QuestDescription` = CONCAT(
    'So you heard the dead talking. Most warlocks hear them once, then spend the rest of their lives pretending they ',
    'didn''t.$B$BI''ll keep this simple, $N. A demon serves you because it''s afraid of you. The dead aren''t afraid of ',
    'anything. They answer to whoever has already paid the price, and the price is a soul.$B$BYou know how to take ',
    'one. Bring me a shard with a soul still in it, and we''ll see if you mean this.')
WHERE `ID` = 990370;

UPDATE `necromancer_quest` SET
  `LogTitle` = 'The Last Breath',
  `RequiredNpcOrGo1` = 990375,
  `RequiredNpcOrGoCount1` = 8,
  `ObjectiveText1` = 'Last breath heard',
  `RewardItem1` = 990370,
  `RewardAmount1` = 1,
  `LogDescription` = CONCAT('Kill 8 enemies that are worth your while, then return to Kelvar Grimm in the Undercity ',
    'or Aldric Blackmoor at Raven Hill.'),
  `QuestDescription` = CONCAT(
    'When something dies there''s a moment, just one, when its soul hasn''t decided where to go. A warlock tears it ',
    'out and stuffs it in a shard. We do something quieter. We listen to it leave, and we keep the echo.$B$BSo go and ',
    'kill. Eight times, and nothing so weak it isn''t worth your time. A rabbit''s last breath teaches you nothing.',
    '$B$BCome back when you''ve heard eight of them go.')
WHERE `ID` = 990371;

INSERT INTO `quest_template` SELECT * FROM `necromancer_quest`;
DROP TEMPORARY TABLE `necromancer_quest`;

DELETE FROM `quest_template_addon` WHERE `ID` IN (990370, 990371);
INSERT INTO `quest_template_addon` (`ID`, `AllowableClasses`, `PrevQuestID`) VALUES
(990370, 256, 0),
(990371, 256, 990370);

DELETE FROM `quest_details` WHERE `ID` IN (990370, 990371);
INSERT INTO `quest_details` (`ID`, `Emote1`) VALUES (990370, 1), (990371, 1);

DELETE FROM `quest_request_items` WHERE `ID` IN (990370, 990371);
INSERT INTO `quest_request_items` (`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `CompletionText`) VALUES
(990370, 1, 1, 'Well? The dead don''t wait long, $N. Neither do I.'),
(990371, 1, 1, 'How many have you heard? Be honest. The dead can tell when you''re counting wrong.');

DELETE FROM `quest_offer_reward` WHERE `ID` IN (990370, 990371);
INSERT INTO `quest_offer_reward` (`ID`, `Emote1`, `RewardText`) VALUES
(990370, 1, CONCAT('Still warm. Good.$B$BYou took this from something that wanted very badly to keep living. ',
  'Remember how that felt. Everything I teach you from here starts with it.')),
(990371, 1, CONCAT('There. You hear it now, don''t you? A little cold at the back of your skull, like a draft under ',
  'a door. That''s the grave, $N, and it knows your name.$B$BTake the ledger. It''s what we know, and what we managed ',
  'to keep. From today your fire answers to frost and plague, and your demons to bone. If you want the old ways back ',
  'for one of your spec slots, come and see me. It''ll cost you, but I won''t stop you.'));

DELETE FROM `creature_queststarter` WHERE `quest` IN (990370, 990371);
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
(990370, 990370), (990371, 990370), (990370, 990371), (990371, 990371);
DELETE FROM `creature_questender` WHERE `quest` IN (990370, 990371);
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
(990370, 990370), (990371, 990370), (990370, 990371), (990371, 990371);

-- The Grave Ledger (item 990370), a readable book copied from An Unopened Tome of Advice (37674) with an old-book look
-- (9165), is in solo-wow's spell data (patches/client/spell-data/subclasses/necromancer-items.json): its generated SQL
-- owns every item in 990000-990999. Its pages stay here.

DELETE FROM `page_text` WHERE `ID` BETWEEN 990370 AND 990372;
INSERT INTO `page_text` (`ID`, `Text`, `NextPageID`) VALUES
(990370, CONCAT(
  'Every warlock learns the same lesson early. A demon serves only for as long as it fears you, and the day it ',
  'stops, it eats you.\n\nThe dead are different. They want nothing, because they have nothing left to want.\n\n',
  'Kel''Thuzad understood this. He was a magus of the Kirin Tor before he went north to Northrend, and when he came ',
  'back he carried a voice with him. The Cult of the Damned grew out of the plague villages of Lordaeron, and its ',
  'first teachers were men Dalaran had told no.'), 990371),
(990371, CONCAT(
  'Most of what the Cult taught died with it at Stratholme and Andorhal. Not all of it.\n\nIn the Undercity the ',
  'Forsaken keep their own grave-callers, and the Dark Lady doesn''t ask what goes on in the lower halls so long as ',
  'it is pointed at the Scourge. In Duskwood, Morbent Fel raised the dead of Raven Hill for his own ends, and that ',
  'town has never been quiet since. The art survives wherever people are too frightened to look closely.'), 990372),
(990372, CONCAT(
  'The art is simple to say and hard to keep.\n\nFrost slows the heart. Plague rots whatever the frost leaves. Bone ',
  'answers whoever calls it by name.\n\nYou''ll find the old fire harder to reach for now, and that''s as it should ',
  'be. Fire burns out. What you raise stays raised.\n\nRead this twice. Then go and listen.'), 0);
