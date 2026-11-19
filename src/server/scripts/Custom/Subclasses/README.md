# Sub-class paths

| Path | Class | Folder | IDs |
|---|---|---|---|
| 1 Gladiator | Warrior | `Gladiator/` | 990100-990299, talent tab 990 |
| 2 Necromancer | Warlock | `Necromancer/` | 990300-990499 in every table (spells, creatures, NPCs, quests, items, texts) |
| 3 Ranger | Hunter | `Ranger/` | 990500-990599 in every table (the Starcaller has 990600-990699); no pet (`subclass_path.pets` = 0) |
| 4 Witch Doctor | Shaman | `WitchDoctor/` | 990700-990899 in every table; wards are totems in other bodies |
| 5 Starcaller | Druid | `Starcaller/` | spells 990600-990699 (the Ranger's block's second half), everything else 990650-990699 |

Each path has its own folder here, and its world SQL is in `data/sql/custom/db_world/subclasses/`. The shared code
that runs every path (`subclass_paths.cpp`, `SubclassPaths.h` and the rest) is one folder up, in `Custom/`.

## Adding a path

1. `data/sql/custom/db_world/subclasses/<date>_<path>_path.sql`: the `subclass_path` row (with `talent_tab` for its
   fourth tree and `bot_share` for bots), its `subclass_spell_swap` rows, and the `spell_proc`, `spell_bonus_data`
   and `spell_script_names` rows its spells need. A swap whose replacement is a talent in the path's tree (Gladiator
   Stance for Battle Stance) takes the original away only once the slot has learned that talent. A spell the path
   takes away with nothing in its place goes in `subclass_spell_remove` (the Ranger's pet spells), one it gives on
   top of the class's in `subclass_spell_grant` (Ranger's Focus), and `subclass_path.pets` = 0 keeps the class's
   pets in the stable while a slot follows the path.
   A spell a tree talent teaches can have higher ranks (a `spell_ranks` chain from the talent's spell, which
   solo-wow's spell data writes): a slot on the path gets each rank from that rank's `SpellLevel` on while it holds
   the rank below, and loses it when it no longer qualifies, so the spell scales like the class's own (Blight's
   Grave Worms). Its `spell_script_names` and `spell_proc` rows then take the first rank's negative id.
   A `subclass_creature_swap` row names a creature the path summons in place of the class's: a pet (the
   Necromancer's minions take the demon's stats) or a totem (the Witch Doctor's wards keep their own model, where
   the core would give a player's totem its race's).
2. `<Path>/<path>_spells.cpp`: spell scripts the data can't express, with an `AddSC_<path>_spells()` that
   `custom_script_loader.cpp` calls. Leave it out if the path needs none.
3. In solo-wow, the spells and the talent tree go in `patches/client/spell-data/subclasses/<path>.json`.
4. In mod-playerbots, bot support goes in `src/Ai/Subclass/<Path>/` (see its README).
5. Optional: gear and glyphs in solo-wow's `subclasses/<path>-gear.json` with their procs in
   `<date>_<path>_gear.sql`, and retail-style charges for a spell as a `solo_wow_spell_charges` row
   (`spell_charges.cpp`; the spell's DBC cooldown must be 0).

Sources and SQL in subfolders are picked up without CMake or updater changes.
