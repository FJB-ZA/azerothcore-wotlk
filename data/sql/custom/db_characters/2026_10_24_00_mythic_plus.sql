-- solo-wow Mythic+ (scripts/Custom/MythicPlus): keystones, crests, weekly progress and best runs per character.
CREATE TABLE IF NOT EXISTS `mythic_plus_keystone` (
  `guid` INT UNSIGNED NOT NULL,
  `map` SMALLINT UNSIGNED NOT NULL,
  `level` TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow Mythic+ keystone held by each character';

CREATE TABLE IF NOT EXISTS `mythic_plus_crests` (
  `guid` INT UNSIGNED NOT NULL,
  `tier` TINYINT UNSIGNED NOT NULL COMMENT '1 Cobalt, 2 Saronite, 3 Titanium, 4 Titansteel',
  `amount` INT UNSIGNED NOT NULL DEFAULT 0,
  `week` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'MythicAffixPolicy.h WeekIndex of earned_week',
  `earned_week` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'earned during that week, against the weekly cap',
  PRIMARY KEY (`guid`, `tier`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow Mythic+ crests';

CREATE TABLE IF NOT EXISTS `mythic_plus_week` (
  `guid` INT UNSIGNED NOT NULL,
  `week` INT UNSIGNED NOT NULL,
  `runs` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'completed runs, timed or not',
  `best_level` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `vault_claimed` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'the vault for this week was opened the week after',
  PRIMARY KEY (`guid`, `week`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow Mythic+ weekly runs, for the Dalaran vault';

CREATE TABLE IF NOT EXISTS `mythic_plus_week_maps` (
  `guid` INT UNSIGNED NOT NULL,
  `week` INT UNSIGNED NOT NULL,
  `map` SMALLINT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `week`, `map`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow Mythic+ dungeons completed each week; the vault offers their loot';

CREATE TABLE IF NOT EXISTS `mythic_plus_best` (
  `guid` INT UNSIGNED NOT NULL,
  `map` SMALLINT UNSIGNED NOT NULL,
  `level` TINYINT UNSIGNED NOT NULL,
  `seconds` INT UNSIGNED NOT NULL,
  `upgrade` TINYINT UNSIGNED NOT NULL COMMENT '0 over time, 1-3 the key upgrade',
  `completed` INT UNSIGNED NOT NULL COMMENT 'unix time',
  PRIMARY KEY (`guid`, `map`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow Mythic+ best run per character and dungeon';
