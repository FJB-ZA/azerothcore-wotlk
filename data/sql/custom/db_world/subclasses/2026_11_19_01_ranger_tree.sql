-- Windrunner, the Ranger path's own talent tree (path 3, talent tab 992), beyond its spell data (solo-wow
-- patches/client/spell-data/subclasses/ranger-tree.json; design in solo-wow's
-- docs/superpowers/specs/2026-11-19-subclasses-ranger-tree-design.md). Spells 993000-993999 are the tree's. The talents
-- that need more than spell data are in src/server/scripts/Custom/Subclasses/Ranger/ranger_windrunner.cpp.

-- The tree opens on the path. The core counts every hunter tree as the caster role (ranged), so Windrunner does too.
UPDATE `subclass_path` SET `talent_tab` = 992, `role` = 3 WHERE `id` = 3;

-- Advantage, the hidden passive that builds the stacks, comes with the path (beside Ranger's Focus); it builds
-- nothing until the Ranger has learned Precision Shot (ranger_windrunner.cpp).
DELETE FROM `subclass_spell_grant` WHERE `path_id` = 3 AND `spell` = 993011;
INSERT INTO `subclass_spell_grant` (`path_id`, `spell`) VALUES (3, 993011);

-- Ranged attack power per hit, per tick for Serrated Shot. Set against Arcane Shot's 15% and Serpent Sting's 20% over
-- its five ticks; the balance harness tunes them (solo-wow's BALANCE.md). The talents' bleed, the Marked Quarry arrow
-- and Kindling's splash take no bonus (they repeat or scale damage that had its bonuses already).
DELETE FROM `spell_bonus_data` WHERE `entry` IN (993000, 993001, 993003, 993004, 993005, 993006, 993007, 993014, 993015, 993020);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(993000, 0, 0, 0.1, 0, 'Ranger - Precision Shot'),
(993001, 0, 0, 0, 0.04, 'Ranger - Serrated Shot, per tick'),
(993003, 0, 0, 0.2, 0, 'Ranger - Final Arrow'),
(993004, 0, 0, 0, 0, 'Ranger - Ricochet Shot (weapon damage)'),
(993005, 0, 0, 0.2, 0, 'Ranger - Hushing Arrow'),
(993006, 0, 0, 0.25, 0, 'Ranger - Incendiary Shot'),
(993007, 0, 0, 0.2, 0, 'Ranger - Piercing Volley'),
(993014, 0, 0, 0, 0, 'Ranger - Marked Quarry arrow (weapon damage)'),
(993015, 0, 0, 0, 0, 'Ranger - Barbed Tips bleed'),
(993020, 0, 0, 0, 0, 'Ranger - Kindling splash');

-- Piercing Volley: a narrow line (it ignored armor too until 2026_11_19_02)
DELETE FROM `spell_cone` WHERE `ID` = 993007;
INSERT INTO `spell_cone` (`ID`, `ConeDegrees`) VALUES (993007, 10);
DELETE FROM `spell_custom_attr` WHERE `spell_id` = 993007;
INSERT INTO `spell_custom_attr` (`spell_id`, `attributes`) VALUES (993007, 0x00008000);

-- Rending Shot sits with Blood Frenzy and Savage Combat: only the strongest physical damage taken debuff counts.
DELETE FROM `spell_group` WHERE `id` = 1108 AND `spell_id` = 993012;
INSERT INTO `spell_group` (`id`, `spell_id`) VALUES (1108, 993012);

-- Procs. Advantage: Arcane Shot (word 1 0x800), Multi-Shot (0x1000), Steady Shot (word 2 0x1), and Serrated Shot,
-- Final Arrow and Ricochet Shot (word 3 0x400000, 0x1000000, 0x2000000), on landing, damage or not. Cruel Aim: ranged
-- ability crits. Barbed Tips: Aimed Shot (word 1 0x20000) and Precision Shot (word 3 0x200000) crits. Telling Blow:
-- ranged physical damage, auto or ability, at the spell's own 8%.
DELETE FROM `spell_proc` WHERE `SpellId` IN (993011, -993139, 993141, 993146);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(993011, 0, 9, 0x1800, 0x1, 0x3400000, 0x100, 0x5, 0x2, 0, 0, 0, 0, 100, 0, 0),
(-993139, 0, 9, 0, 0, 0, 0x100, 0x1, 0x2, 0x2, 0, 0, 0, 100, 0, 0),
(993141, 0, 9, 0x20000, 0, 0x200000, 0x100, 0x1, 0x2, 0x2, 0, 0, 0, 100, 0, 0),
(993146, 1, 0, 0, 0, 0, 0x140, 0x1, 0x2, 0, 0, 0, 0, 0, 0, 0);

-- Scripts. spell_ranger_dot (Quickened Barbs, Quick Venom) goes on Serrated Shot and every rank of Serpent Sting and
-- Black Arrow, beside the scripts those already have.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_ranger_finisher', 'spell_ranger_final_arrow',
  'spell_ranger_ricochet_shot', 'spell_ranger_marked_quarry', 'spell_ranger_advantage', 'spell_ranger_cruel_aim',
  'spell_ranger_barbed_tips', 'spell_ranger_dot');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(993000, 'spell_ranger_finisher'),
(993002, 'spell_ranger_finisher'),
(993005, 'spell_ranger_finisher'),
(993006, 'spell_ranger_finisher'),
(993003, 'spell_ranger_final_arrow'),
(993004, 'spell_ranger_ricochet_shot'),
(993013, 'spell_ranger_marked_quarry'),
(993011, 'spell_ranger_advantage'),
(-993139, 'spell_ranger_cruel_aim'),
(993141, 'spell_ranger_barbed_tips'),
(993001, 'spell_ranger_dot'),
(-1978, 'spell_ranger_dot'),
(-3674, 'spell_ranger_dot');
