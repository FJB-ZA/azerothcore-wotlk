# Sub-class paths

Each path has its own folder here, and its world SQL is in `data/sql/custom/db_world/subclasses/`. The shared code
that runs every path (`subclass_paths.cpp`, `SubclassPaths.h` and the rest) is one folder up, in `Custom/`.

## Adding a path

1. `data/sql/custom/db_world/subclasses/<date>_<path>_path.sql`: the `subclass_path` row (with `talent_tab` for its
   fourth tree and `bot_share` for bots), its `subclass_spell_swap` rows, and the `spell_proc`, `spell_bonus_data`
   and `spell_script_names` rows its spells need.
2. `<Path>/<path>_spells.cpp`: spell scripts the data can't express, with an `AddSC_<path>_spells()` that
   `custom_script_loader.cpp` calls. Leave it out if the path needs none.
3. In solo-wow, the spells and the talent tree go in `patches/client/spell-data/subclasses/<path>.json`.
4. In mod-playerbots, bot support goes in `src/Ai/Subclass/<Path>/` (see its README).

Sources and SQL in subfolders are picked up without CMake or updater changes.
