-- Witch Doctor trainers, unlock quests and lore book (path 4). Re-applicable.
--
--   990800 Witch Doctor Zun'jabu: Orgrimmar's Valley of Wisdom, by the shaman trainers (Horde)
--   990801 Witch Doctor Mai'zoth: the Exodar, beside Farseer Nobundo (Alliance)
-- Both are Zandalari and neutral, so either faction can use either one. Move a spawn in game with .npc move if it
-- stands badly. They use the core's npc_subclass_path gossip: a shaman who has done the quests sets the active spec
-- slot's path there, for the path change fee.
--
--   990800 A Doll for the Loa  bring 4 Linen Cloth
--   990801 Feeding the Loa     kill 8 creatures worth experience while the shaman's own Flame Shock burns them
--                              (credited as creature 990805 by Custom/Subclasses/WitchDoctor/witch_doctor_quests.cpp);
--                              rewards The Loa's Due (item 990800) and puts the active spec slot on the path for free.
--                              subclass_path.unlock_quest names it.

DELETE FROM `npc_text` WHERE `ID` BETWEEN 990800 AND 990803;
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`) VALUES
(990800, CONCAT('Earth and fire answer you, $c. Good. But they not be the only spirits listening.$B$BThe loa are ',
  'older than your elements, and they always hungry. Feed them and they lend you their voodoo: shadow where you had ',
  'fire, wards where you had totems. Do what I ask and I teach each of your spec slots the old ways. You can go back ',
  'to your elements whenever you like. It costs a little more each time, so choose carefully.'), '', 1),
(990801, 'The loa have no use for you, $c. Go on now.', '', 1),
(990802, CONCAT('You look surprised to find a troll here, $c. The Zandalar go where we must. The Blood God stirs in ',
  'Zul''Gurub, and we need every shaman who can hear the spirits, even the ones who fell from the sky.$B$BYour ',
  'elements are strong. The loa are older, and they do not care what color your skin is. Earn their favor and I will ',
  'teach each of your spec slots our voodoo. If you want your elements back later, come and see me, but it costs a ',
  'little more each time.'), '', 1),
(990803, 'You hear no spirits, $c. I have nothing to teach you.', '', 1);

-- Shamans get the teaching text, everyone else the refusal; each trainer has their own voice
DELETE FROM `gossip_menu` WHERE `MenuID` IN (990800, 990801);
INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES (990800, 990800), (990800, 990801), (990801, 990802), (990801, 990803);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 14 AND `SourceGroup` IN (990800, 990801);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `ConditionTypeOrReference`,
  `ConditionValue1`, `NegativeCondition`, `Comment`) VALUES
(14, 990800, 990800, 15, 64, 0, 'Witch Doctor trainer Zun''jabu: teaching text for shamans'),
(14, 990800, 990801, 15, 64, 1, 'Witch Doctor trainer Zun''jabu: refusal for other classes'),
(14, 990801, 990802, 15, 64, 0, 'Witch Doctor trainer Mai''zoth: teaching text for shamans'),
(14, 990801, 990803, 15, 64, 1, 'Witch Doctor trainer Mai''zoth: refusal for other classes');

DELETE FROM `creature_template` WHERE `entry` IN (990800, 990801);
DROP TEMPORARY TABLE IF EXISTS `witch_doctor_trainer`;
CREATE TEMPORARY TABLE `witch_doctor_trainer` AS SELECT * FROM `creature_template` WHERE `entry` = 3344;  -- Kardris Dreamseeker
UPDATE `witch_doctor_trainer` SET `entry` = 990800, `name` = 'Witch Doctor Zun''jabu', `subname` = 'Witch Doctor',
  `gossip_menu_id` = 990800;
INSERT INTO `witch_doctor_trainer` SELECT * FROM `creature_template` WHERE `entry` = 3344;
UPDATE `witch_doctor_trainer` SET `entry` = 990801, `name` = 'Witch Doctor Mai''zoth', `subname` = 'Witch Doctor',
  `gossip_menu_id` = 990801 WHERE `entry` = 3344;
UPDATE `witch_doctor_trainer` SET `faction` = 35, `npcflag` = 3, `ScriptName` = 'npc_subclass_path';
INSERT INTO `creature_template` SELECT * FROM `witch_doctor_trainer`;
DROP TEMPORARY TABLE `witch_doctor_trainer`;

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (990800, 990801);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(990800, 0, 14996, 1, 1),  -- Maywiki of Zuldazar's model
(990801, 0, 14995, 1, 1);  -- Al'tabim the All-Seeing's model

DELETE FROM `creature` WHERE `guid` IN (9990800, 9990801);
DROP TEMPORARY TABLE IF EXISTS `witch_doctor_spawn`;
CREATE TEMPORARY TABLE `witch_doctor_spawn` AS SELECT * FROM `creature` WHERE `guid` = 4663;  -- Kardris Dreamseeker
UPDATE `witch_doctor_spawn` SET `guid` = 9990800, `id` = 990800, `position_x` = 1937.5, `position_y` = -4212.0,
  `position_z` = 42.42, `orientation` = 3.14;
INSERT INTO `witch_doctor_spawn` SELECT * FROM `creature` WHERE `guid` = 61719;  -- Farseer Nobundo
UPDATE `witch_doctor_spawn` SET `guid` = 9990801, `id` = 990801, `position_x` = -3809.5, `position_y` = -11400.0,
  `position_z` = -104.30, `orientation` = 0.6 WHERE `guid` = 61719;
UPDATE `witch_doctor_spawn` SET `equipment_id` = 0, `wander_distance` = 0, `MovementType` = 0;
INSERT INTO `creature` SELECT * FROM `witch_doctor_spawn`;
DROP TEMPORARY TABLE `witch_doctor_spawn`;

DELETE FROM `creature_template` WHERE `entry` = 990805;
DROP TEMPORARY TABLE IF EXISTS `witch_doctor_credit`;
CREATE TEMPORARY TABLE `witch_doctor_credit` AS SELECT * FROM `creature_template` WHERE `entry` = 1;
UPDATE `witch_doctor_credit` SET `entry` = 990805, `name` = 'Offering to the loa', `subname` = '', `ScriptName` = '';
INSERT INTO `creature_template` SELECT * FROM `witch_doctor_credit`;
DROP TEMPORARY TABLE `witch_doctor_credit`;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 990805;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(990805, 0, 10045, 1, 1);  -- entry 1's model: a credit creature is never seen, but every creature needs one

DELETE FROM `quest_template` WHERE `ID` IN (990800, 990801);
DROP TEMPORARY TABLE IF EXISTS `witch_doctor_quest`;
CREATE TEMPORARY TABLE `witch_doctor_quest` AS SELECT * FROM `quest_template` WHERE `ID` = 1473;  -- Creature of the Void
INSERT INTO `witch_doctor_quest` SELECT * FROM `quest_template` WHERE `ID` = 1473;
UPDATE `witch_doctor_quest` SET `ID` = 990800 LIMIT 1;
UPDATE `witch_doctor_quest` SET `ID` = 990801 WHERE `ID` = 1473;
UPDATE `witch_doctor_quest` SET `QuestType` = 2, `QuestLevel` = -1, `MinLevel` = 10, `QuestSortID` = -82, `Flags` = 0,
  `AllowableRaces` = 0, `RewardXPDifficulty` = 4, `RewardMoney` = 0, `RewardMoneyDifficulty` = 0,
  `RewardNextQuest` = 0, `StartItem` = 0, `RewardItem1` = 0, `RewardAmount1` = 0,
  `RequiredItemId1` = 0, `RequiredItemCount1` = 0, `RequiredNpcOrGo1` = 0, `RequiredNpcOrGoCount1` = 0,
  `ItemDrop1` = 0, `ItemDropQuantity1` = 0, `ObjectiveText1` = '', `AreaDescription` = '', `QuestCompletionLog` = '';

UPDATE `witch_doctor_quest` SET
  `LogTitle` = 'A Doll for the Loa',
  `RewardNextQuest` = 990801,
  `RequiredItemId1` = 2589,
  `RequiredItemCount1` = 4,
  `LogDescription` = CONCAT('Bring 4 Linen Cloth to Witch Doctor Zun''jabu in Orgrimmar or Witch Doctor Mai''zoth in ',
    'the Exodar.'),
  `QuestDescription` = CONCAT(
    'Before the loa listen to you, they need to know your face. For that we make a doll. Not a toy for a child, ',
    '$N. A voodoo doll, with a little of you stitched inside it.$B$B',
    'Bring me four pieces of linen cloth. Take it from the ones you fight; plenty of them walking around wearing it.')
WHERE `ID` = 990800;

UPDATE `witch_doctor_quest` SET
  `LogTitle` = 'Feeding the Loa',
  `RequiredNpcOrGo1` = 990805,
  `RequiredNpcOrGoCount1` = 8,
  `ObjectiveText1` = 'Offering to the loa',
  `RewardItem1` = 990800,
  `RewardAmount1` = 1,
  `LogDescription` = CONCAT('Kill 8 creatures that are worth your while as your Flame Shock burns them, then return to ',
    'Witch Doctor Zun''jabu in Orgrimmar or Witch Doctor Mai''zoth in the Exodar.'),
  `QuestDescription` = CONCAT(
    'The loa know your face now. Next they want a taste of what you can do.$B$B',
    'Go out and set your Flame Shock on your enemies, and kill them while it still burns. The fire carries their ',
    'spirits to the loa. Eight will do, and none so weak the loa would laugh at the offering.$B$B',
    'Come back when the loa are fed, $N.')
WHERE `ID` = 990801;

INSERT INTO `quest_template` SELECT * FROM `witch_doctor_quest`;
DROP TEMPORARY TABLE `witch_doctor_quest`;

DELETE FROM `quest_template_addon` WHERE `ID` IN (990800, 990801);
INSERT INTO `quest_template_addon` (`ID`, `AllowableClasses`, `PrevQuestID`) VALUES
(990800, 64, 0),
(990801, 64, 990800);

DELETE FROM `quest_details` WHERE `ID` IN (990800, 990801);
INSERT INTO `quest_details` (`ID`, `Emote1`) VALUES (990800, 1), (990801, 1);

DELETE FROM `quest_request_items` WHERE `ID` IN (990800, 990801);
INSERT INTO `quest_request_items` (`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `CompletionText`) VALUES
(990800, 1, 1, 'The loa are waiting, $N. You bring the linen?'),
(990801, 1, 1, 'The loa still hungry, $N. I can feel it from here.');

