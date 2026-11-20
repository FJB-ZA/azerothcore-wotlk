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

## After the Arcane Shot fix

Every hunter bot without Explosive Shot used to skip Arcane Shot (see the Windrunner findings below), so the plain
side of the rows above under-cast it. A check of the base path with the fix in:

| Date | Level | Spec | Scenario | Plain | Path | Difference | Damage taken | Pet share | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| 2026-11-20 | 40 | mm pve / mm pve | single target | 118.2 DPS | 114.2 DPS | -3.3% ± 4.1% | +0.0% | 20% / 0% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-20 | 20 | mm pve / mm pve | single target | 40.4 DPS | 40.3 DPS | -0.2% ± 4.0% | +0.0% | 20% / 0% | pass |
| 2026-11-20 | 60 | mm pve / mm pve | single target | 514.1 DPS | 522.9 DPS | +1.7% ± 5.4% | +0.0% | 13% / 0% | inconclusive: too close to the budget for 16 fights; run more |

The plain hunter went from 86 to 118 DPS at 40 and from 26 to 40 at 20. The Ranger kept pace, so Ranger's Focus
still holds.

## Windrunner

The Ranger's own talent tree (`ranger_windrunner.cpp`, design in solo-wow's
`docs/superpowers/specs/2026-11-19-subclasses-ranger-tree-design.md`), as the "windrunner" spec, against the plain
hunter's Marksmanship: `.balance start 3 <level> 1 6 16 60`.

| Date | Level | Spec | Scenario | Plain | Path | Difference | Damage taken | Pet share | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| 2026-11-19 | 60 | mm pve / windrunner | single target | 468.4 DPS | 462.9 DPS | -1.2% ± 4.3% | +0.0% | 14% / 0% | pass 1, before the Serrated Shot fix |
| 2026-11-19 | 20 | mm pve / windrunner | single target | 26.1 DPS | 26.9 DPS | +3.0% ± 1.9% | +0.0% | 32% / 0% | pass 1 |
| 2026-11-19 | 60 | mm pve / windrunner | single target | 462.7 DPS | 505.0 DPS | +9.2% ± 4.9% | +0.0% | 15% / 0% | pass 1 with Serrated and Arcane Shot cast |
| 2026-11-19 | 40 | mm pve / windrunner | single target | 86.9 DPS | 132.9 DPS | +52.9% ± 4.2% | +0.0% | 26% / 0% | pass 2; the plain bot skipped Arcane Shot |
| 2026-11-19 | 60 | mm pve / windrunner | single target | 486.0 DPS | 499.0 DPS | +2.7% ± 4.0% | +0.0% | 16% / 0% | pass 2; the plain bot skipped Arcane Shot |
| 2026-11-19 | 20 | mm pve / windrunner | single target | 27.2 DPS | 45.8 DPS | +68.3% ± 4.0% | +0.0% | 33% / 0% | pass 2; the plain bot skipped Arcane Shot |
| 2026-11-20 | 20 | mm pve / windrunner | single target | 40.7 DPS | 44.0 DPS | +8.1% ± 3.9% | +0.0% | 22% / 0% | pass 2, Arcane Shot fixed: inconclusive |
| 2026-11-20 | 40 | mm pve / windrunner | single target | 117.8 DPS | 139.5 DPS | +18.4% ± 3.9% | +0.0% | 20% / 0% | pass 2, Arcane Shot fixed: FAIL |
| 2026-11-20 | 60 | mm pve / windrunner | single target | 495.1 DPS | 489.1 DPS | -1.2% ± 4.2% | +0.0% | 14% / 0% | pass 2, Arcane Shot fixed: inconclusive |
| 2026-11-20 | 40 | mm pve / windrunner | single target | 118.3 DPS | 131.2 DPS | +10.9% ± 4.8% | +0.0% | 19% / 0% | pass 3: FAIL |
| 2026-11-20 | 60 | mm pve / windrunner | single target | 511.1 DPS | 494.1 DPS | -3.3% ± 3.9% | +0.0% | 13% / 0% | pass 3: inconclusive |
| 2026-11-20 | 40 | mm pve / windrunner | single target | 116.0 DPS | 123.4 DPS | +6.3% ± 3.7% | +0.0% | 19% / 0% | pass 4: inconclusive |
| 2026-11-20 | 60 | mm pve / windrunner | single target | 500.3 DPS | 514.0 DPS | +2.7% ± 2.2% | +0.0% | 13% / 0% | pass 4: pass |
| 2026-11-20 | 20 | mm pve / windrunner | single target | 41.2 DPS | 43.7 DPS | +6.1% ± 2.9% | +0.0% | 22% / 0% | pass 4: inconclusive |
| 2026-11-20 | 40 | mm pve / windrunner | single target | 120.4 DPS | 122.6 DPS | +1.8% ± 3.7% | +0.0% | 20% / 0% | pass 5: inside the budget, margin past it |
| 2026-11-20 | 20 | mm pve / windrunner | single target | 38.9 DPS | 42.7 DPS | +9.8% ± 3.3% | +0.0% | 21% / 0% | pass 5: FAIL |
| 2026-11-20 | 60 | mm pve / windrunner | single target | 541.5 DPS | 498.3 DPS | -8.0% ± 3.3% | +0.0% | 14% / 0% | pass 5: inconclusive |
| 2026-11-20 | 20 | mm pve / windrunner | single target | 40.6 DPS | 39.8 DPS | -2.0% ± 4.6% | +0.0% | 22% / 0% | pass 6: inside the budget, margin past it |
| 2026-11-20 | 40 | mm pve / windrunner | single target | 121.7 DPS | 115.2 DPS | -5.3% ± 4.2% | +0.0% | 20% / 0% | pass 6: inconclusive |
| 2026-11-20 | 60 | mm pve / windrunner | single target | 501.6 DPS | 446.7 DPS | -10.9% ± 4.0% | +0.0% | 13% / 0% | pass 6: FAIL |
| 2026-11-20 | 60 | mm pve / windrunner | single target (32 fights) | 505.1 DPS | 495.9 DPS | -1.8% ± 3.2% | +0.0% | 14% / 0% | pass 7: inside the budget, margin past it |
| 2026-11-20 | 40 | mm pve / windrunner | single target | 117.7 DPS | 117.6 DPS | -0.1% ± 3.8% | +0.0% | 20% / 0% | pass 7: pass |
| 2026-11-20 | 20 | mm pve / windrunner | single target | 40.3 DPS | 40.2 DPS | -0.3% ± 3.5% | +0.0% | 21% / 0% | pass 7: pass |
| 2026-11-20 | 60 | mm pve / windrunner dip | single target | 525.8 DPS | 399.4 DPS | -24.0% ± 4.0% | +0.0% | 13% / 0% | pass 7, dip check: the dip doesn't beat 51 Marksmanship |
| 2026-11-20 | 60 | mm pve / windrunner | 2-mob pack | 441.9 DPS | 332.6 DPS | -24.7% ± 4.7% | +46.3% ± 11.7% | 24% / 0% | pass 7: FAIL; the base path's pack row decides whether the tree or the missing pet is to blame |

