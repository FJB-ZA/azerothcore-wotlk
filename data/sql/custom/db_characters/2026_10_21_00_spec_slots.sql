-- solo-wow spec slots: talent group 1 is spec slot 2, so a bought dual spec simply stays.
-- The Wrath activation spells give way to the spec slot switch (spells 990010-990013).
DELETE FROM `character_spell` WHERE `spell` IN (63644, 63645);
DELETE FROM `character_action` WHERE `type` = 0 AND `action` IN (63644, 63645);
