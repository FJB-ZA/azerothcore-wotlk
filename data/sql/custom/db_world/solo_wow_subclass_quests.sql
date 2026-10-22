-- solo-wow sub-class unlock quests (src/server/scripts/Custom/subclass.cpp). Re-applicable.
-- The Necromancer's chain, offered by both path trainers (solo_wow_subclass_trainers.sql) to warlocks from level 10:
--
--   990170 The Price of a Soul  bring one Soul Shard
--   990171 The Last Breath      kill 8 foes worth experience (credited by the core script as creature 990175),
--                               rewards The Grave Ledger (990170) and puts the active spec slot on the path
--
-- subclass_path_unlock names 990171: a trainer teaches the path to other spec slots only after it.

CREATE TABLE IF NOT EXISTS `subclass_path_unlock` (
  `path_id` TINYINT UNSIGNED NOT NULL,
  `quest` INT UNSIGNED NOT NULL COMMENT 'trainers teach the path after it; its reward puts the active slot on it',
  PRIMARY KEY (`path_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELETE FROM `subclass_path_unlock` WHERE `path_id` = 1;
INSERT INTO `subclass_path_unlock` (`path_id`, `quest`) VALUES (1, 990171);

DELETE FROM `creature_template` WHERE `entry` = 990175;
DROP TEMPORARY TABLE IF EXISTS `solo_wow_credit`;
CREATE TEMPORARY TABLE `solo_wow_credit` AS SELECT * FROM `creature_template` WHERE `entry` = 1;
UPDATE `solo_wow_credit` SET `entry` = 990175, `name` = 'Last breath heard', `subname` = '', `ScriptName` = '';
INSERT INTO `creature_template` SELECT * FROM `solo_wow_credit`;
DROP TEMPORARY TABLE `solo_wow_credit`;

DELETE FROM `quest_template` WHERE `ID` IN (990170, 990171);
DROP TEMPORARY TABLE IF EXISTS `solo_wow_quest`;
CREATE TEMPORARY TABLE `solo_wow_quest` AS SELECT * FROM `quest_template` WHERE `ID` = 1473;  -- Creature of the Void
INSERT INTO `solo_wow_quest` SELECT * FROM `quest_template` WHERE `ID` = 1473;
UPDATE `solo_wow_quest` SET `ID` = 990170 LIMIT 1;
UPDATE `solo_wow_quest` SET `ID` = 990171 WHERE `ID` = 1473;
UPDATE `solo_wow_quest` SET `QuestType` = 2, `QuestLevel` = -1, `MinLevel` = 10, `QuestSortID` = -61, `Flags` = 0,
  `AllowableRaces` = 0, `RewardXPDifficulty` = 4, `RewardMoney` = 0, `RewardMoneyDifficulty` = 0,
  `RewardNextQuest` = 0, `StartItem` = 0, `RewardItem1` = 0, `RewardAmount1` = 0,
  `RequiredItemId1` = 0, `RequiredItemCount1` = 0, `RequiredNpcOrGo1` = 0, `RequiredNpcOrGoCount1` = 0,
  `ItemDrop1` = 0, `ItemDropQuantity1` = 0, `ObjectiveText1` = '', `AreaDescription` = '', `QuestCompletionLog` = '';

UPDATE `solo_wow_quest` SET
  `LogTitle` = 'The Price of a Soul',
  `RewardNextQuest` = 990171,
  `RequiredItemId1` = 6265,
  `RequiredItemCount1` = 1,
  `LogDescription` = 'Bring a Soul Shard to Kelvar Grimm in the Undercity or Aldric Blackmoor at Raven Hill.',
  `QuestDescription` = CONCAT(
    'So you heard the dead talking. Most warlocks hear them once, then spend the rest of their lives pretending they ',
    'didn''t.$B$BI''ll keep this simple, $N. A demon serves you because it''s afraid of you. The dead aren''t afraid of ',
    'anything. They answer to whoever has already paid the price, and the price is a soul.$B$BYou know how to take ',
    'one. Bring me a shard with a soul still in it, and we''ll see if you mean this.')
WHERE `ID` = 990170;

UPDATE `solo_wow_quest` SET
  `LogTitle` = 'The Last Breath',
  `RequiredNpcOrGo1` = 990175,
  `RequiredNpcOrGoCount1` = 8,
  `ObjectiveText1` = 'Last breath heard',
  `RewardItem1` = 990170,
  `RewardAmount1` = 1,
  `LogDescription` = CONCAT('Kill 8 enemies that are worth your while, then return to Kelvar Grimm in the Undercity ',
    'or Aldric Blackmoor at Raven Hill.'),
  `QuestDescription` = CONCAT(
    'When something dies there''s a moment, just one, when its soul hasn''t decided where to go. A warlock tears it ',
    'out and stuffs it in a shard. We do something quieter. We listen to it leave, and we keep the echo.$B$BSo go and ',
    'kill. Eight times, and nothing so weak it isn''t worth your time. A rabbit''s last breath teaches you nothing.',
    '$B$BCome back when you''ve heard eight of them go.')
WHERE `ID` = 990171;

INSERT INTO `quest_template` SELECT * FROM `solo_wow_quest`;
DROP TEMPORARY TABLE `solo_wow_quest`;

DELETE FROM `quest_template_addon` WHERE `ID` IN (990170, 990171);
INSERT INTO `quest_template_addon` (`ID`, `AllowableClasses`, `PrevQuestID`) VALUES
(990170, 256, 0),
(990171, 256, 990170);

DELETE FROM `quest_details` WHERE `ID` IN (990170, 990171);
INSERT INTO `quest_details` (`ID`, `Emote1`) VALUES (990170, 1), (990171, 1);

DELETE FROM `quest_request_items` WHERE `ID` IN (990170, 990171);
INSERT INTO `quest_request_items` (`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `CompletionText`) VALUES
(990170, 1, 1, 'Well? The dead don''t wait long, $N. Neither do I.'),
(990171, 1, 1, 'How many have you heard? Be honest. The dead can tell when you''re counting wrong.');

DELETE FROM `quest_offer_reward` WHERE `ID` IN (990170, 990171);
INSERT INTO `quest_offer_reward` (`ID`, `Emote1`, `RewardText`) VALUES
(990170, 1, CONCAT('Still warm. Good.$B$BYou took this from something that wanted very badly to keep living. ',
  'Remember how that felt. Everything I teach you from here starts with it.')),
(990171, 1, CONCAT('There. You hear it now, don''t you? A little cold at the back of your skull, like a draft under ',
  'a door. That''s the grave, $N, and it knows your name.$B$BTake the ledger. It''s what we know, and what we managed ',
  'to keep. From today your fire answers to frost and plague, and your demons to bone. If you want the old ways back ',
  'for one of your spec slots, come and see me. It''ll cost you, but I won''t stop you.'));

DELETE FROM `creature_queststarter` WHERE `quest` IN (990170, 990171);
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
(990170, 990170), (990171, 990170), (990170, 990171), (990171, 990171);
DELETE FROM `creature_questender` WHERE `quest` IN (990170, 990171);
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
(990170, 990170), (990171, 990170), (990170, 990171), (990171, 990171);

-- The Grave Ledger: a readable book, copied from An Unopened Tome of Advice (37674) with an old-book look (9165)
DELETE FROM `item_template` WHERE `entry` = 990170;
DROP TEMPORARY TABLE IF EXISTS `solo_wow_book`;
CREATE TEMPORARY TABLE `solo_wow_book` AS SELECT * FROM `item_template` WHERE `entry` = 37674;
UPDATE `solo_wow_book` SET `entry` = 990170, `name` = 'The Grave Ledger', `displayid` = 9165, `Quality` = 1,
  `bonding` = 1, `PageText` = 990170, `PageMaterial` = 1, `LanguageID` = 0, `startquest` = 0, `stackable` = 1,
  `maxcount` = 1, `BuyPrice` = 0, `SellPrice` = 0,
  `description` = 'Kept by those who answer the dead.';
INSERT INTO `item_template` SELECT * FROM `solo_wow_book`;
DROP TEMPORARY TABLE `solo_wow_book`;

DELETE FROM `page_text` WHERE `ID` BETWEEN 990170 AND 990172;
INSERT INTO `page_text` (`ID`, `Text`, `NextPageID`) VALUES
(990170, CONCAT(
  'Every warlock learns the same lesson early. A demon serves only for as long as it fears you, and the day it ',
  'stops, it eats you.\n\nThe dead are different. They want nothing, because they have nothing left to want.\n\n',
  'Kel''Thuzad understood this. He was a magus of the Kirin Tor before he went north to Northrend, and when he came ',
  'back he carried a voice with him. The Cult of the Damned grew out of the plague villages of Lordaeron, and its ',
  'first teachers were men Dalaran had told no.'), 990171),
(990171, CONCAT(
  'Most of what the Cult taught died with it at Stratholme and Andorhal. Not all of it.\n\nIn the Undercity the ',
  'Forsaken keep their own grave-callers, and the Dark Lady doesn''t ask what goes on in the lower halls so long as ',
  'it is pointed at the Scourge. In Duskwood, Morbent Fel raised the dead of Raven Hill for his own ends, and that ',
  'town has never been quiet since. The art survives wherever people are too frightened to look closely.'), 990172),
(990172, CONCAT(
  'The art is simple to say and hard to keep.\n\nFrost slows the heart. Plague rots whatever the frost leaves. Bone ',
  'answers whoever calls it by name.\n\nYou''ll find the old fire harder to reach for now, and that''s as it should ',
  'be. Fire burns out. What you raise stays raised.\n\nRead this twice. Then go and listen.'), 0);