The dip check (`.balance start 3 60 1 7 16 60`) gives the Ranger 31 points of Marksmanship and Windrunner's first 20
against the plain hunter's 51 in Marksmanship. It is there to show that a dip doesn't beat a full tree, and it falls
far short: the dip has no Chimera Shot (about a fifth of the plain hunter's damage) and none of Marksmanship's deep
passives, and Windrunner's first four tiers were kept modest on purpose (passes 4 to 6). Blight's dip came out even
(-0.3%).
### Reading

- **The passes.** The first build had Advantage at +20% a stack, Precision Shot at 100% weapon damage, Final Arrow
  at 250%, Piercing Volley at 450% ignoring armor, Corroded Hide at 10/20% and Open Wounds at 5/10%. It ran about
  +75% at 60. Pass 1 set Advantage to 10% a stack, Precision Shot to 70%, Final Arrow to 150% and Piercing Volley to
  300% with the armor ignore removed. Pass 2 set Corroded Hide to 5/10%, Open Wounds to 3/6% and Piercing Volley to
  200%. With the bot faults fixed, that left 60 even and 40 at +18%.
- **Why 40 and not 60.** Marksmanship gets most of its damage late (Chimera Shot, more Steady Shot), while
  Windrunner's flat passives sat at tiers 5 and 6, inside a level 40 build. Against the dummy's 9729 armor at every
  level, ignoring 10% of it is worth about 8% of physical damage at 20 and 40 (6.6% at 60), and the sting that Open
  Wounds needs is always up. Pass 3 moves Corroded Hide to tier 8 and Open Wounds to tier 7, trades Barbed Tips and
  Heavy Draw up into their places, sets Ricochet Shot to 60% weapon damage and raises Piercing Volley to 250%.
- **Passes 4 to 7.**
  - Pass 4 moved damage from the early tiers to the late ones: Ricochet Shot to 30%, Serrated Shot lighter, Marked
    Quarry's arrows to 60% and Piercing Volley to 300%.
  - Pass 5 trimmed Barbed Edges to 5/10% and Precision Shot to 60%.
  - Pass 6 found the base path even at 20, so 20's surplus came from the tree's first 11 points. It took Precision
    Shot's and Serrated Shot's flat per-level damage out, gave them more ranged attack power instead, and cut Quick
    Venom to 20%. Flat amounts are a third of their damage at 20 and under a fifth at 60.
  - Pass 7 put damage back in the deep tiers after 60 fell to -11%: Piercing Volley 400%, Marked Quarry 80%, Heavy
    Draw 15/30%, Precision Shot 80% and Ricochet Shot 45%.
  - Pass 7 passes at 20 (-0.3%) and 40 (-0.1%), and 60 sits at -1.8% ± 3.2% over 32 fights.
- **Noise between runs.** The same build at 60 ran from -11% to +3% between passes. Part of that is the passes, and
  part is the runs themselves: each run rolls the plain bot a new pet (Claw or Bite) and new gear, and Precision Shot
  swung from 1.6k to 4.4k a run as Telling Blow and Skirmish changed how Advantage was spent. A 16-fight margin of ±4%
  understates it. Settle close calls with 32 fights or repeat runs.
- **What carries over to the next trees.** Scale new damage from attack or spell power rather than per-level points.
  Keep the passives this dummy flatters (armor ignore, "your DoT is up", "above 80% health") deep in the tree. Expect
  the plain spec to gain most of its damage late.
- **Bot and harness faults found here, fixed in the playerbots fork:**
  - The tree's strategy shared a sibling group with the path strategies, so adding "windrunner" removed "ranger" and
    the other way round, and the bot never cast a Windrunner spell. Trees now have their own strategy context.
  - The harness cloned the plain bot's bow onto the path bot but not its arrows; the path bot stood idle with the
    wrong ammo. It now stocks ammo after the clone.
  - Serrated Shot's debuff trigger asked for a target expected to live a while, which a training dummy never
    reports, so it was never cast. It now checks only the aura.
  - Hunter bots without Explosive Shot never cast Arcane Shot: an armor penetration check meant for Survival
    stopped everyone else. It now applies only with Explosive Shot. That raises every Marksmanship and Beast Mastery
    hunter bot, and the plain side of every row before 2026-11-20.
