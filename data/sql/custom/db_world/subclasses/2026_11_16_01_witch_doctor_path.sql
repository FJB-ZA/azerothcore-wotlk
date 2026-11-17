-- Witch Doctor shaman path (path 4): troll voodoo where the shaman has fire. Its spells are in solo-wow's
-- patches/client/spell-data/subclasses/witch-doctor.json; design in solo-wow's
-- docs/superpowers/specs/2026-11-16-subclasses-witch-doctor-design.md. IDs 990700-990899 are this path's in every
-- table (spells, creatures, NPCs, quests, items and texts). The trainers, quests and book are in the _02 file.
DELETE FROM `subclass_path` WHERE `id` = 4;
INSERT INTO `subclass_path` (`id`, `name`, `class`, `unlock_level`, `era`, `unlock_quest`, `damage_pve`, `damage_pvp`, `healing_pve`, `healing_pvp`, `talent_tab`, `bot_share`, `role`, `pets`) VALUES
(4, 'Witch Doctor', 7, 10, 0, 990801, 1, 1, 1, 1, 0, 0.25, 0, 1);

-- Every mirror has the same numbers, ranks and family flags as the shaman spell it replaces. Lava Lash and Totem of
-- Wrath are talents, which slots can't swap, so the shaman keeps them on this path.
DELETE FROM `subclass_spell_swap` WHERE `path_id` = 4;
INSERT INTO `subclass_spell_swap` (`path_id`, `spell`, `replacement`) VALUES
-- Flame Shock -> Voodoo Shock
(4, 8050, 990700), (4, 8052, 990701), (4, 8053, 990702), (4, 10447, 990703), (4, 10448, 990704), (4, 29228, 990705),
(4, 25457, 990706), (4, 49232, 990707), (4, 49233, 990708),
-- Fire Nova -> Spirit Nova, and the damage the fire totem casts
(4, 1535, 990710), (4, 8498, 990711), (4, 8499, 990712), (4, 11314, 990713), (4, 11315, 990714), (4, 25546, 990715),
(4, 25547, 990716), (4, 61649, 990717), (4, 61657, 990718),
(4, 8349, 990720), (4, 8502, 990721), (4, 8503, 990722), (4, 11306, 990723), (4, 11307, 990724), (4, 25535, 990725),
(4, 25537, 990726), (4, 61650, 990727), (4, 61654, 990728),
-- Lava Burst -> Bad Mojo
(4, 51505, 990730), (4, 60043, 990731),
-- Searing Totem -> Serpent Ward, and its bolt (Venom Spit)
(4, 3599, 990740), (4, 6363, 990741), (4, 6364, 990742), (4, 6365, 990743), (4, 10437, 990744), (4, 10438, 990745),
(4, 25533, 990746), (4, 58699, 990747), (4, 58703, 990748), (4, 58704, 990749),
(4, 3606, 990750), (4, 6350, 990751), (4, 6351, 990752), (4, 6352, 990753), (4, 10435, 990754), (4, 10436, 990755),
(4, 25530, 990756), (4, 58700, 990757), (4, 58701, 990758), (4, 58702, 990759),
-- Magma Totem -> Voodoo Ward, its passive and the pulse the passive triggers
(4, 8190, 990760), (4, 10585, 990761), (4, 10586, 990762), (4, 10587, 990763), (4, 25552, 990764), (4, 58731, 990765),
(4, 58734, 990766),
(4, 8188, 990770), (4, 10582, 990771), (4, 10583, 990772), (4, 10584, 990773), (4, 25551, 990774), (4, 58733, 990775),
(4, 58736, 990776),
(4, 8187, 990780), (4, 10579, 990781), (4, 10580, 990782), (4, 10581, 990783), (4, 25550, 990784), (4, 58732, 990785),
(4, 58735, 990786);

