-- Gladiator warrior path beyond its spell data (solo-wow patches/client/spell-data/subclasses/gladiator.json).
DELETE FROM `subclass_path` WHERE `id` = 1;
INSERT INTO `subclass_path` (`id`, `name`, `class`, `unlock_level`, `era`, `unlock_quest`, `damage_pve`, `damage_pvp`, `healing_pve`, `healing_pvp`, `talent_tab`, `bot_share`) VALUES
(1, 'Gladiator', 1, 10, 0, 0, 1, 1, 1, 1, 990, 0.25);

DELETE FROM `subclass_spell_swap` WHERE `path_id` = 1;
INSERT INTO `subclass_spell_swap` (`path_id`, `spell`, `replacement`) VALUES
(1, 2457, 990100),
(1, 2565, 990101),
(1, 6572, 990110),
(1, 6574, 990111),
(1, 7379, 990112),
(1, 11600, 990113),
(1, 11601, 990114),
(1, 25288, 990115),
(1, 25269, 990116),
(1, 30357, 990117),
(1, 57823, 990118);

DELETE FROM `spell_proc` WHERE `SpellId` IN (-990138, 990151, 990156, -990183, 990187, 990189);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(-990138, 0, 4, 0x400, 0x200, 0, 0x10, 0x1, 0x2, 0x2, 0, 0, 0, 0, 0, 0), -- Battle Fury: Shield Slam / Revenge crits
(990151, 0, 4, 0x4400, 0x40, 0, 0x10, 0x5, 0x2, 0, 0, 0, 0, 100, 0, 0), -- Unyielding Strikes: Revenge, Sunder, Devastate
(990156, 0, 4, 0, 0x200, 0, 0x10, 0x1, 0x2, 0x2, 0, 0, 0, 100, 0, 0), -- Ultimatum: Shield Slam crits
(-990183, 0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0, 0, 0, 0, 0), -- Riposte: parried melee hits
(990187, 0, 0, 0, 0, 0, 0, 0, 0, 0x800, 0, 0, 0, 0, 0, 0), -- Mass Spell Reflection: reflects
(990189, 0, 0, 0, 0, 0, 0, 0x1, 0x2, 0, 0, 0, 0, 100, 0, 0); -- Bloodbath: melee hits that deal damage

DELETE FROM `spell_bonus_data` WHERE `entry` IN (990110, 990162, 990181);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(990110, 0, 0, 0.31, 0, 'Gladiator - Revenge'),
(990162, 0, 0, 0.75, 0, 'Gladiator - Dragon Roar'),
(990181, 0, 0, 0.5, 0, 'Gladiator - Storm Bolt');

DELETE FROM `spell_script_names` WHERE `spell_id` IN (990182, 990189);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(990182, 'spell_gladiator_shield_barrier'),
(990189, 'spell_gladiator_bloodbath');

-- Dragon Roar ignores armor (SPELL_ATTR0_CU_IGNORE_ARMOR).
DELETE FROM `spell_custom_attr` WHERE `spell_id` = 990162;
INSERT INTO `spell_custom_attr` (`spell_id`, `attributes`) VALUES (990162, 0x8000);
