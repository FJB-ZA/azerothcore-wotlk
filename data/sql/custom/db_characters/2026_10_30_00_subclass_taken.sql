-- solo-wow sub-class paths: originals a path's talent took out of a spec slot (Battle Stance for Gladiator Stance).
-- Only these come back when the slot leaves the path or loses the talent, so a spell it never trained isn't given.
CREATE TABLE IF NOT EXISTS `character_subclass_taken` (
  `guid` INT UNSIGNED NOT NULL,
  `talent_group` TINYINT UNSIGNED NOT NULL,
  `spell` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `talent_group`, `spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow originals a sub-class path talent took';

-- Gladiator slots that already traded Battle Stance (2457) or Shield Block (2565) for their talents (990100, 990101)
-- before this table: they are owed the original back, as the old rule would have given it.
INSERT IGNORE INTO `character_subclass_taken` (`guid`, `talent_group`, `spell`)
SELECT `p`.`guid`, `p`.`talent_group`, `s`.`original`
FROM `character_subclass_path` AS `p`
JOIN (SELECT 990100 AS `talent`, 2457 AS `original` UNION ALL SELECT 990101, 2565) AS `s`
JOIN `character_talent` AS `t` ON `t`.`guid` = `p`.`guid` AND `t`.`spell` = `s`.`talent`
  AND (`t`.`specMask` & (1 << `p`.`talent_group`)) != 0
LEFT JOIN `character_spell` AS `c` ON `c`.`guid` = `p`.`guid` AND `c`.`spell` = `s`.`original`
  AND (`c`.`specMask` & (1 << `p`.`talent_group`)) != 0
WHERE `p`.`path_id` = 1 AND `c`.`guid` IS NULL;
