-- Gladiator tier set procs and Shield Charge's charges (solo-wow patches/client/spell-data/subclasses/gladiator-gear.json).
DELETE FROM `solo_wow_spell_charges` WHERE `spell` = 990101;
INSERT INTO `solo_wow_spell_charges` (`spell`, `max_charges`, `recharge_ms`) VALUES
(990101, 2, 15000); -- Shield Charge: WoD's 2 charges, 15 sec each

-- The T10 2pc's free Shield Charge buff replaces a real one instead of stacking with it.
DELETE FROM `spell_group_stack_rules` WHERE `group_id` = 990101;
INSERT INTO `spell_group_stack_rules` (`group_id`, `stack_rule`, `description`) VALUES
(990101, 1, 'Gladiator - Shield Charge buffs');
DELETE FROM `spell_group` WHERE `id` = 990101;
INSERT INTO `spell_group` (`id`, `spell_id`) VALUES
(990101, 990101),
(990101, 990210);

DELETE FROM `spell_proc` WHERE `SpellId` IN (990203, 990204, 990208, 990209);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(990203, 0, 4, 0, 0x200, 0x4, 0x10, 0x1, 0x2, 0, 0, 0, 0, 0, 0, 0), -- T7 4pc: Revenge / Shield Slam hits
(990204, 0, 4, 0x40, 0x200, 0, 0x10, 0x1, 0x2, 0x2, 0, 0, 0, 0, 0, 0), -- T8 2pc: Heroic Strike / Shield Slam crits
(990208, 0, 4, 0, 0x200, 0, 0x10, 0x1, 0x2, 0, 0, 0, 0, 0, 0, 0), -- T10 2pc: Shield Slam hits
(990209, 0, 4, 0, 0, 0x2, 0x10, 0, 0x1, 0, 0, 0, 0, 100, 0, 0); -- T10 4pc: Shield Charge casts, not the 2pc's free one
