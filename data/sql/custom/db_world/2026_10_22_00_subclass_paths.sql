-- solo-wow sub-class paths. Each path's rows are added with the path itself.
CREATE TABLE IF NOT EXISTS `subclass_path` (
  `id` SMALLINT UNSIGNED NOT NULL,
  `name` VARCHAR(50) NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `unlock_level` TINYINT UNSIGNED NOT NULL DEFAULT 10,
  `era` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 vanilla, 1 TBC, 2 Wrath',
  `unlock_quest` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rewarded quest needed before choosing the path, 0 for none',
  `damage_pve` FLOAT NOT NULL DEFAULT 1 COMMENT 'damage done by the character and its minions to creatures',
  `damage_pvp` FLOAT NOT NULL DEFAULT 1 COMMENT 'damage done to anything a player controls',
  `healing_pve` FLOAT NOT NULL DEFAULT 1 COMMENT 'healing done outside battlegrounds and arenas',
  `healing_pvp` FLOAT NOT NULL DEFAULT 1 COMMENT 'healing done inside battlegrounds and arenas',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow sub-class paths';

-- One row per rank: the path's replacement for an original class spell. Replacement ranks need their own spell_ranks
-- chain, so a new rank supersedes the old one like the originals do.
CREATE TABLE IF NOT EXISTS `subclass_spell_swap` (
  `path_id` SMALLINT UNSIGNED NOT NULL,
  `spell` INT UNSIGNED NOT NULL,
  `replacement` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`path_id`, `spell`),
  UNIQUE KEY `replacement` (`replacement`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow sub-class spell swaps';

-- A summon spell swap's creature: `replacement` is what the replacement spell summons, standing in for `entry`.
-- With bodies > 1 the pet is joined by bodies - 1 guardians of the same entry, all sharing the original's health and
-- damage.
CREATE TABLE IF NOT EXISTS `subclass_creature_swap` (
  `path_id` SMALLINT UNSIGNED NOT NULL,
  `entry` INT UNSIGNED NOT NULL,
  `replacement` INT UNSIGNED NOT NULL,
  `bodies` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`path_id`, `entry`),
  UNIQUE KEY `replacement` (`replacement`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow sub-class creature swaps';
