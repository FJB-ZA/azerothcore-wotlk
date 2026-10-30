-- Necromancer minions (path 2): each is the demon it replaces in an undead body. A copy of the demon's creature row
-- (family, class, scaling, flags and scripts), auras, spells and movement, with an undead model and name. The core
-- gives each the demon's pet stats and scaling (subclass_minions.cpp), so the pet bar, pet AI, pet talents and every
-- number behave as the demon's.
--
--   990350 Skeletal Archer  <- Imp 416         model 9785 (Skeletal Guardian); two bodies share the Imp's health
--   990351 Flesh Golem      <- Voidwalker 1860  model 1693 (Flesh Golem)
--   990352 Ghoul            <- Succubus 1863    model 571 (Ghoul)
--   990353 Banshee          <- Felhunter 417    model 8782 (Wailing Banshee)
--   990355 Bone Colossus    <- Infernal 89      model 12073 (Bone Construct)
--   990356 Lich Lieutenant  <- Doomguard 11859  model 17444 (Lich)
--   990376 Grave Portal     <- Doom Portal 177193 (game object), for Ritual of the Lich

DELETE FROM `creature_template` WHERE `entry` BETWEEN 990350 AND 990356;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 990350 AND 990356;
DELETE FROM `creature_template_addon` WHERE `entry` BETWEEN 990350 AND 990356;
DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 990350 AND 990356;
DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 990350 AND 990356;
DELETE FROM `pet_name_generation` WHERE `entry` BETWEEN 990350 AND 990356;

DROP TEMPORARY TABLE IF EXISTS `necromancer_minion_map`;
CREATE TEMPORARY TABLE `necromancer_minion_map` (
  `demon` INT UNSIGNED NOT NULL,
  `minion` INT UNSIGNED NOT NULL PRIMARY KEY,
  `name` VARCHAR(40) NOT NULL,
  `display` INT UNSIGNED NOT NULL
);
INSERT INTO `necromancer_minion_map` (`demon`, `minion`, `name`, `display`) VALUES
(416, 990350, 'Skeletal Archer', 9785),
(1860, 990351, 'Flesh Golem', 1693),
(1863, 990352, 'Ghoul', 571),
(417, 990353, 'Banshee', 8782),
(89, 990355, 'Bone Colossus', 12073),
(11859, 990356, 'Lich Lieutenant', 17444);

-- creature_template: copy the whole row, then change the entry and name
DROP TEMPORARY TABLE IF EXISTS `necromancer_minion`;
CREATE TEMPORARY TABLE `necromancer_minion` AS
SELECT t.* FROM `creature_template` t JOIN `necromancer_minion_map` m ON m.`demon` = t.`entry`;
UPDATE `necromancer_minion` t JOIN `necromancer_minion_map` m ON m.`demon` = t.`entry`
SET t.`entry` = m.`minion`, t.`name` = m.`name`;
INSERT INTO `creature_template` SELECT * FROM `necromancer_minion`;
DROP TEMPORARY TABLE `necromancer_minion`;

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
SELECT `minion`, 0, `display`, 1, 1 FROM `necromancer_minion_map`;

-- Auras the demon always carries (the Infernal's Immolation)
DROP TEMPORARY TABLE IF EXISTS `necromancer_minion_addon`;
CREATE TEMPORARY TABLE `necromancer_minion_addon` AS
SELECT a.*, m.`minion` FROM `creature_template_addon` a JOIN `necromancer_minion_map` m ON m.`demon` = a.`entry`;
UPDATE `necromancer_minion_addon` SET `entry` = `minion`;
ALTER TABLE `necromancer_minion_addon` DROP COLUMN `minion`;
INSERT INTO `creature_template_addon` SELECT * FROM `necromancer_minion_addon`;
DROP TEMPORARY TABLE `necromancer_minion_addon`;

INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`)
SELECT m.`minion`, s.`Index`, s.`Spell`
FROM `creature_template_spell` s JOIN `necromancer_minion_map` m ON m.`demon` = s.`CreatureID`;

INSERT INTO `creature_template_movement` (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`, `InteractionPauseTimer`)
SELECT m.`minion`, v.`Ground`, v.`Swim`, v.`Flight`, v.`Rooted`, v.`Chase`, v.`Random`, v.`InteractionPauseTimer`
FROM `creature_template_movement` v JOIN `necromancer_minion_map` m ON m.`demon` = v.`CreatureId`;

-- Random names from the death knight ghoul's word list (Corpse, Grave, Blight ...), numbered per minion
INSERT INTO `pet_name_generation` (`id`, `word`, `entry`, `half`)
SELECT m.`minion` * 100 + ROW_NUMBER() OVER (PARTITION BY m.`minion` ORDER BY g.`id`), g.`word`, m.`minion`, g.`half`
FROM `pet_name_generation` g JOIN `necromancer_minion_map` m ON g.`entry` = 26125;

DROP TEMPORARY TABLE `necromancer_minion_map`;

-- The Grave Portal: a copy of the Doom Portal that casts Ritual of the Lich's effect (990375) instead of Ritual of
-- Doom's (18541) when the ritual completes
DELETE FROM `gameobject_template` WHERE `entry` = 990376;
DROP TEMPORARY TABLE IF EXISTS `necromancer_portal`;
CREATE TEMPORARY TABLE `necromancer_portal` AS SELECT * FROM `gameobject_template` WHERE `entry` = 177193;
UPDATE `necromancer_portal` SET `entry` = 990376, `name` = 'Grave Portal', `Data1` = 990375;
INSERT INTO `gameobject_template` SELECT * FROM `necromancer_portal`;
DROP TEMPORARY TABLE `necromancer_portal`;
DELETE FROM `gameobject_template_addon` WHERE `entry` = 990376;
INSERT INTO `gameobject_template_addon` (`entry`, `faction`, `flags`, `mingold`, `maxgold`)
SELECT 990376, `faction`, `flags`, `mingold`, `maxgold` FROM `gameobject_template_addon` WHERE `entry` = 177193;
