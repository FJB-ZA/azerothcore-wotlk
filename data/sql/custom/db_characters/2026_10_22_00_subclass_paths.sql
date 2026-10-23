-- solo-wow sub-class paths: the path each spec slot follows. No row means the plain class.
CREATE TABLE IF NOT EXISTS `character_subclass_path` (
  `guid` INT UNSIGNED NOT NULL,
  `talent_group` TINYINT UNSIGNED NOT NULL,
  `path_id` SMALLINT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `talent_group`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow sub-class path per spec slot';

-- The rising path change fee, counted apart from the talent reset fee.
CREATE TABLE IF NOT EXISTS `character_subclass_fee` (
  `guid` INT UNSIGNED NOT NULL,
  `last_fee` INT UNSIGNED NOT NULL DEFAULT 0,
  `last_change` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow sub-class path change fee';
