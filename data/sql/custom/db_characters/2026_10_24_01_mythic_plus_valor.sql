-- solo-wow Mythic+ (scripts/Custom/MythicPlus): Valorstones and every finished run, for the Mythic+ Dungeons tab.
CREATE TABLE IF NOT EXISTS `mythic_plus_valor` (
  `guid` INT UNSIGNED NOT NULL,
  `amount` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow Mythic+ Valorstones from end caches';

CREATE TABLE IF NOT EXISTS `mythic_plus_runs` (
  `guid` INT UNSIGNED NOT NULL,
  `week` INT UNSIGNED NOT NULL COMMENT 'MythicAffixPolicy.h WeekIndex',
  `completed` INT UNSIGNED NOT NULL COMMENT 'unix time',
  `map` SMALLINT UNSIGNED NOT NULL,
  `level` TINYINT UNSIGNED NOT NULL,
  `seconds` INT UNSIGNED NOT NULL,
  `upgrade` TINYINT UNSIGNED NOT NULL COMMENT '0 over time, 1-3 the key upgrade',
  PRIMARY KEY (`guid`, `completed`, `map`),
  KEY `week` (`guid`, `week`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow Mythic+ finished runs per character';
