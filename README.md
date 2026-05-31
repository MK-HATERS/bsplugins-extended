# Bethesda Plugin Manager - Extended for Mod Organizer 2

Manages plugin load order for Bethesda Game Studios game engines.

A rework and recompilation of the base MO2 bsplugins addon with improved stability, bug fixes, and extended features — now updated for the **latest MO2** and with full **Starfield** plugin type support.

---

## What's Different From the Original

### Group Management

- Added `Reset Groups` — clears group structure without changing load order
- Added `Merge Group` — combines two groups in one action
- Added `Clean Groups` — removes empty groups automatically
- Added *Remove Group...* option in the right-click context menu
- Optional confirmation prompts before risky mass operations
- Improved drag-and-drop behaviour for full group moves
- Improved performance on large load orders
- Fixed several context menu and group action visibility issues
- Added option to fully disable the group system

### Load Order

- Restored the **Lock Load Order Position** option (removed in upstream bsplugins)
- Added restore behaviour for load order snapshots after external executable runs
- Added protection against unwanted external load order changes

---

## Starfield Plugin Type Support

This version implements the full plugin type rules researched by MO2 for Starfield.

### Blueprint Plugins (`blueprintships-*.esm`)

Blueprint plugins work like archives — when a regular plugin is loaded, its paired `blueprintships-<name>.esm` is automatically force-enabled alongside it.

- **Auto force-enable/disable** — blueprint plugins are force-disabled by default and activate automatically when their paired main plugin is enabled or disabled
- **Correct load order position** — blueprint plugins always sort to the end of the load order, after all non-blueprint plugins, matching game behaviour
- **Zone enforcement** — blueprint plugins cannot be moved into the non-blueprint section and vice versa, whether by drag-and-drop, keyboard shortcut, or any programmatic reorder
- **Link icon** in the flags column identifies blueprint plugins at a glance
- **CCC file support** — plugins manually added to the CCC file remain force-enabled regardless

**Four tooltip states** matching MO2's rules:

| State | Meaning |
| --- | --- |
| Force-disabled + flagged + prefixed | Must be enabled by a paired main plugin |
| Force-disabled + flagged + not prefixed | Not using the blueprint prefix — game can't load it |
| Force-disabled + prefixed + not flagged | No blueprint flag — invalid blueprint file |
| Enabled + prefixed + not flagged | ⚠ Unintended autoload, may cause ambiguous load order |

**Problem detection** — the warning icon is shown for blueprints in invalid states (flagged but wrongly named, or named like a blueprint but missing the flag).

### Medium Plugins (ESH, `0x400` flag)

Starfield introduced medium-sized plugins that sit between full plugins and ESL plugins. This is a Starfield-exclusive plugin type — not enabled for other games.

