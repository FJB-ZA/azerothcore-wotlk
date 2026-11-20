-- Windrunner, fourth balance pass (solo-wow's Ranger balance.md): Serrated Shot takes 3% of ranged attack power a tick,
-- down from 4%. A level 40 Windrunner ran 11% over Marksmanship while 60 sat 3% under, so the early tiers lose damage
-- and the late ones gain it.
UPDATE `spell_bonus_data` SET `ap_dot_bonus` = 0.03 WHERE `entry` = 993001;
