-- Windrunner, sixth balance pass (solo-wow's Ranger balance.md): Precision Shot and Serrated Shot lose their flat
-- per-level damage (the spell data) and take more ranged attack power instead, so they grow with gear rather than
-- level. The flat part was a third of their damage at level 20 and under a fifth at 60, and level 20 ran 7% over.
UPDATE `spell_bonus_data` SET `ap_bonus` = 0.15 WHERE `entry` = 993000;
UPDATE `spell_bonus_data` SET `ap_dot_bonus` = 0.04 WHERE `entry` = 993001;
