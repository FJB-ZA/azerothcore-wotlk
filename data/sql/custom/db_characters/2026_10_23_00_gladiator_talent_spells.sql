-- Gladiator Stance and Shield Charge became talents: whoever the path gave them gets the originals back.
INSERT INTO `character_spell` (`guid`, `spell`, `specMask`)
SELECT `s`.`guid`, 2457, `s`.`specMask` FROM (SELECT `guid`, `specMask` FROM `character_spell` WHERE `spell` = 990100) AS `s`
ON DUPLICATE KEY UPDATE `specMask` = `character_spell`.`specMask` | `s`.`specMask`;
INSERT INTO `character_spell` (`guid`, `spell`, `specMask`)
SELECT `s`.`guid`, 2565, `s`.`specMask` FROM (SELECT `guid`, `specMask` FROM `character_spell` WHERE `spell` = 990101) AS `s`
ON DUPLICATE KEY UPDATE `specMask` = `character_spell`.`specMask` | `s`.`specMask`;
DELETE FROM `character_spell` WHERE `spell` IN (990100, 990101);
