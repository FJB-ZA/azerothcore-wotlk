-- solo-wow Mythic+ (scripts/Custom/MythicPlus): the Northrend Grand Master Blacksmiths teach the Prismatic Socket Kit
-- (spell 990314, item 990301 in solo-wow's spell data) at 400 skill, so smiths and bots can sell kits on the auction house.
DELETE FROM `trainer_spell` WHERE `SpellId` = 990314;
INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`,
  `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`) VALUES
(59, 990314, 100000, 164, 400, 0, 0, 0, 0, 0);
