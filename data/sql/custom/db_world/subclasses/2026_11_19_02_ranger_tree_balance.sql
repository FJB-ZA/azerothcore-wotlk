-- Windrunner, first balance pass (solo-wow's Ranger balance.md): Piercing Volley no longer ignores armor. Against a
-- level 60 target's armor that nearly trebled it, and it was the tree's biggest hitter by far.
DELETE FROM `spell_custom_attr` WHERE `spell_id` = 993007;
