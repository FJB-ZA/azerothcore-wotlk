-- Gladiator's Vigor (990168-990170) left the Gladiator tree; a saved rank of it fails the talent load assert.
DELETE FROM `character_talent` WHERE `spell` IN (990168, 990169, 990170);
DELETE FROM `character_spell` WHERE `spell` IN (990168, 990169, 990170);
