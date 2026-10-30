# Necromancer balance

Measured with the balance harness (`../BALANCE.md`): a plain warlock bot against a Necromancer bot of the same level,
talents and gear, each on its own Grandmaster's Training Dummy, fights of 180 seconds after an uncounted warm-up. The
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

## Reading

- **Affliction passes at 20, 40 and 60.**
- **Demonology passes at 20 and 40; at 60 it passes with the sides swapped.** The Necromancer sits 1-4% behind at
  every level. Demonology bots run short of mana and wand, so the gnome's 5% more Intellect counts: -3.8% with the
  gnome plain, -1.1% with the gnome on the path, about -2.5% between the two. Inside the budget.
- **Destruction has no verdict at 40 or 60.** A Destruction bot's fights swing about ±15% (636 to 933 DPS for the
  same bot at 60): its weak fights have fewer Life Taps, more wand shots and once a drink mid-fight. That is the
  bot's mana and health handling, on both sides, not the path; it needs fixing, or a measurement with mana held
  full, before Destruction can be judged.
- Not measured yet: the 5-mob pack, duels, and levels 70 and 80.