-- The wards: each rank's totem in a troll ward's body. A copy of the totem's creature row (totem class, slot, health,
-- flags), with the ward's model and name, casting the ward's mirror of the totem's spell.
--
--   990740-990749 Serpent Ward <- Searing Totem 2523 to 31165, model 15291 (Jin'do's Brain Wash Totem)
--   990760-990766 Voodoo Ward  <- Magma Totem 5929 to 31167,   model 2030 (the trolls' Healing Ward)
DROP TEMPORARY TABLE IF EXISTS `witch_doctor_ward_map`;
CREATE TEMPORARY TABLE `witch_doctor_ward_map` (
  `totem` INT UNSIGNED NOT NULL,
  `ward` INT UNSIGNED NOT NULL PRIMARY KEY,
  `name` VARCHAR(40) NOT NULL,
  `display` INT UNSIGNED NOT NULL
);
INSERT INTO `witch_doctor_ward_map` (`totem`, `ward`, `name`, `display`) VALUES
(2523, 990740, 'Serpent Ward', 15291), (3902, 990741, 'Serpent Ward II', 15291), (3903, 990742, 'Serpent Ward III', 15291),
(3904, 990743, 'Serpent Ward IV', 15291), (7400, 990744, 'Serpent Ward V', 15291), (7402, 990745, 'Serpent Ward VI', 15291),
(15480, 990746, 'Serpent Ward VII', 15291), (31162, 990747, 'Serpent Ward VIII', 15291),
(31164, 990748, 'Serpent Ward IX', 15291), (31165, 990749, 'Serpent Ward X', 15291),
(5929, 990760, 'Voodoo Ward', 2030), (7464, 990761, 'Voodoo Ward II', 2030), (7465, 990762, 'Voodoo Ward III', 2030),
(7466, 990763, 'Voodoo Ward IV', 2030), (15484, 990764, 'Voodoo Ward V', 2030), (31166, 990765, 'Voodoo Ward VI', 2030),
(31167, 990766, 'Voodoo Ward VII', 2030);

DELETE FROM `creature_template` WHERE `entry` BETWEEN 990740 AND 990766;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 990740 AND 990766;
DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 990740 AND 990766;

DROP TEMPORARY TABLE IF EXISTS `witch_doctor_ward`;
CREATE TEMPORARY TABLE `witch_doctor_ward` AS
SELECT t.* FROM `creature_template` t JOIN `witch_doctor_ward_map` m ON m.`totem` = t.`entry`;
UPDATE `witch_doctor_ward` t JOIN `witch_doctor_ward_map` m ON m.`totem` = t.`entry`
SET t.`entry` = m.`ward`, t.`name` = m.`name`;
INSERT INTO `creature_template` SELECT * FROM `witch_doctor_ward`;
DROP TEMPORARY TABLE `witch_doctor_ward`;

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
SELECT `ward`, 0, `display`, 1, 1 FROM `witch_doctor_ward_map`;

INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`, `VerifiedBuild`)
SELECT m.`ward`, s.`Index`, COALESCE(w.`replacement`, s.`Spell`), s.`VerifiedBuild`
FROM `creature_template_spell` s
JOIN `witch_doctor_ward_map` m ON m.`totem` = s.`CreatureID`
LEFT JOIN `subclass_spell_swap` w ON w.`path_id` = 4 AND w.`spell` = s.`Spell`;

-- The core gives a player's totem its race's model as it enters the world; Subclasses/WitchDoctor puts the ward's
-- own model back for every creature named here.
DELETE FROM `subclass_creature_swap` WHERE `path_id` = 4;
INSERT INTO `subclass_creature_swap` (`path_id`, `entry`, `replacement`, `bodies`)
SELECT 4, `totem`, `ward`, 1 FROM `witch_doctor_ward_map`;

DROP TEMPORARY TABLE `witch_doctor_ward_map`;

-- A mirror behaves like its original: copy the original's per-spell rows to it. A negative id (every rank of a chain)
-- maps to the negative mirror; the spell pipeline writes the mirrors' own rank chains. Fire Nova's script only takes
-- Fire Nova's ranks, so Spirit Nova has its own (spell_witch_doctor_spirit_nova).
DROP TEMPORARY TABLE IF EXISTS `witch_doctor_mirror`;
CREATE TEMPORARY TABLE `witch_doctor_mirror` (
  `spell` INT UNSIGNED NOT NULL PRIMARY KEY,
  `replacement` INT UNSIGNED NOT NULL
);
INSERT INTO `witch_doctor_mirror` (`spell`, `replacement`)
SELECT `spell`, `replacement` FROM `subclass_spell_swap` WHERE `path_id` = 4;

DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 990700 AND 990899;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT m.`replacement`, b.`direct_bonus`, b.`dot_bonus`, b.`ap_bonus`, b.`ap_dot_bonus`, CONCAT('Witch Doctor - mirror of ', b.`entry`)
FROM `spell_bonus_data` b JOIN `witch_doctor_mirror` m ON m.`spell` = b.`entry`;

DELETE FROM `spell_script_names` WHERE ABS(`spell_id`) BETWEEN 990700 AND 990899;
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
SELECT IF(n.`spell_id` < 0, -CAST(m.`replacement` AS SIGNED), m.`replacement`), n.`ScriptName`
FROM `spell_script_names` n JOIN `witch_doctor_mirror` m ON m.`spell` = ABS(n.`spell_id`)
WHERE n.`ScriptName` <> 'spell_sha_fire_nova';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (-990710, 'spell_witch_doctor_spirit_nova');

DELETE FROM `spell_proc` WHERE ABS(`SpellId`) BETWEEN 990700 AND 990899;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`)
SELECT IF(p.`SpellId` < 0, -CAST(m.`replacement` AS SIGNED), m.`replacement`), p.`SchoolMask`, p.`SpellFamilyName`,
  p.`SpellFamilyMask0`, p.`SpellFamilyMask1`, p.`SpellFamilyMask2`, p.`ProcFlags`, p.`SpellTypeMask`, p.`SpellPhaseMask`,
  p.`HitMask`, p.`AttributesMask`, p.`DisableEffectsMask`, p.`ProcsPerMinute`, p.`Chance`, p.`Cooldown`, p.`Charges`
FROM `spell_proc` p JOIN `witch_doctor_mirror` m ON m.`spell` = ABS(p.`SpellId`);

DELETE FROM `spell_custom_attr` WHERE `spell_id` BETWEEN 990700 AND 990899;
INSERT INTO `spell_custom_attr` (`spell_id`, `attributes`)
SELECT m.`replacement`, a.`attributes`
FROM `spell_custom_attr` a JOIN `witch_doctor_mirror` m ON m.`spell` = a.`spell_id`;

DELETE FROM `spell_group` WHERE `spell_id` BETWEEN 990700 AND 990899;
INSERT INTO `spell_group` (`id`, `spell_id`)
SELECT g.`id`, m.`replacement`
FROM `spell_group` g JOIN `witch_doctor_mirror` m ON m.`spell` = g.`spell_id`;

DROP TEMPORARY TABLE `witch_doctor_mirror`;
