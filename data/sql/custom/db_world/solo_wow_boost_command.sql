-- Help text for the character boost command (solo-wow, src/server/scripts/Custom/boost.cpp).
DELETE FROM `command` WHERE `name` = 'boost';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('boost', 0, 'Syntax: .boost\r\n\r\nBoost this character once per account and era: to 58 in the vanilla and TBC eras, to 68 or 80 in the Wrath era, with a spec''s gear, mounts and bags. The era follows the character''s own progression.');
