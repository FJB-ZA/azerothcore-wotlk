-- solo-wow sub-class minions (Necromancer path; src/server/scripts/Custom/subclass.cpp). Re-applicable.
-- Each minion is the demon it replaces in an undead body: a copy of the demon's creature row (family, class, stats,
-- scaling, flags and scripts), its spells, movement and level stats, with an existing undead model and name. So the
-- pet bar, pet AI, pet talents (Demonic Sacrifice, Soul Link) and every number behave as the demon's.
--
--   990150 Skeletal Archer  <- Imp 416         model 9785 (Skeletal Guardian)
--   990151 Flesh Golem      <- Voidwalker 1860  model 1693 (Flesh Golem)
--   990152 Ghoul            <- Succubus 1863    model 571 (Ghoul)
--   990153 Banshee          <- Felhunter 417    model 8782 (Wailing Banshee)
--   990154 Abomination      <- Felguard 17252   model 12818 (Abomination)
--   990155 Bone Colossus    <- Infernal 89      model 12073 (Bone Construct)
--   990156 Lich Lieutenant  <- Doomguard 11859  model 17444 (Lich)
--   990157 Skeletal Archer  <- Imp 416          model 9789, the second archer: a guardian next to the pet
--
-- The two archers each have half the Imp's health (pet_levelstats below) and deal half its damage
-- (subclass_creature_swap.damage_pct), so together they match one Imp.

DELETE FROM `creature_template` WHERE `entry` BETWEEN 990150 AND 990157;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 990150 AND 990157;
DELETE FROM `creature_template_addon` WHERE `entry` BETWEEN 990150 AND 990157;
DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 990150 AND 990157;
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 990150 AND 990157;
DELETE FROM `pet_levelstats` WHERE `creature_entry` BETWEEN 990150 AND 990157;
DELETE FROM `pet_name_generation` WHERE `entry` BETWEEN 990150 AND 990157;

DROP TEMPORARY TABLE IF EXISTS `solo_wow_minion_map`;
CREATE TEMPORARY TABLE `solo_wow_minion_map` (
  `demon` INT UNSIGNED NOT NULL,
  `minion` INT UNSIGNED NOT NULL PRIMARY KEY,
  `name` VARCHAR(40) NOT NULL,
  `display` INT UNSIGNED NOT NULL,
  `health_pct` TINYINT UNSIGNED NOT NULL
);
INSERT INTO `solo_wow_minion_map` (`demon`, `minion`, `name`, `display`, `health_pct`) VALUES
(416, 990150, 'Skeletal Archer', 9785, 50),
(1860, 990151, 'Flesh Golem', 1693, 100),
(1863, 990152, 'Ghoul', 571, 100),
(417, 990153, 'Banshee', 8782, 100),
(17252, 990154, 'Abomination', 12818, 100),
(89, 990155, 'Bone Colossus', 12073, 100),
(11859, 990156, 'Lich Lieutenant', 17444, 100),
(416, 990157, 'Skeletal Archer', 9789, 50);

-- creature_template: copy the whole row, then change the entry and name
DROP TEMPORARY TABLE IF EXISTS `solo_wow_minion`;
CREATE TEMPORARY TABLE `solo_wow_minion` AS SELECT * FROM `creature_template` WHERE 1 = 0;
INSERT INTO `solo_wow_minion` SELECT * FROM `creature_template` WHERE `entry` = 416;
UPDATE `solo_wow_minion` SET `entry` = 990150, `name` = 'Skeletal Archer' WHERE `entry` = 416;
INSERT INTO `solo_wow_minion` SELECT * FROM `creature_template` WHERE `entry` = 1860;
UPDATE `solo_wow_minion` SET `entry` = 990151, `name` = 'Flesh Golem' WHERE `entry` = 1860;
INSERT INTO `solo_wow_minion` SELECT * FROM `creature_template` WHERE `entry` = 1863;
UPDATE `solo_wow_minion` SET `entry` = 990152, `name` = 'Ghoul' WHERE `entry` = 1863;
INSERT INTO `solo_wow_minion` SELECT * FROM `creature_template` WHERE `entry` = 417;
UPDATE `solo_wow_minion` SET `entry` = 990153, `name` = 'Banshee' WHERE `entry` = 417;
INSERT INTO `solo_wow_minion` SELECT * FROM `creature_template` WHERE `entry` = 17252;
UPDATE `solo_wow_minion` SET `entry` = 990154, `name` = 'Abomination' WHERE `entry` = 17252;
INSERT INTO `solo_wow_minion` SELECT * FROM `creature_template` WHERE `entry` = 89;
UPDATE `solo_wow_minion` SET `entry` = 990155, `name` = 'Bone Colossus' WHERE `entry` = 89;
INSERT INTO `solo_wow_minion` SELECT * FROM `creature_template` WHERE `entry` = 11859;
UPDATE `solo_wow_minion` SET `entry` = 990156, `name` = 'Lich Lieutenant' WHERE `entry` = 11859;
INSERT INTO `solo_wow_minion` SELECT * FROM `creature_template` WHERE `entry` = 416;
UPDATE `solo_wow_minion` SET `entry` = 990157, `name` = 'Skeletal Archer' WHERE `entry` = 416;
INSERT INTO `creature_template` SELECT * FROM `solo_wow_minion`;
DROP TEMPORARY TABLE `solo_wow_minion`;

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
SELECT `minion`, 0, `display`, 1, 1 FROM `solo_wow_minion_map`;

