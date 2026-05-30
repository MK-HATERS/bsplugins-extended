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
|---|---|
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

### LOOT Integration
LOOT sorting is fully supported for Starfield (re-enabled in LOOT v0.29.0). The LOOT report analysis — dirty/clean plugin info, incompatibilities, missing masters — is displayed inline in the plugin list tooltip. Blueprint and medium plugins are handled transparently by the LOOT library; no special configuration is required.

---

## Build Compatibility

- Built against the **latest MO2 uibase** (post-2.5.2, cmake_common v2)
- Requires MO2 **2.5.2 or later**
- Supports: Oblivion, Fallout 3, Fallout NV, Skyrim, Skyrim SE, Enderal, Enderal SE, Fallout 4, Skyrim VR, Fallout 4 VR, **Starfield**
