-- solo-wow sub-class paths (src/server/scripts/Custom/subclass.cpp). Re-applicable: rerun on every change.
-- Paths are data. Each path swaps spells (one row per rank) and pets for mirrors with the same numbers; the mirror
-- spells themselves come from the spell-data pipeline (solo-wow patches/client/spell-data, .\wow.ps1 spells).

CREATE TABLE IF NOT EXISTS `subclass_path` (
  `id` TINYINT UNSIGNED NOT NULL,
  `name` VARCHAR(40) NOT NULL,
  `base_class` TINYINT UNSIGNED NOT NULL,
  `unlock_level` TINYINT UNSIGNED NOT NULL DEFAULT 10,
  `unlock_expansion` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 vanilla, 1 TBC, 2 Wrath',
  `bot_share_pct` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'share of new random bots of the class that take it',
  `pve_damage_mod` FLOAT NOT NULL DEFAULT 1,
  `pve_healing_mod` FLOAT NOT NULL DEFAULT 1,
  `pvp_damage_mod` FLOAT NOT NULL DEFAULT 1,
  `pvp_healing_mod` FLOAT NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `subclass_spell_swap` (
  `path_id` TINYINT UNSIGNED NOT NULL,
  `original_spell` INT UNSIGNED NOT NULL,
  `replacement_spell` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`path_id`, `original_spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `subclass_creature_swap` (
  `path_id` TINYINT UNSIGNED NOT NULL,
  `original_entry` INT UNSIGNED NOT NULL,
  `replacement_entry` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`path_id`, `original_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Path 1: the warlock's Necromancer (docs/superpowers/specs/2026-10-21-subclasses-necromancer-design.md in solo-wow).
DELETE FROM `subclass_path` WHERE `id` = 1;
INSERT INTO `subclass_path` (`id`, `name`, `base_class`, `unlock_level`, `unlock_expansion`, `bot_share_pct`) VALUES
(1, 'Necromancer', 9, 10, 0, 25);

DELETE FROM `subclass_spell_swap` WHERE `path_id` = 1;
INSERT INTO `subclass_spell_swap` (`path_id`, `original_spell`, `replacement_spell`) VALUES
-- Immolate -> Plague Strike (11 ranks)
(1, 348, 990100), (1, 707, 990101), (1, 1094, 990102), (1, 2941, 990103), (1, 11665, 990104), (1, 11667, 990105),
(1, 11668, 990106), (1, 25309, 990107), (1, 27215, 990108), (1, 47810, 990109), (1, 47811, 990110),
-- Searing Pain -> Chilling Touch (10 ranks)
(1, 5676, 990111), (1, 17919, 990112), (1, 17920, 990113), (1, 17921, 990114), (1, 17922, 990115), (1, 17923, 990116),
(1, 27210, 990117), (1, 30459, 990118), (1, 47814, 990119), (1, 47815, 990120),
-- Soul Fire -> Soul Frost (6 ranks)
(1, 6353, 990121), (1, 17924, 990122), (1, 27211, 990123), (1, 30545, 990124), (1, 47824, 990125), (1, 47825, 990126),
-- Incinerate -> Frost Lance (4 ranks, TBC)
(1, 29722, 990127), (1, 32231, 990128), (1, 47837, 990129), (1, 47838, 990130),
-- Rain of Fire -> Plague Cloud (7 ranks), and the damage spells its channel triggers
(1, 5740, 990131), (1, 6219, 990132), (1, 11677, 990133), (1, 11678, 990134), (1, 27212, 990135), (1, 47819, 990136),
(1, 47820, 990137),
(1, 42223, 990160), (1, 42224, 990161), (1, 42225, 990162), (1, 42226, 990163), (1, 42218, 990164), (1, 47817, 990165),
(1, 47818, 990166),
-- Hellfire -> Death and Decay (5 ranks), and the damage spells its channel triggers
(1, 1949, 990138), (1, 11683, 990139), (1, 11684, 990140), (1, 27213, 990141), (1, 47823, 990142),
(1, 5857, 990167), (1, 11681, 990168), (1, 11682, 990169), (1, 27214, 990170), (1, 47822, 990171),
-- Conflagrate -> Shatter Bones (talent)
(1, 17962, 990143),
-- Fear (3 ranks) -> Entomb (one rank)
(1, 5782, 990144), (1, 6213, 990144), (1, 6215, 990144),
-- Summons: Imp, Voidwalker, Succubus, Felhunter, Felguard (TBC talent)
(1, 688, 990150), (1, 697, 990151), (1, 712, 990152), (1, 691, 990153), (1, 30146, 990154);

DELETE FROM `subclass_creature_swap` WHERE `path_id` = 1;
INSERT INTO `subclass_creature_swap` (`path_id`, `original_entry`, `replacement_entry`) VALUES
(1, 416, 990150),    -- Imp -> Skeletal Archer
(1, 1860, 990151),   -- Voidwalker -> Flesh Golem
(1, 1863, 990152),   -- Succubus -> Ghoul
(1, 417, 990153),    -- Felhunter -> Banshee
(1, 17252, 990154);  -- Felguard -> Abomination

-- Mirrors behave like their originals: copy every per-spell table row from each original to its mirror. A negative
-- id (all ranks of a chain) maps to the negative mirror. Fear's ranks share Entomb, which is not a mirror, so
-- one-to-one swaps only.
DROP TEMPORARY TABLE IF EXISTS `solo_wow_mirror`;
CREATE TEMPORARY TABLE `solo_wow_mirror` (
  `original_spell` INT UNSIGNED NOT NULL PRIMARY KEY,
  `replacement_spell` INT UNSIGNED NOT NULL
);
INSERT INTO `solo_wow_mirror` (`original_spell`, `replacement_spell`)
SELECT s.`original_spell`, s.`replacement_spell`
FROM `subclass_spell_swap` s
JOIN (SELECT `path_id`, `replacement_spell` FROM `subclass_spell_swap` GROUP BY `path_id`, `replacement_spell`
      HAVING COUNT(*) = 1) single
  ON single.`path_id` = s.`path_id` AND single.`replacement_spell` = s.`replacement_spell`;

DELETE FROM `spell_ranks` WHERE `spell_id` BETWEEN 990100 AND 990999;
INSERT INTO `spell_ranks` (`first_spell_id`, `spell_id`, `rank`)
SELECT f.`replacement_spell`, m.`replacement_spell`, r.`rank`
FROM `spell_ranks` r
JOIN `solo_wow_mirror` m ON m.`original_spell` = r.`spell_id`
JOIN `solo_wow_mirror` f ON f.`original_spell` = r.`first_spell_id`;

DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 990100 AND 990999;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT m.`replacement_spell`, b.`direct_bonus`, b.`dot_bonus`, b.`ap_bonus`, b.`ap_dot_bonus`,
  CONCAT('solo-wow mirror of ', b.`entry`)
FROM `spell_bonus_data` b JOIN `solo_wow_mirror` m ON m.`original_spell` = b.`entry`;

DELETE FROM `spell_script_names` WHERE ABS(`spell_id`) BETWEEN 990100 AND 990999;
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
SELECT IF(n.`spell_id` < 0, -CAST(m.`replacement_spell` AS SIGNED), m.`replacement_spell`), n.`ScriptName`
FROM `spell_script_names` n JOIN `solo_wow_mirror` m ON m.`original_spell` = ABS(n.`spell_id`);

DELETE FROM `spell_proc` WHERE ABS(`SpellId`) BETWEEN 990100 AND 990999;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
  `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`,
  `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`)
SELECT IF(p.`SpellId` < 0, -CAST(m.`replacement_spell` AS SIGNED), m.`replacement_spell`), p.`SchoolMask`,
  p.`SpellFamilyName`, p.`SpellFamilyMask0`, p.`SpellFamilyMask1`, p.`SpellFamilyMask2`, p.`ProcFlags`,
  p.`SpellTypeMask`,
  p.`SpellPhaseMask`, p.`HitMask`, p.`AttributesMask`, p.`DisableEffectsMask`, p.`ProcsPerMinute`, p.`Chance`,
  p.`Cooldown`, p.`Charges`
FROM `spell_proc` p JOIN `solo_wow_mirror` m ON m.`original_spell` = ABS(p.`SpellId`);

DELETE FROM `spell_custom_attr` WHERE `spell_id` BETWEEN 990100 AND 990999;
INSERT INTO `spell_custom_attr` (`spell_id`, `attributes`)
SELECT m.`replacement_spell`, a.`attributes`
FROM `spell_custom_attr` a JOIN `solo_wow_mirror` m ON m.`original_spell` = a.`spell_id`;

DELETE FROM `spell_group` WHERE `spell_id` BETWEEN 990100 AND 990999;
INSERT INTO `spell_group` (`id`, `spell_id`)
SELECT g.`id`, m.`replacement_spell`
FROM `spell_group` g JOIN `solo_wow_mirror` m ON m.`original_spell` = g.`spell_id`;

DROP TEMPORARY TABLE `solo_wow_mirror`;
