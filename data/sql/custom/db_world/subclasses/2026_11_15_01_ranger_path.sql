-- Ranger hunter path (path 3): a hunter without a pet. Its spells are in solo-wow's
-- patches/client/spell-data/subclasses/ranger.json; design in solo-wow's
-- docs/superpowers/specs/2026-11-15-subclasses-ranger-design.md. IDs 990500-990699 are this path's in every table
-- (spells, creatures, NPCs, quests, items and texts). The trainers, quests and book are in the _02 file.
DELETE FROM `subclass_path` WHERE `id` = 3;
INSERT INTO `subclass_path` (`id`, `name`, `class`, `unlock_level`, `era`, `unlock_quest`, `damage_pve`, `damage_pvp`, `healing_pve`, `healing_pvp`, `talent_tab`, `bot_share`, `role`, `pets`) VALUES
(3, 'Ranger', 3, 10, 0, 990571, 1, 1, 1, 1, 0, 0.25, 0, 0);

-- The pet spells go, with nothing in their place: Call Pet, Dismiss Pet, Revive Pet, Tame Beast, Feed Pet, Beast
-- Training, Eyes of the Beast, Kill Command (TBC), Master's Call and Call Stabled Pet (Wrath)
DELETE FROM `subclass_spell_remove` WHERE `path_id` = 3;
INSERT INTO `subclass_spell_remove` (`path_id`, `spell`) VALUES
(3, 883), (3, 2641), (3, 982), (3, 1515), (3, 6991), (3, 5149), (3, 1002), (3, 34026), (3, 53271), (3, 62757);

-- Ranger's Focus: the damage a pet would have dealt (+30%, tuned by the balance harness in ranger.json)
DELETE FROM `subclass_spell_grant` WHERE `path_id` = 3;
INSERT INTO `subclass_spell_grant` (`path_id`, `spell`) VALUES (3, 990500);

-- Mend Pet -> Field Dressing, rank for rank: the same heal on the hunter
DELETE FROM `subclass_spell_swap` WHERE `path_id` = 3;
INSERT INTO `subclass_spell_swap` (`path_id`, `spell`, `replacement`) VALUES
(3, 136, 990501), (3, 3111, 990502), (3, 3661, 990503), (3, 3662, 990504), (3, 13542, 990505), (3, 13543, 990506),
(3, 13544, 990507), (3, 27046, 990508), (3, 48989, 990509), (3, 48990, 990510);
