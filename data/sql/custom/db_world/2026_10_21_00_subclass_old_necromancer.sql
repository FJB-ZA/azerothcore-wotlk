-- Clears what the first Necromancer build (before it moved onto the sub-class path engine) left in a world database:
-- its own subclass tables, whose columns differ from the engine's, and its rows at IDs 990100-990199, which belong to
-- the Gladiator (path 1) now. It runs before the engine's tables and the paths, which build theirs again. Where that
-- build never ran (no subclass_path.base_class column), it changes nothing.
DROP TABLE IF EXISTS `subclass_path_unlock`, `subclass_path_trainer`;

SET @old_necromancer := (SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'subclass_path' AND COLUMN_NAME = 'base_class');

DROP PROCEDURE IF EXISTS `solo_wow_old_necromancer`;
DELIMITER //
CREATE PROCEDURE `solo_wow_old_necromancer`()
BEGIN
  IF @old_necromancer > 0 THEN
    DROP TABLE IF EXISTS `subclass_path`, `subclass_spell_swap`, `subclass_creature_swap`;
    DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 990100 AND 990199;
    DELETE FROM `spell_script_names` WHERE ABS(`spell_id`) BETWEEN 990100 AND 990199;
    DELETE FROM `spell_proc` WHERE ABS(`SpellId`) BETWEEN 990100 AND 990199;
    DELETE FROM `spell_custom_attr` WHERE `spell_id` BETWEEN 990100 AND 990199;
    DELETE FROM `spell_group` WHERE `spell_id` BETWEEN 990100 AND 990199;
    DELETE FROM `creature` WHERE `guid` IN (9990170, 9990171);
    DELETE FROM `creature_template` WHERE `entry` BETWEEN 990150 AND 990175;
    DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 990150 AND 990175;
    DELETE FROM `creature_template_addon` WHERE `entry` BETWEEN 990150 AND 990175;
    DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 990150 AND 990175;
    DELETE FROM `creature_template_movement` WHERE `CreatureId` BETWEEN 990150 AND 990175;
    DELETE FROM `pet_levelstats` WHERE `creature_entry` BETWEEN 990150 AND 990157;
    DELETE FROM `pet_name_generation` WHERE `entry` BETWEEN 990150 AND 990157;
    DELETE FROM `npc_text` WHERE `ID` BETWEEN 990170 AND 990173;
    DELETE FROM `quest_template` WHERE `ID` IN (990170, 990171);
    DELETE FROM `quest_template_addon` WHERE `ID` IN (990170, 990171);
    DELETE FROM `quest_details` WHERE `ID` IN (990170, 990171);
    DELETE FROM `quest_request_items` WHERE `ID` IN (990170, 990171);
    DELETE FROM `quest_offer_reward` WHERE `ID` IN (990170, 990171);
    DELETE FROM `creature_queststarter` WHERE `quest` IN (990170, 990171);
    DELETE FROM `creature_questender` WHERE `quest` IN (990170, 990171);
    DELETE FROM `item_template` WHERE `entry` = 990170;
    DELETE FROM `page_text` WHERE `ID` BETWEEN 990170 AND 990172;
    DELETE FROM `gameobject_template` WHERE `entry` = 990176;
    DELETE FROM `gameobject_template_addon` WHERE `entry` = 990176;
  END IF;
END//
DELIMITER ;
CALL `solo_wow_old_necromancer`();
DROP PROCEDURE `solo_wow_old_necromancer`;
