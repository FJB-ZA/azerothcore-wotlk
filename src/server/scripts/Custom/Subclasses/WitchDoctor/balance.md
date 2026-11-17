# Witch Doctor balance

Measured with the balance harness (`../BALANCE.md`): a plain shaman bot against a Witch Doctor bot of the same level,
talents and gear, each on its own Grandmaster's Training Dummy, 16 fights of 60 seconds after an uncounted warm-up
(`.balance start 4 <level> <spec> <spec> 16 60`, and the same with `swap 1`). The difference carries a margin of two
standard errors; the budget is 5%. Every Witch Doctor spell mirrors the shaman spell it replaces, so the path has no
dial: these runs check that the mirrors, wards and bot actions add up to the shaman.

Shamans are Horde only before TBC, so the runs stand an orc GM at the Crossroads (solo-wow's `tools/balance/runs.json`).
The two bots were a tauren (Ninla) and a troll (Reger); each run was made both ways round.

## Single target

| Date | Level | Spec | Scenario | Plain | Path | Difference | Damage taken | Pet share | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| 2026-11-17 | 20 | ele pve / ele pve | single target | 67.5 DPS | 67.2 DPS | -0.5% ± 2.5% | +0.0% | 16% / 17% | pass |
| 2026-11-17 | 20 | ele pve / ele pve (sides swapped) | single target | 64.9 DPS | 63.7 DPS | -1.8% ± 4.1% | +0.0% | 17% / 16% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-17 | 40 | ele pve / ele pve | single target | 198.3 DPS | 204.0 DPS | +2.9% ± 5.6% | +0.0% | 0% / 0% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-17 | 40 | ele pve / ele pve (sides swapped) | single target | 201.5 DPS | 198.4 DPS | -1.5% ± 6.5% | +0.0% | 0% / 0% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-17 | 60 | ele pve / ele pve | single target | 785.4 DPS | 826.7 DPS | +5.3% ± 7.4% | +0.0% | 0% / 0% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-17 | 60 | ele pve / ele pve (sides swapped) | single target | 798.3 DPS | 781.1 DPS | -2.2% ± 6.3% | +0.0% | 0% / 0% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-17 | 20 | enh pve / enh pve | single target | 31.1 DPS | 31.7 DPS | +2.1% ± 4.5% | +0.0% | 19% / 20% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-17 | 20 | enh pve / enh pve (sides swapped) | single target | 32.9 DPS | 32.7 DPS | -0.6% ± 3.3% | +0.0% | 21% / 22% | pass |
| 2026-11-17 | 40 | enh pve / enh pve | single target | 141.9 DPS | 136.5 DPS | -3.8% ± 6.9% | +0.0% | 19% / 20% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-17 | 40 | enh pve / enh pve (sides swapped) | single target | 143.5 DPS | 145.9 DPS | +1.7% ± 8.9% | +0.0% | 19% / 19% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-17 | 60 | enh pve / enh pve | single target | 850.3 DPS | 877.6 DPS | +3.2% ± 3.1% | +0.0% | 24% / 24% | inconclusive: too close to the budget for 16 fights; run more |
| 2026-11-17 | 60 | enh pve / enh pve (sides swapped) | single target | 840.0 DPS | 823.1 DPS | -2.0% ± 3.0% | +0.0% | 24% / 25% | pass |

In the unswapped rows the troll is the Witch Doctor; in the swapped rows the tauren is.

## Reading

- **Every pair averages inside 2% and none leans one way.** Averaging each run with its swapped run (which cancels
  the two characters): Elemental -1.2%, +0.7% and +1.6% at 20, 40 and 60; Enhancement +0.8%, -1.1% and +0.6%.
  Three runs pass outright. The rest are "inconclusive" only because a caster's crits and a melee shaman's Windfury
  swing from fight to fight, so the margin reaches past 5% in 16 fights.
- **The troll is worth a few points.** Whichever side the troll was on came out ahead at Elemental 40 and 60 (by
  about 2 and 4 points). Berserking is the likely reason. The swapped runs are there for exactly this.
- **The wards pull their weight.** As a share of each bot's damage the Serpent Ward matched Searing Totem within a
  point in every Enhancement run (19-25%) and at Elemental 20 (16-17%). Elemental bots at 40 and 60 put out no fire
  totem on either side, so those rows compare the spells alone.
- **A gap the runs found, fixed in the core:** Elemental Focus (and any proc with a school in `spell_proc`) never
  procced from a mirror of another school, so a Witch Doctor's shadow Voodoo Shock crits gave no Clearcasting. The
  actor's own procs now count a mirror as the school it replaces too (`SpellMgr::CanSpellTriggerProcOnEvent`), as
  its other caster bonuses already did. The six runs before the fix (2026-11-16) are in the raw balance file;
  they read within the noise of these, so the gap was too small to see in 16 fights, but it was a real difference.
- **Harness and bot faults found here, fixed in the playerbots fork:**
  - Shaman bots didn't handle wards: the totem triggers and actions looked for the shaman's spell IDs only. They
    now look for the known spell or its mirror, and a totem counts as present by its original creature's name too.
  - The setup check called a talent that teaches spells (Spirit Weapons, Dual Wield) unlearned, since the talent's
    own spell is never in the bot's spell book. It now counts the spells the talent teaches.
  - A human GM got no shaman bots, since `addclass` only adds its own faction's: the runs can now set the GM's race.
- Not measured yet: Restoration (the path doesn't touch healing), the pack (where Voodoo Ward and Spirit Nova earn
  their keep), duels, and levels 70 and 80.