- Detected from the `0x400` TES4 header flag via `GamePlugins::mediumPluginsAreSupported()`
- Assigned **`FD:xxx`** index space (256 slots, parallel to ESL's `FE:xxx`)
- Flag icon and tooltip explaining ESH space and the 256-slot limit
- Warning shown if a plugin is incorrectly flagged as both light (ESL) and medium simultaneously

### Overlay Plugins (`0x200` flag)

- Overlay plugin detection was previously hardcoded to disabled — now correctly enabled for Starfield and Fallout 4
- Overlay plugins display `XX` in the mod index column (no record space consumed)

### Master-Child Zone Isolation

Following MO2's rules, master-dependency constraints are only enforced within the same blueprint zone. A regular master plugin does not affect the ordering of blueprint plugins and vice versa.

### ESL Force-Disable on Unsupported Games

If a `.esl` file is present for a game that does not support light plugins, it is automatically force-disabled (matching MO2 behaviour) rather than being treated as a regular plugin and potentially corrupting the load order.

### Plugin Count Breakdown

The active plugin counter tooltip now shows a full breakdown by type — ESMs, ESPs, ESHs, ESLs, Blueprint masters — with active and total counts for each, matching MO2's counter display. ESH and Blueprint rows only appear when the current game supports those plugin types.

### Intelligent Plugin Classification and Group Review

After every LOOT sort, a **Group Review** dialog appears with two tabs:

**Patch Order tab** — plugins that override many records from another mod without declaring it as a master are likely patches placed in the wrong position. The dialog shows which plugin patches which target, with a confidence rating (●●●●●), the mod origin name from the left panel, and a checkbox to confirm or skip each suggestion. Confirmed patches are moved just after their target and optionally grouped into a "Patches" group. On subsequent LOOT runs, already-correctly-ordered plugins and plugins already in named groups are silently skipped — the dialog only shows genuinely unresolved issues.

**Group Suggestions tab** — unclassified plugins (those still in "default") are automatically assigned to suggested groups using a 7-signal priority chain:

1. LOOT masterlist group assignment (highest trust)
2. Blueprint-prefixed plugins → Blueprints zone
3. No records + BA2 archive → Archive Loader (left panel note shown)
4. Masters list → framework ecosystem detection (e.g., "Lux", "LOTD", "JK's Skyrim", etc.)
5. Record type histogram (dominant record types from the conflict scan)
6. Name keywords (tiebreaker)
7. Unclassified → user review bucket

Already-grouped plugins, force-loaded game masters, and plugins assigned on a previous run are all skipped. Confidence dots (●●●●●) indicate classification certainty; only high-confidence suggestions are pre-checked.

**Archive Loader note** — plugins that exist only to load BSA/BA2 archives (no records or only TXST records) are flagged with a 📦 icon and a note that left panel (mod list) position controls which textures/meshes win, not plugin load order.

**Scales to large mod lists** — for a 3000-plugin load order only the unclassified bucket needs user attention; the rest auto-classifies in under a second. All group assignments persist in `plugingroups.txt` and are respected on future runs.

### Inferred Patch Detection and Smart Sort

Conflict analysis identifies when a plugin overrides many records from another plugin without declaring it as a formal master — a strong signal that it is a patch for that plugin and needs to load after it.

- **Warning tooltip** — on any plugin overriding ≥ 3 records from another plugin without mastering it: *"Overrides N record(s) from [Plugin X] — if this patches [Plugin X], ensure it loads after it"*
- **Problem flag** — warning icon shown when the count reaches ≥ 5 records (a likely accidental load order issue)
- **Apply Inferred Load Order** — right-click → All Items → *Apply inferred load order* runs a pass over all plugins and moves any that patch another plugin (≥ 3 shared records) to load after their inferred target; safe to run after a LOOT sort to catch patches not in the LOOT masterlist

This works entirely from record-level conflict data — no filename heuristics needed — and catches cases like two mods both editing the same NPC or weapon record where one is clearly a compatibility patch for the other.

### ONAM Conflict Detection

Navmesh, landscape, dialog, and scene record overrides are declared in the TES4 `ONAM` subrecord. bsplugins-extended reads this list and registers those overrides in the conflict system, so navmesh/landscape conflicts are detected without requiring the slow full CELL/WRLD group scan.

### ObjectID Range Validation

ESL plugins must keep record ObjectIDs ≤ `0xFFF`; ESH plugins must keep them ≤ `0xFF`. If a plugin violates these limits (indicating a broken Creation Kit export), the warning icon is shown and the tooltip explains the issue. The `nextObjectId` field in the HEDR subrecord is used as a fast pre-check.

### Plugin Header Version

The `headerVersion` field (the float at the start of the HEDR subrecord) is now parsed and exposed via `IPluginList::headerVersion()`. This returns `0.96` for Starfield plugins, `0.95` for FO4, and so on — correctly implemented instead of always returning `-1`.

### LOOT Integration

LOOT sorting is fully supported for Starfield (re-enabled in LOOT v0.29.0). The LOOT report analysis — dirty/clean plugin info, incompatibilities, missing masters — is displayed inline in the plugin list tooltip. Blueprint and medium plugins are handled transparently by the LOOT library; no special configuration is required.

---

## Credits

This project is a fork of work by several authors, all released under GPL v3.

- **[Exit-9B (Parapets)](https://github.com/Exit-9B/modorganizer-bsplugins)** — original bsplugins plugin; foundational architecture, conflict detection, group management, LOOT integration
- **[Alaxouche](https://github.com/Alaxouche/modorganizer-bsplugins-extended)** — bsplugins-extended fork; group tools, stability improvements, restored lock position option
- **MK-HATERS** — this fork; full Starfield plugin type support (blueprint, medium, overlay), latest MO2 compatibility, ONAM conflict detection, inferred patch analysis, smart sort

---

## Build Compatibility

- Built against the **latest MO2 uibase** (post-2.5.2, cmake_common v2)
- Requires MO2 **2.5.2 or later**
- Supports: Oblivion, Fallout 3, Fallout NV, Skyrim, Skyrim SE, Enderal, Enderal SE, Fallout 4, Skyrim VR, Fallout 4 VR, **Starfield**