DELETE FROM `quest_offer_reward` WHERE `ID` IN (990800, 990801);
INSERT INTO `quest_offer_reward` (`ID`, `Emote1`, `RewardText`) VALUES
(990800, 1, CONCAT('Good. Now watch.$B$BA little cloth, a little of your hair, a few words in the old tongue... ',
  'there. The doll stays with me, and through it the loa will know you. Now they need to know what you can offer.')),
(990801, 1, CONCAT('The loa are fed, and they are pleased with you.$B$BTake this book. It tells what we owe the loa ',
  'and what they give back. From today your fire burns black with their voodoo, and your totems rise as wards. If ',
  'you want your elements back for one of your spec slots, come and see me. It costs you, but the loa do not hold a ',
  'grudge. Mostly.'));

DELETE FROM `creature_queststarter` WHERE `quest` IN (990800, 990801);
INSERT INTO `creature_queststarter` (`id`, `quest`) VALUES
(990800, 990800), (990801, 990800), (990800, 990801), (990801, 990801);
DELETE FROM `creature_questender` WHERE `quest` IN (990800, 990801);
INSERT INTO `creature_questender` (`id`, `quest`) VALUES
(990800, 990800), (990801, 990800), (990800, 990801), (990801, 990801);

-- The Loa's Due (item 990800) is in solo-wow's spell data (patches/client/spell-data/subclasses/witch-doctor.json):
-- its generated SQL owns every item in 990000-990999. Its pages stay here.

