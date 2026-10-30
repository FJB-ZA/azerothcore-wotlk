-- Necromancer warlock path (path 2) beyond its spell data (solo-wow patches/client/spell-data/subclasses/necromancer.json).
-- Its spells mirror the warlock spells they replace: the same numbers, ranks, talents and procs, with frost and plague
-- where the warlock has fire and fear. IDs 990300-990499 are this path's in every table (spells, creatures, NPCs,
-- quests, items and texts). The minions are in the _01 file, the trainers, quests and book in the _02 file.
DELETE FROM `subclass_path` WHERE `id` = 2;
INSERT INTO `subclass_path` (`id`, `name`, `class`, `unlock_level`, `era`, `unlock_quest`, `damage_pve`, `damage_pvp`, `healing_pve`, `healing_pvp`, `talent_tab`, `bot_share`) VALUES
(2, 'Necromancer', 9, 10, 0, 990371, 1, 1, 1, 1, 0, 0.25);

-- Conflagrate and Summon Felguard are talents, which slots can't swap, so the warlock keeps them on this path.
DELETE FROM `subclass_spell_swap` WHERE `path_id` = 2;
INSERT INTO `subclass_spell_swap` (`path_id`, `spell`, `replacement`) VALUES
-- Immolate -> Plague Strike
(2, 348, 990300), (2, 707, 990301), (2, 1094, 990302), (2, 2941, 990303), (2, 11665, 990304), (2, 11667, 990305),
(2, 11668, 990306), (2, 25309, 990307), (2, 27215, 990308), (2, 47810, 990309), (2, 47811, 990310),
-- Searing Pain -> Chilling Touch
(2, 5676, 990311), (2, 17919, 990312), (2, 17920, 990313), (2, 17921, 990314), (2, 17922, 990315), (2, 17923, 990316),
(2, 27210, 990317), (2, 30459, 990318), (2, 47814, 990319), (2, 47815, 990320),
-- Soul Fire -> Soul Frost
(2, 6353, 990321), (2, 17924, 990322), (2, 27211, 990323), (2, 30545, 990324), (2, 47824, 990325), (2, 47825, 990326),
-- Incinerate -> Frost Lance (TBC)
(2, 29722, 990327), (2, 32231, 990328), (2, 47837, 990329), (2, 47838, 990330),
-- Rain of Fire -> Plague Cloud, and the damage spells its channel triggers
(2, 5740, 990331), (2, 6219, 990332), (2, 11677, 990333), (2, 11678, 990334), (2, 27212, 990335), (2, 47819, 990336),
(2, 47820, 990337),
(2, 42223, 990360), (2, 42224, 990361), (2, 42225, 990362), (2, 42226, 990363), (2, 42218, 990364), (2, 47817, 990365),
(2, 47818, 990366),
-- Hellfire -> Death and Decay, and the damage spells its channel triggers
(2, 1949, 990338), (2, 11683, 990339), (2, 11684, 990340), (2, 27213, 990341), (2, 47823, 990342),
(2, 5857, 990367), (2, 11681, 990368), (2, 11682, 990369), (2, 27214, 990370), (2, 47822, 990371),
-- Fear -> Entomb, a 3 second stun
(2, 5782, 990344), (2, 6213, 990345), (2, 6215, 990346),
-- Summons: Imp, Voidwalker, Succubus, Felhunter
(2, 688, 990350), (2, 697, 990351), (2, 712, 990352), (2, 691, 990353),
-- Inferno -> Bone Colossus and its impact. Ritual of Doom -> Ritual of the Lich, the spell its portal casts, and
-- the summon that spell's script casts (Summon Doomguard)
(2, 1122, 990372), (2, 22703, 990373), (2, 18540, 990374), (2, 18541, 990375), (2, 60478, 990376);

-- The Imp's replacement comes as two Skeletal Archers sharing its health and melee.
DELETE FROM `subclass_creature_swap` WHERE `path_id` = 2;
INSERT INTO `subclass_creature_swap` (`path_id`, `entry`, `replacement`, `bodies`) VALUES
(2, 416, 990350, 2),    -- Imp -> Skeletal Archers
(2, 1860, 990351, 1),   -- Voidwalker -> Flesh Golem
(2, 1863, 990352, 1),   -- Succubus -> Ghoul
(2, 417, 990353, 1),    -- Felhunter -> Banshee
(2, 89, 990355, 1),     -- Infernal -> Bone Colossus
(2, 11859, 990356, 1);  -- Doomguard -> Lich Lieutenant

-- A mirror behaves like its original: copy the original's per-spell rows to it. A negative id (every rank of a chain)
-- maps to the negative mirror; the spell pipeline writes the mirrors' own rank chains. Entomb is no Fear, so it takes
-- none of Fear's rows.
DROP TEMPORARY TABLE IF EXISTS `necromancer_mirror`;
CREATE TEMPORARY TABLE `necromancer_mirror` (
  `spell` INT UNSIGNED NOT NULL PRIMARY KEY,
  `replacement` INT UNSIGNED NOT NULL
);
INSERT INTO `necromancer_mirror` (`spell`, `replacement`)
SELECT `spell`, `replacement` FROM `subclass_spell_swap` WHERE `path_id` = 2 AND `spell` NOT IN (5782, 6213, 6215);

DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 990300 AND 990499;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT m.`replacement`, b.`direct_bonus`, b.`dot_bonus`, b.`ap_bonus`, b.`ap_dot_bonus`, CONCAT('Necromancer - mirror of ', b.`entry`)
FROM `spell_bonus_data` b JOIN `necromancer_mirror` m ON m.`spell` = b.`entry`;

DELETE FROM `spell_script_names` WHERE ABS(`spell_id`) BETWEEN 990300 AND 990499;
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
SELECT IF(n.`spell_id` < 0, -CAST(m.`replacement` AS SIGNED), m.`replacement`), n.`ScriptName`
FROM `spell_script_names` n JOIN `necromancer_mirror` m ON m.`spell` = ABS(n.`spell_id`);

DELETE FROM `spell_proc` WHERE ABS(`SpellId`) BETWEEN 990300 AND 990499;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`)
SELECT IF(p.`SpellId` < 0, -CAST(m.`replacement` AS SIGNED), m.`replacement`), p.`SchoolMask`, p.`SpellFamilyName`,
  p.`SpellFamilyMask0`, p.`SpellFamilyMask1`, p.`SpellFamilyMask2`, p.`ProcFlags`, p.`SpellTypeMask`, p.`SpellPhaseMask`,
  p.`HitMask`, p.`AttributesMask`, p.`DisableEffectsMask`, p.`ProcsPerMinute`, p.`Chance`, p.`Cooldown`, p.`Charges`
FROM `spell_proc` p JOIN `necromancer_mirror` m ON m.`spell` = ABS(p.`SpellId`);

DELETE FROM `spell_custom_attr` WHERE `spell_id` BETWEEN 990300 AND 990499;
INSERT INTO `spell_custom_attr` (`spell_id`, `attributes`)
SELECT m.`replacement`, a.`attributes`
FROM `spell_custom_attr` a JOIN `necromancer_mirror` m ON m.`spell` = a.`spell_id`;

DELETE FROM `spell_group` WHERE `spell_id` BETWEEN 990300 AND 990499;
INSERT INTO `spell_group` (`id`, `spell_id`)
SELECT g.`id`, m.`replacement`
FROM `spell_group` g JOIN `necromancer_mirror` m ON m.`spell` = g.`spell_id`;

DROP TEMPORARY TABLE `necromancer_mirror`;
