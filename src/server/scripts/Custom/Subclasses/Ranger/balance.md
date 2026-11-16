# Ranger balance

Measured with the balance harness (`../BALANCE.md`): a plain hunter bot with its pet out against a Ranger bot of the
same level, talents and gear, each on its own Grandmaster's Training Dummy, 16 fights of 60 seconds after an
uncounted warm-up (`.balance start 3 <level> <spec> <spec> 16 60`). The difference carries a margin of two standard
errors; the budget is 5%. Ranger's Focus at +100% Auto Shot damage (solo-wow's `ranger.json`, `EffectBasePoints_1`
99).

## Single target

| Date | Level | Spec | Scenario | Plain | Path | Difference | Damage taken | Pet share | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| 2026-11-16 | 20 | mm pve / mm pve | single target | 25.7 DPS | 26.1 DPS | +1.5% ± 6.0% | +0.0% | 34% / 0% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-16 | 40 | mm pve / mm pve | single target | 86.2 DPS | 87.8 DPS | +1.8% ± 4.3% | +0.0% | 26% / 0% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-16 | 60 | mm pve / mm pve | single target | 472.3 DPS | 461.6 DPS | -2.3% ± 4.8% | +0.0% | 17% / 0% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-16 | 20 | surv pve / surv pve | single target | 22.6 DPS | 22.6 DPS | +0.2% ± 3.4% | +0.0% | 33% / 0% | pass |
| 2026-11-16 | 40 | surv pve / surv pve | single target | 62.6 DPS | 64.2 DPS | +2.6% ± 2.6% | +0.0% | 33% / 0% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-16 | 60 | surv pve / surv pve | single target | 681.1 DPS | 678.7 DPS | -0.3% ± 4.7% | +0.0% | 8% / 0% | inconclusive: too close to the budget for 16 fights; run more |

The two bots were a night elf (plain) and a dwarf (Ranger), both with the same bow and arrows, so neither race's
weapon bonus counted.

## Reading

- **Every run is inside the budget; one passes outright.** The differences run from -2.3% to +2.6%, with no lean
  either way across trees or levels. The other five are "inconclusive" only because a hunter's damage swings from
  fight to fight (crits on big shots, Lock and Load procs), so the margin reaches past 5% in 16 fights; the
  difference itself never comes near it.
- **What a pet is worth.** As a share of the plain hunter's damage the pet ranges from 8% (Survival at 60) to 34%
  (level 20), so no "+X% damage" fits. Measured against the plain hunter's own Auto Shot it is close to one more Auto
  Shot at every level and in both trees: the runs at +85% needed 90-108%. Ranger's Focus therefore doubles Auto Shot.
- **The settings tried on the way.** +30% to all damage fell short at 20 and 40. Ranged attack power in
  ranks by level fitted 20 and 40 but not 60, where Survival's Explosive Shot drew far more from it than
  Marksmanship's shots did. +60% Auto Shot left the Ranger about 10% behind, and +85% about 4% behind.
- **Harness and bot faults found here, fixed in the playerbots fork:**
  - A talent's trained ranks survived a bot's reset when it had no spec slot, so the Ranger bot had level 80's Aimed
    Shot and Black Arrow at 40 and 60.
  - A bot whose strategies were reset mid-run went back to assisting its group and left its dummy for the plain
    bot's, where the pet made up most of the attackers. The harness now puts the run's strategies back on every
    tick and says when it had to (10 times in this set); the trace marks a stray with "x" (none after the first
    sample of a fight).
  - Hunter bots only started Auto Shot when every shot above it in the rotation failed. A plain hunter's pet
    actions gave it those gaps; a Marksmanship Ranger at 60, whose Steady Shot is always ready, never fired Auto
    Shot at all (-40%). Hunter bots now start it whenever it isn't running; in this set the Ranger was shooting in
    all 1,931 trace samples.
- Not measured yet: the 2-mob pack (where the pet's tanking shows up as damage taken, and Field Dressing is the
  dial), duels, and levels 70 and 80.