DELETE FROM `page_text` WHERE `ID` BETWEEN 990800 AND 990802;
INSERT INTO `page_text` (`ID`, `Text`, `NextPageID`) VALUES
(990800, CONCAT(
  'The trolls have bargained with the loa since before there were kingdoms. Shirvallah the tiger, Hir''eek the bat, ',
  'Bethekk the panther, Hakkar the Blood God: each takes what it is owed and gives something back. A witch doctor is ',
  'the one who keeps the accounts.\n\nThe Zandalar Tribe has kept them longest of all.'), 990801),
(990801, CONCAT(
  'When the Gurubashi priests woke Hakkar in Zul''Gurub, they forgot that a loa takes as much as it is allowed. ',
  'Hakkar wanted blood, and they let him have it. The Zandalar came out of Yojamba Isle to put him back in the dark, ',
  'and they asked shamans of every race to help. A loa does not care whose hand makes the offering.\n\nSo a ',
  'Zandalari witch doctor will teach an orc, a tauren or a draenei. They will not teach just anyone.'), 990802),
(990802, CONCAT(
  'What the loa ask is simple.\n\nBurn what you offer, and waste none of it. Plant a ward where a totem would ',
  'stand, so the spirits know whose it is.\n\nPay the loa what they are owed and they pay you back. Forget, and ',
  'they come to collect.'), 0);
