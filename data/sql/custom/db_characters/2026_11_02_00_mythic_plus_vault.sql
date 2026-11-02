-- solo-wow Mythic+ (scripts/Custom/MythicPlus/mythic_vault.cpp): retail's Great Vault, rows Raids (1), Dungeons (2)
-- and World (3) of three slots. The Dungeons row counts mythic_plus_runs; the other rows count their activity here.
-- Each unlocked slot's option is rolled once, the week after, and one of them is chosen.
CREATE TABLE IF NOT EXISTS `mythic_plus_vault_activity` (`guid` INT UNSIGNED NOT NULL, `week` INT UNSIGNED NOT NULL,
  `row` TINYINT UNSIGNED NOT NULL, `map` SMALLINT UNSIGNED NOT NULL, `difficulty` TINYINT UNSIGNED NOT NULL,
  `ref` INT UNSIGNED NOT NULL, PRIMARY KEY (`guid`, `week`, `row`, `map`, `difficulty`, `ref`));
CREATE TABLE IF NOT EXISTS `mythic_plus_vault_option` (`guid` INT UNSIGNED NOT NULL, `week` INT UNSIGNED NOT NULL,
  `row` TINYINT UNSIGNED NOT NULL, `slot` TINYINT UNSIGNED NOT NULL, `item` INT UNSIGNED NOT NULL,
  `chosen` TINYINT UNSIGNED NOT NULL DEFAULT 0, PRIMARY KEY (`guid`, `week`, `row`, `slot`));
