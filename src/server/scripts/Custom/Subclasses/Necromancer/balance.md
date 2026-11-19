# Necromancer balance

Measured with the balance harness (`../BALANCE.md`): a plain warlock bot against a Necromancer bot of the same level,
talents and gear, each on its own Grandmaster's Training Dummy, fights of 180 seconds (Destruction at 40 and 60: 90
seconds, below) after an uncounted warm-up. The
difference carries a margin of two standard errors; the budget is 5%.

## Single target

| Date | Level | Spec | Scenario | Plain | Path | Difference | Damage taken | Pet share | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| 2026-10-30 | 20 | affli pve / affli pve | single target | 52.0 DPS | 51.0 DPS | -2.0% ± 2.1% | +0.0% | 15% / 15% | pass |
| 2026-10-30 | 20 | demo pve / demo pve | single target | 49.3 DPS | 47.9 DPS | -2.8% ± 0.5% | +0.0% | 15% / 16% | pass |
| 2026-10-30 | 20 | destro pve / destro pve | single target | 57.7 DPS | 57.4 DPS | -0.6% ± 3.4% | +0.0% | 22% / 25% | pass |
| 2026-10-30 | 40 | affli pve / affli pve | single target | 197.2 DPS | 201.5 DPS | +2.2% ± 1.8% | +0.0% | 4% / 4% | pass |
| 2026-10-30 | 40 | demo pve / demo pve | single target | 126.4 DPS | 121.5 DPS | -3.9% ± 0.6% | +0.0% | 8% / 8% | pass |
| 2026-10-30 | 40 | destro pve / destro pve | single target | 162.1 DPS | 155.8 DPS | -3.9% ± 5.4% | +0.0% | 14% / 17% | inconclusive: too close to the budget for 8 fights; run more |
| 2026-10-30 | 60 | affli pve / affli pve | single target | 799.3 DPS | 788.5 DPS | -1.4% ± 0.6% | +0.0% | 8% / 8% | pass |
| 2026-10-30 | 60 | demo pve / demo pve | single target | 429.5 DPS | 413.0 DPS | -3.8% ± 2.1% | +0.0% | 18% / 20% | inconclusive: too close to the budget for 8 fights; run more |
| 2026-10-30 | 60 | demo pve / demo pve (sides swapped) | single target | 469.5 DPS | 464.2 DPS | -1.1% ± 3.6% | +0.0% | 17% / 18% | pass |
| 2026-10-30 | 60 | destro pve / destro pve | single target | 809.8 DPS | 839.5 DPS | +3.7% ± 12.1% | +0.0% | 5% / 8% | inconclusive: too close to the budget for 8 fights; run more |

The two bots were a gnome (plain) and a human (Necromancer); "sides swapped" puts the gnome on the path.

### Destruction, 90-second fights

After the Life Tap fix (below), measured again with fights of 90 seconds, `.balance start 2 <level> 2 2 6 90`:

| Date | Level | Spec | Scenario | Plain | Path | Difference | Damage taken | Pet share | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| 2026-10-30 | 40 | destro pve / destro pve | single target | 227.3 DPS | 222.0 DPS | -2.4% ± 4.1% | +0.0% | 11% / 13% | inconclusive: too close to the budget for 8 fights; run more |
| 2026-10-30 | 40 | destro pve / destro pve (sides swapped) | single target | 225.5 DPS | 234.8 DPS | +4.1% ± 4.0% | +0.0% | 10% / 13% | inconclusive: too close to the budget for 12 fights; run more |
| 2026-10-30 | 60 | destro pve / destro pve | single target | 889.1 DPS | 901.9 DPS | +1.4% ± 3.2% | +0.0% | 5% / 7% | pass |

## Reading

- **Affliction passes at 20, 40 and 60.**
- **Demonology passes at 20 and 40; at 60 it passes with the sides swapped.** The Necromancer sits 1-4% behind at
  every level. Demonology bots run short of mana and wand, so the gnome's 5% more Intellect counts: -3.8% with the
  gnome plain, -1.1% with the gnome on the path, about -2.5% between the two. Inside the budget.
- **Destruction passes at 20 and 60; at 40 it is inside the budget across both sides, with no single-run
  verdict.** In 180-second fights a Destruction bot's damage swung about ±15% (636 to 933 DPS for the same bot at
  60). The per-fight mana and health trace showed why: by about 110 seconds a bot sat at 0 mana and 30-40% health,
  below the 45% floor under which bots never Life Tap, and wanded out the fight; when that came depended on random
  Soul Leech heals. Bots now Life Tap, while nothing attacks them, as long as the tap leaves them above 25%
  (mod-playerbots 57b794d1), and the 90-second fights end before the wall at 60. At 40 the wall still comes in the
  last 10-15 seconds, so the gnome's extra mana counts for about 3 points: -2.4% with the gnome plain, +4.1% with
  the gnome on the path. The mean of the two, which cancels the race, is +0.9% ± 2.9%.
- Not measured yet: the 5-mob pack, duels, and levels 70 and 80. Two bots of the same race would remove the race
  effect from runs like Destruction at 40; the harness takes whichever bots `addclass` gives.

## Blight

The Necromancer's own tree (talent tab 3, `blight` premade spec 6) against plain Affliction (spec 0), 60-second
fights, `.balance start 2 <level> 0 6 16 60`; the pack is `.balance pack 2 60 0 6 32 180 0 0 0 2`. Disease ranks
as in solo-wow's `necromancer-tree.json` (Grave Worms and Plague of the Risen 54 a tick at 40 and 245 at 60).

| Date | Level | Spec | Scenario | Plain | Path | Difference | Damage taken | Pet share | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| 2026-11-19 | 20 | affli pve / blight | single target | 61.8 DPS | 62.8 DPS | +1.6% ± 1.9% | +0.0% | 13% / 12% | pass |
| 2026-11-19 | 40 | affli pve / blight | single target | 203.1 DPS | 203.8 DPS | +0.3% ± 1.8% | +0.0% | 4% / 4% | pass |
| 2026-11-19 | 60 | affli pve / blight | single target | 776.1 DPS | 782.7 DPS | +0.9% ± 1.8% | +0.0% | 8% / 9% | pass |
| 2026-11-19 | 60 | affli pve / blight | 2-mob pack | 483.8 DPS | 485.2 DPS | +0.3% ± 7.1% | -13.3% ± 14.7% | 16% / 17% | inconclusive: too close to the budget for 32 fights; run more |

### Reading

- **Blight passes single target at 20, 40 and 60.** It took disease ranks to get there. With one rank whose ticks grew
  by a fixed amount a level, Blight was 12% ahead at 20 and 7% behind at 40 (first ranks: 6% ahead at 40 and 14-17%
  behind at 60). Affliction gains Unstable Affliction, Haunt and its Corruption talents from 40 on, and Affliction's
  Corruption hits 1.7 times as hard as Blight's at 60, so the diseases tick below Corruption at 20, level with it at
  40, and well above it from 50.
- **The 2-mob pack is centred but inconclusive**, as the base path's was: fights of about 13 seconds swing ±10-20%.
  A lone level 60 warlock, plain or Necromancer, dies to 3 or 5 mobs in most fights, so the 5-mob pack in the design
  can't be measured.
- Mutation and Raise Gargoyle are boosts: bots use them in hard fights (the pack), never on a dummy, like other
  classes' cooldowns, so the single-target figures leave them out.
- Not measured yet: the 20-point dip, levels 70 and 80, duels.
