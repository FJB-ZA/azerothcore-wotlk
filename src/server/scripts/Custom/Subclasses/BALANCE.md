# Balance harness

A path is a different way to play its class, not extra power. Before a path ships, the harness measures it against
the plain class and the result goes into `<Path>/balance.md`. The rules are in `../SubclassBalancePolicy.h` and
tested by solo-wow's `tests/subclass-balance-policy.cpp`; the command is in mod-playerbots,
`src/Script/SubclassBalanceHarness.cpp`.

## Running it

On a quiet spot of open ground (nothing hostile in 60 yards), as a GM with no bots of the class already added:

```
.balance start <path id> <level> <plain spec no> <path spec no> [fights] [seconds] [swap]
.balance status
.balance stop
```

- `path id` is from `.subclass list`. `level` is at least the path's unlock level.
- The spec numbers are the class's premade specs in `playerbots.conf` (`Playerbots.PremadeSpecName.<class>.<no>`).
  The two are the same build for a path without its own tree (the Necromancer's Affliction against the warlock's
  Affliction). For a path with a tree they are the path's premade spec and the plain spec it grew from.
- `fights` is the least a run fights (default 3), and the most is the larger of `fights` and 8; `seconds` is each
  fight's length (default 180). A class whose bots run out of mana in a long fight is measured in shorter fights
  (warlock Destruction: 90): each fight logs both bots' mana and health in tenths, so a fight that ends dry shows.
- `swap 1` puts the bot that would have been plain on the path; the row says "sides swapped". Each run names
  both bots' races, as a race's bonus can count (below).

## What it does

1. Adds two bots of the path's class (`addclass`, so the account needs two free characters of the class).
2. Brings both to the level (`PlayerbotFactory::Randomize`), puts the second on the path, gives each its spec and
   glyphs, gears the first with blue items for the level (`AutoGear`) and clones every worn item, with its random
   stats, enchants and gems, onto the second. It says both bots' item, enchant, spell power and attack power counts,
   so a gear difference shows.
3. Places them 40 yards apart beside the GM, on the ground, each with a Grandmaster's Training Dummy of the run's level
   15 yards ahead, and waits 20 seconds for buffs, pets and minions. A bot drops a target it can't see, so a spot
   without a clear line to its dummy stops the run: stand on flat, open ground.
4. A 60-second warm-up fight that isn't counted (the first fight ran about 10% low on both sides while pets, buffs
   and shards settled).
5. Each fight: both attack their own dummy for the set time. The attack is renewed every 3 seconds, because a dummy
   drops a player from combat 5 seconds after their last direct hit; neither bot assists its group, which would send
   one onto the other's dummy. Everything a bot and its pets, guardians and totems deal is metered after absorbs
   (`UnitScript::DealDamage`), before the dummy throws it away.
6. Between fights: cooldowns reset, health and power full, 15 seconds' rest, fresh dummies; the reset is done again
   as the fight starts.
7. After `fights` fights, while the difference is still too close to the budget to call, it fights on, up to 8.
8. Then the bots log out and one row goes to the GM's chat, the server log and `<LogsDir>/balance-<path>.md`. Copy
   that file's rows into the path's `balance.md`.

## Scenarios and limits

| Scenario | Measured | Passes when | No verdict when |
|---|---|---|---|
| Single target (built) | damage per second, pet share, damage taken, deaths | the path's damage per second is within 5% of the plain class's, margin and all; damage taken (when either takes any) within 5%; the path dies no more often | fewer than 3 fights a side; a side dealt nothing; the difference's margin reaches past the budget but the difference itself doesn't (run more fights) |
| 5-mob pack (to build) | as above, against five mobs of the level that fight back | as above | as above |
| Duel (to build) | wins of each side | the path wins 40-60% of the duels decided | fewer than 10 duels decided |

A fight's damage varies with crits and procs, about 3% from one 180-second fight to the next at level 20. So the
difference between the two sides' means carries a margin of two standard errors (about 95%), and the row shows it
(`+0.7% ± 4.4%`). A difference passes when the whole margin lies inside the 5% budget and fails when the whole margin
lies past it.

Known limit: the two bots are whichever characters `addclass` gives, so they may be different races (in the
Necromancer's runs a Gnome warlock against a Human Necromancer: the Gnome's 5% more Intellect is worth well under 1% of
damage). A path close to the budget's edge is worth running again with `swap 1`.

Levels: 20, 40 and 60 in vanilla; 70 and 80 once those phases open. Each spec the path supports, at each level the
path is open at. A FAIL means tuning the path's `subclass_path` modifiers (or the values its design names) and running
again; "inconclusive" means running more fights.

## Seen in game

The Necromancer, 2026-10-30, levels 20, 40 and 60, each warlock spec: Affliction passes at all three, Demonology at
all three (at 60 with the sides swapped), Destruction at 20 and 60 (90-second fights); Destruction at 40 has no
single-run verdict but sits inside the budget once the two bots' races are averaged out. The rows and what they mean
are in `Necromancer/balance.md`.

Getting there found these in the harness and the bots, each fixed and measured again:

- The plain bot dealt nothing at level 20: its dummy sat in a slope at the GM's height, out of its sight, and a bot
  drops a target it can't see. Spots are now on the ground and checked.
- The two bots hit the same dummy now and then: "dps assist" sent the plain one onto the path bot's target.
- The first fight ran about 10% low on both sides: the uncounted warm-up.
- A per-fight spread limit of 2.5% turned away almost every run; a 180-second fight varies about 3%. The margin rule
  replaced it.
- Necromancer bots recast Plague Strike every global cooldown and never reached Shadow Bolt: `PlayerbotAI::GetAura`
  didn't map Immolate to the path's spell. This was in every Necromancer bot, not only the harness's.
- The path bot's cloned gear had no enchants or gems, 10-27% of its damage at level 60.
- Two bots of different races: a gnome's Intellect is worth 1-3% in a fight short of mana. Hence `swap`.

- Destruction bots swung about ±15% from fight to fight. At 0 mana and under 45% health a warlock bot never Life
  Tapped, and wanded out the fight. Unattacked, it now taps while the tap leaves it above 25% health. A tap to under
  10% in the first try showed the floor must count the tap's own cost. A run's first fight also started with the
  warm-up's cooldowns and mana: the reset now happens as each fight starts.
- In 180-second fights Destruction bots still run dry at about 110 seconds; 90-second fights end before that at 60,
  but not quite at 40.

Still open: the two bots' races. At level 40 Destruction the gnome's Intellect was worth about 3 points either way.
`swap 1` and averaging the two runs handles it; two bots of the same race would be cleaner.
