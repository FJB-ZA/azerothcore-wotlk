-- solo-wow Mythic+ (scripts/Custom/MythicPlus): the Dalaran keystone master, the Font of Power at every WotLK heroic
-- entrance, the end chest and the creatures the affixes and boss mechanics summon.
DELETE FROM `creature_template` WHERE `entry` BETWEEN 990920 AND 990923;
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `BaseAttackTime`, `RangeAttackTime`, `unit_class`, `unit_flags`, `type`, `HealthModifier`, `RegenHealth`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(990920, 'Keystone Master', 'Mythic+', 80, 80, 2, 35, 1, 1, 1.14286, 0, 2000, 2000, 1, 0, 7, 1, 1, 2, 'npc_mythic_plus', 0),
(990921, 'Explosive Orb', '', 80, 80, 2, 14, 0, 0, 0, 0, 2000, 2000, 1, 0, 10, 0.05, 0, 64, 'npc_mythic_explosive_orb', 0),
(990922, 'Mythic Servant', '', 80, 80, 2, 14, 0, 1, 1.14286, 1, 2000, 2000, 1, 0, 6, 2, 1, 64, '', 0),
(990923, 'Mythic Rune', '', 80, 80, 2, 14, 0, 1, 1.14286, 0, 2000, 2000, 1, 33554434, 10, 1, 1, 128, '', 0);

-- Ivrenne's Kirin Tor look, a soul fragment, a Servant of the Throne and the invisible trigger
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 990920 AND 990923;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(990920, 0, 27556, 1, 1, 0),
(990921, 0, 30233, 1.5, 1, 0),
(990922, 0, 26919, 1, 1, 0),
(990923, 0, 169, 1, 1, 0);

-- Type 2 so a click reaches the script's gossip hook; the Titan Relic and the Champion's Cache looks
DELETE FROM `gameobject_template` WHERE `entry` BETWEEN 990920 AND 990921;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`, `ScriptName`, `VerifiedBuild`) VALUES
(990920, 2, 7951, 'Font of Power', '', '', '', 0.8, 'go_mythic_font_of_power', 0),
(990921, 2, 9069, 'Mythic Cache', '', '', '', 1.5, 'go_mythic_cache', 0);

-- Beside the emblem quartermasters: the Silver Enclave and Sunreaver's Sanctuary
DELETE FROM `creature` WHERE `id` = 990920 AND `guid` BETWEEN 9909200 AND 9909201;
INSERT INTO `creature` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `MovementType`, `VerifiedBuild`) VALUES
(9909200, 990920, 571, 1, 1, 0, 5763.85, 740.12, 653.748, 3.67, 300, 0, 0, 0),
(9909201, 990920, 571, 1, 1, 0, 5937.0, 510.33, 650.263, 2.3, 300, 0, 0, 0);

-- Four yards inside each entrance, facing the way in; heroic only, which is the difficulty a key runs on
DELETE FROM `gameobject` WHERE `id` = 990920 AND `guid` BETWEEN 9909201 AND 9909216;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`) VALUES
(9909201, 990920, 574, 2, 1, 157.606, -85.351, 12.551, 3.4456, 0, 0, 0.988470, -0.151415, 0, 100, 1), -- Utgarde Keep
(9909202, 990920, 575, 2, 1, 580.118, -327.896, 110.138, 6.2636, 0, 0, 0.009796, -0.999952, 0, 100, 1), -- Utgarde Pinnacle
(9909203, 990920, 576, 2, 1, 146.041, -6.558, -16.636, 4.6696, 0, 0, 0.722075, -0.691815, 0, 100, 1), -- The Nexus
(9909204, 990920, 578, 2, 1, 1059.365, 984.800, 361.070, 2.6034, 0, 0, 0.964013, 0.265857, 0, 100, 1), -- The Oculus
(9909205, 990920, 595, 2, 1, 1432.831, 553.314, 36.690, 2.0184, 0, 0, 0.846408, 0.532535, 0, 100, 1), -- The Culling of Stratholme
(9909206, 990920, 599, 2, 1, 1153.250, 802.164, 195.937, 1.5734, 0, 0, 0.708029, 0.706183, 0, 100, 1), -- Halls of Stone
(9909207, 990920, 600, 2, 1, -516.870, -491.948, 11.010, 1.6894, 0, 0, 0.747774, 0.663953, 0, 100, 1), -- Drak'Tharon Keep
(9909208, 990920, 601, 2, 1, 416.149, 793.146, 831.351, 2.3584, 0, 0, 0.924302, 0.381661, 0, 100, 1), -- Azjol-Nerub
(9909209, 990920, 602, 2, 1, 1331.708, 255.626, 53.398, 1.6304, 0, 0, 0.727865, 0.685720, 0, 100, 1), -- Halls of Lightning
(9909210, 990920, 604, 2, 1, 1889.790, 835.604, 176.669, 5.2506, 0, 0, 0.493663, -0.869654, 0, 100, 1), -- Gundrak
(9909211, 990920, 608, 2, 1, 1812.820, 803.925, 44.364, 3.1404, 0, 0, 1.000000, 0.000593, 0, 100, 1), -- The Violet Hold
(9909212, 990920, 619, 2, 1, 336.755, -1107.839, 69.772, 3.6946, 0, 0, 0.962017, -0.272990, 0, 100, 1), -- Ahn'kahet
(9909213, 990920, 632, 2, 1, 4921.183, 2179.261, 638.734, 5.1451, 0, 0, 0.538808, -0.842429, 0, 100, 1), -- The Forge of Souls
(9909214, 990920, 650, 2, 1, 801.227, 618.022, 412.393, 0.0040, 0, 0, 0.002004, 0.999998, 0, 100, 1), -- Trial of the Champion
(9909215, 990920, 658, 2, 1, 439.742, 212.306, 528.709, 3.1149, 0, 0, 0.999911, 0.013362, 0, 100, 1), -- Pit of Saron
(9909216, 990920, 668, 2, 1, 5241.795, 1935.511, 707.695, 3.9422, 0, 0, 0.920951, -0.389679, 0, 100, 1); -- Halls of Reflection
