-- Sub-class paths that take spells away with nothing in their place, give spells of their own, or keep the class's
-- pets in the stable (the Ranger, a hunter without a pet). Named to run after the paths table's creation and before
-- the paths' own files.
ALTER TABLE `subclass_path` ADD COLUMN `pets` TINYINT UNSIGNED NOT NULL DEFAULT 1
  COMMENT '0: the class''s pets stay in the stable while a slot follows the path' AFTER `role`;

-- One row per rank: a spell the path takes away while a slot follows it, and gives back off the path
CREATE TABLE IF NOT EXISTS `subclass_spell_remove` (
  `path_id` SMALLINT UNSIGNED NOT NULL,
  `spell` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`path_id`, `spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow sub-class spells taken away';

-- A spell a slot has only while it follows the path
CREATE TABLE IF NOT EXISTS `subclass_spell_grant` (
  `path_id` SMALLINT UNSIGNED NOT NULL,
  `spell` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`path_id`, `spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow sub-class spells given';
