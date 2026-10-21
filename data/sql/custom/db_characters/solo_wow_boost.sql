-- Character boosts (solo-wow): one row per account and era that has used its boost.
-- Written by src/server/scripts/Custom/boost.cpp.
CREATE TABLE IF NOT EXISTS `solo_wow_boost` (
  `account` INT UNSIGNED NOT NULL,
  `era` TINYINT UNSIGNED NOT NULL COMMENT '0 vanilla, 1 TBC, 2 Wrath',
  `guid` INT UNSIGNED NOT NULL COMMENT 'the boosted character',
  `level` TINYINT UNSIGNED NOT NULL,
  `spec` VARCHAR(32) NOT NULL,
  `boosted_at` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`account`, `era`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow character boosts';
