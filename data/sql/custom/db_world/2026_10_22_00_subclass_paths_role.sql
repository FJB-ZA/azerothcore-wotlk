-- What a path's own tree plays as for the core's role helpers (HasTankSpec and the rest): SmartAI role targeting and
-- bots reading a player's role. Named to run after the table's creation and before the paths' own files.
ALTER TABLE `subclass_path` ADD COLUMN `role` TINYINT UNSIGNED NOT NULL DEFAULT 0
  COMMENT '0 none (stock rules), 1 tank, 2 melee, 3 caster, 4 healer' AFTER `bot_share`;
