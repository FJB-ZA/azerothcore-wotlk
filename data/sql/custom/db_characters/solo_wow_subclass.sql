-- solo-wow sub-class paths (src/server/scripts/Custom/subclass.cpp). Re-applicable.
-- The path each spec slot (talent group 0-3) is on; no row means the plain class.
CREATE TABLE IF NOT EXISTS `character_subclass_path` (
  `guid` INT UNSIGNED NOT NULL,
  `talent_group` TINYINT UNSIGNED NOT NULL,
  `path_id` TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `talent_group`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- The path-change fee's own counter, on the talent reset's schedule.
CREATE TABLE IF NOT EXISTS `character_subclass_fee` (
  `guid` INT UNSIGNED NOT NULL,
  `last_fee` INT UNSIGNED NOT NULL DEFAULT 0,
  `last_change` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
