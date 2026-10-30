# Balance harness

A path is a different way to play its class, not extra power. Before a path ships, the harness measures it against
the plain class and the result goes into `<Path>/balance.md`. The rules are in `../SubclassBalancePolicy.h` and
tested by solo-wow's `tests/subclass-balance-policy.cpp`; the command is in mod-playerbots,
`src/Script/SubclassBalanceHarness.cpp`.

## Running it

On a quiet spot of open ground (nothing hostile in 60 yards), as a GM with no bots of the class already added:

```
.balance start <path id> <level> <plain spec no> <path spec no> [fights] [seconds]
.balance status
.balance stop
```

- `path id` is from `.subclass list`. `level` is at least the path's unlock level.
- The spec numbers are the class's premade specs in `playerbots.conf` (`Playerbots.PremadeSpecName.<class>.<no>`).
  The two are the same build for a path without its own tree (the Necromancer's Affliction against the warlock's
  Affliction). For a path with a tree they are the path's premade spec and the plain spec it grew from.
- `fights` defaults to 3 and `seconds` to 180.

## What it does

1. Adds two bots of the path's class (`addclass`, so the account needs two free characters of the class).
2. Brings both to the level (`PlayerbotFactory::Randomize`), puts the second on the path, gives each its spec and
   glyphs, gears the first with blue items for the level (`AutoGear`) and clones every worn item, with its enchants
   and random stats, onto the second.
3. Places them 40 yards apart beside the GM, each with a Grandmaster's Training Dummy of the run's level 15 yards
   ahead, and waits 20 seconds for buffs, pets and minions.
4. Each fight: both attack their own dummy for the set time. The attack is renewed every 3 seconds, because a dummy
   drops a player from combat 5 seconds after their last direct hit. Everything a bot and its pets, guardians and
   totems deal is metered after absorbs (`UnitScript::DealDamage`), before the dummy throws it away.
5. Between fights: cooldowns reset, health and power full, 15 seconds' rest, fresh dummies.
6. After the last fight the bots log out and one row goes to the GM's chat, the server log and
   `<LogsDir>/balance-<path>.md`. Copy that file's rows into the path's `balance.md`.

## Scenarios and limits

| Scenario | Measured | Passes when | No verdict when |
|---|---|---|---|
| Single target (built) | damage per second, pet share, damage taken, deaths | the path's damage per second is within 5% of the plain class's; damage taken (when either takes any) within 5%; the path dies no more often | fewer than 3 fights a side; a side dealt nothing; a side's fights spread more than 2.5% (standard deviation of their damage per second, against its mean) |
| 5-mob pack (to build) | as above, against five mobs of the level that fight back | as above | as above |
| Duel (to build) | wins of each side | the path wins 40-60% of the duels decided | fewer than 10 duels decided |

Levels: 20, 40 and 60 in vanilla; 70 and 80 once those phases open. Each spec the path supports, at each level the
path is open at. A FAIL means tuning the path's `subclass_path` modifiers (or the values its design names) and running
again; "inconclusive" means running more fights.

## Seen in game

First runs, 2026-10-30, Necromancer at level 60, Affliction both sides, 2 fights of 60 seconds (a check of the
harness, not a verdict): both bots logged in, took their specs and the path, fought their dummies for the whole fight,
and the row landed in the file. Damage per second came out at 747 and 678 for the warlock and 651 and 700 for the
Necromancer, a pet share of 9% each.

- Fights of 60 seconds vary by about 10% from one to the next, far past the 2.5% the verdict needs; the default 180
  seconds and more fights are needed before a path can be judged.
- In the very first run the plain warlock dealt nothing in either fight and the cause wasn't found; it hasn't happened
  since. Each fight now logs each bot's level, combat state, target and distance to its dummy, so it shows if it does.
