-- solo-wow spell charges (scripts/Custom/spell_charges.cpp). Each path adds its rows with the path itself.
CREATE TABLE IF NOT EXISTS `solo_wow_spell_charges` (
  `spell` INT UNSIGNED NOT NULL COMMENT 'its DBC RecoveryTime and CategoryRecoveryTime must be 0',
  `max_charges` TINYINT UNSIGNED NOT NULL,
  `recharge_ms` INT UNSIGNED NOT NULL COMMENT 'one charge; cooldown spell mods apply',
  PRIMARY KEY (`spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='solo-wow spells with charges';
