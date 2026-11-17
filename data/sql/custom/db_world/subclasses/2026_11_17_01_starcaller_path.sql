-- Starcaller druid path (path 5): Elune's moonlight where the druid has the storm. Its spells are in solo-wow's
-- patches/client/spell-data/subclasses/starcaller.json; design in solo-wow's
-- docs/superpowers/specs/2026-11-17-subclasses-starcaller-design.md. Spells 990600-990699 (the Ranger's 200-id
-- block's second half), everything else 990650-990699. The trainers, quests and book are in the _02 file.
DELETE FROM `subclass_path` WHERE `id` = 5;
INSERT INTO `subclass_path` (`id`, `name`, `class`, `unlock_level`, `era`, `unlock_quest`, `damage_pve`, `damage_pvp`, `healing_pve`, `healing_pvp`, `talent_tab`, `bot_share`, `role`, `pets`) VALUES
(5, 'Starcaller', 11, 10, 0, 990651, 1, 1, 1, 1, 0, 0.25, 0, 1);

-- Every mirror has the same numbers, ranks and family flags as the druid spell it replaces. Insect Swarm, Force of
-- Nature, Typhoon and Starfall are talents, which slots can't swap, so the druid keeps them on this path; Nature's
-- Grasp keeps its own roots.
DELETE FROM `subclass_spell_swap` WHERE `path_id` = 5;
INSERT INTO `subclass_spell_swap` (`path_id`, `spell`, `replacement`) VALUES
-- Wrath -> Moonbolt
(5, 5176, 990600), (5, 5177, 990601), (5, 5178, 990602), (5, 5179, 990603), (5, 5180, 990604), (5, 6780, 990605),
(5, 8905, 990606), (5, 9912, 990607), (5, 26984, 990608), (5, 26985, 990609), (5, 48459, 990610), (5, 48461, 990611),
-- Hurricane -> Falling Stars, and the damage its channel triggers
(5, 16914, 990620), (5, 17401, 990621), (5, 17402, 990622), (5, 27012, 990623), (5, 48467, 990624),
(5, 42231, 990630), (5, 42232, 990631), (5, 42233, 990632), (5, 42230, 990633), (5, 48466, 990634),
-- Entangling Roots -> Moonlit Bindings
(5, 339, 990640), (5, 1062, 990641), (5, 5195, 990642), (5, 5196, 990643), (5, 9852, 990644), (5, 9853, 990645),
(5, 26989, 990646), (5, 53308, 990647);

-- A mirror behaves like its original: copy the original's per-spell rows to it. A negative id (every rank of a chain)
-- maps to the negative mirror; the spell pipeline writes the mirrors' own rank chains. None of the three has a core
-- script today; the copy keeps it that way if one is added.
DROP TEMPORARY TABLE IF EXISTS `starcaller_mirror`;
CREATE TEMPORARY TABLE `starcaller_mirror` (
  `spell` INT UNSIGNED NOT NULL PRIMARY KEY,
  `replacement` INT UNSIGNED NOT NULL
);
INSERT INTO `starcaller_mirror` (`spell`, `replacement`)
SELECT `spell`, `replacement` FROM `subclass_spell_swap` WHERE `path_id` = 5;

DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 990600 AND 990699;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT m.`replacement`, b.`direct_bonus`, b.`dot_bonus`, b.`ap_bonus`, b.`ap_dot_bonus`, CONCAT('Starcaller - mirror of ', b.`entry`)
FROM `spell_bonus_data` b JOIN `starcaller_mirror` m ON m.`spell` = b.`entry`;

DELETE FROM `spell_script_names` WHERE ABS(`spell_id`) BETWEEN 990600 AND 990699;
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
SELECT IF(n.`spell_id` < 0, -CAST(m.`replacement` AS SIGNED), m.`replacement`), n.`ScriptName`
FROM `spell_script_names` n JOIN `starcaller_mirror` m ON m.`spell` = ABS(n.`spell_id`);

DELETE FROM `spell_proc` WHERE ABS(`SpellId`) BETWEEN 990600 AND 990699;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`)
SELECT IF(p.`SpellId` < 0, -CAST(m.`replacement` AS SIGNED), m.`replacement`), p.`SchoolMask`, p.`SpellFamilyName`,
  p.`SpellFamilyMask0`, p.`SpellFamilyMask1`, p.`SpellFamilyMask2`, p.`ProcFlags`, p.`SpellTypeMask`, p.`SpellPhaseMask`,
  p.`HitMask`, p.`AttributesMask`, p.`DisableEffectsMask`, p.`ProcsPerMinute`, p.`Chance`, p.`Cooldown`, p.`Charges`
FROM `spell_proc` p JOIN `starcaller_mirror` m ON m.`spell` = ABS(p.`SpellId`);

DELETE FROM `spell_custom_attr` WHERE `spell_id` BETWEEN 990600 AND 990699;
INSERT INTO `spell_custom_attr` (`spell_id`, `attributes`)
SELECT m.`replacement`, a.`attributes`
FROM `spell_custom_attr` a JOIN `starcaller_mirror` m ON m.`spell` = a.`spell_id`;

-- Hurricane is in a group with the other attack-speed slows; Falling Stars joins it
DELETE FROM `spell_group` WHERE `spell_id` BETWEEN 990600 AND 990699;
INSERT INTO `spell_group` (`id`, `spell_id`)
SELECT g.`id`, m.`replacement`
FROM `spell_group` g JOIN `starcaller_mirror` m ON m.`spell` = g.`spell_id`;

DROP TEMPORARY TABLE `starcaller_mirror`;
