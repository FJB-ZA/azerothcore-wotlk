-- The first Necromancer build stored a warlock's path as path 1 and taught its spells at IDs 990100-990199. The
-- Necromancer is path 2 at 990300-990499 now, and path 1 and those IDs are the warrior's Gladiator. This moves a
-- warlock's path rows to path 2 and forgets the old spells and their action buttons; the engine teaches the new ones
-- at login. A warlock never has Gladiator rows, so where that build never ran this changes nothing.
CREATE TABLE IF NOT EXISTS `character_subclass_path` (
  `guid` INT UNSIGNED NOT NULL,
  `talent_group` TINYINT UNSIGNED NOT NULL,
  `path_id` SMALLINT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `talent_group`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow sub-class path per spec slot';

UPDATE `character_subclass_path` p JOIN `characters` c ON c.`guid` = p.`guid`
SET p.`path_id` = 2 WHERE c.`class` = 9 AND p.`path_id` = 1;

DELETE s FROM `character_spell` s JOIN `characters` c ON c.`guid` = s.`guid`
WHERE c.`class` = 9 AND s.`spell` BETWEEN 990100 AND 990199;

DELETE a FROM `character_action` a JOIN `characters` c ON c.`guid` = a.`guid`
WHERE c.`class` = 9 AND a.`type` = 0 AND a.`action` BETWEEN 990100 AND 990199;