-- Auras the demon always carries (the Infernal's Immolation)
DROP TEMPORARY TABLE IF EXISTS `solo_wow_minion_addon`;
CREATE TEMPORARY TABLE `solo_wow_minion_addon` AS
SELECT a.*, m.`minion` FROM `creature_template_addon` a JOIN `solo_wow_minion_map` m ON m.`demon` = a.`entry`;
UPDATE `solo_wow_minion_addon` SET `entry` = `minion`;
ALTER TABLE `solo_wow_minion_addon` DROP COLUMN `minion`;
INSERT INTO `creature_template_addon` SELECT * FROM `solo_wow_minion_addon`;
DROP TEMPORARY TABLE `solo_wow_minion_addon`;

INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`)
SELECT m.`minion`, s.`Index`, s.`Spell`
FROM `creature_template_spell` s JOIN `solo_wow_minion_map` m ON m.`demon` = s.`CreatureID`;

INSERT INTO `creature_template_movement` (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`,
  `InteractionPauseTimer`)
SELECT m.`minion`, v.`Ground`, v.`Swim`, v.`Flight`, v.`Rooted`, v.`Chase`, v.`Random`, v.`InteractionPauseTimer`
FROM `creature_template_movement` v JOIN `solo_wow_minion_map` m ON m.`demon` = v.`CreatureId`;

INSERT INTO `pet_levelstats` (`creature_entry`, `level`, `hp`, `mana`, `armor`, `str`, `agi`, `sta`, `inte`, `spi`,
  `min_dmg`, `max_dmg`)
SELECT m.`minion`, p.`level`, GREATEST(1, p.`hp` * m.`health_pct` DIV 100), p.`mana`, p.`armor`, p.`str`, p.`agi`,
  p.`sta` * m.`health_pct` DIV 100, p.`inte`, p.`spi`, p.`min_dmg`, p.`max_dmg`
FROM `pet_levelstats` p JOIN `solo_wow_minion_map` m ON m.`demon` = p.`creature_entry`;

-- Random names from the death knight ghoul's word list (Corpse, Grave, Blight ...), numbered per minion
INSERT INTO `pet_name_generation` (`id`, `word`, `entry`, `half`)
SELECT m.`minion` * 100 + ROW_NUMBER() OVER (PARTITION BY m.`minion` ORDER BY g.`id`), g.`word`, m.`minion`, g.`half`
FROM `pet_name_generation` g JOIN `solo_wow_minion_map` m ON g.`entry` = 26125;

DROP TEMPORARY TABLE `solo_wow_minion_map`;

-- The Grave Portal: Ritual of the Lich's portal, a copy of the Doom Portal (177193) that casts the Lich Lieutenant
-- summon (990175) instead of the Doomguard's (18541) when the ritual completes
DELETE FROM `gameobject_template` WHERE `entry` = 990176;
DROP TEMPORARY TABLE IF EXISTS `solo_wow_portal`;
CREATE TEMPORARY TABLE `solo_wow_portal` AS SELECT * FROM `gameobject_template` WHERE `entry` = 177193;
UPDATE `solo_wow_portal` SET `entry` = 990176, `name` = 'Grave Portal', `Data1` = 990175;
INSERT INTO `gameobject_template` SELECT * FROM `solo_wow_portal`;
DROP TEMPORARY TABLE `solo_wow_portal`;
DELETE FROM `gameobject_template_addon` WHERE `entry` = 990176;
INSERT INTO `gameobject_template_addon` (`entry`, `faction`, `flags`, `mingold`, `maxgold`)
SELECT 990176, `faction`, `flags`, `mingold`, `maxgold` FROM `gameobject_template_addon` WHERE `entry` = 177193;
