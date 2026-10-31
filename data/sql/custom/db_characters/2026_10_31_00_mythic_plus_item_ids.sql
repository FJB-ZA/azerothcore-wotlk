-- solo-wow Mythic+: the Mythic Keystone and the Prismatic Socket Kit moved from items 990300 and 990301 to 990900
-- and 990901 on 2026-10-31, leaving 990300-990499 to the Necromancer path. Keystones and kits already in bags, banks,
-- mail and the auction house keep working. Re-applicable.
UPDATE `item_instance` SET `itemEntry` = 990900 WHERE `itemEntry` = 990300;
UPDATE `item_instance` SET `itemEntry` = 990901 WHERE `itemEntry` = 990301;
