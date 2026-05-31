# BSPlugins Extended — Mod Organizer 2 Plugin Load Order Manager

A rework and recompilation of the MO2 `bsplugins` addon with full **Starfield** plugin type support, intelligent auto-classification, smart patch detection, and a built-in group review workflow — updated for the **latest MO2** (2.5.2+).

---

## Installing into MO2

1. Download `bsplugins.dll` and `bsplugins_en.qm` from the [Releases](../../releases) page.
2. Copy `bsplugins.dll` into your MO2 `plugins/bsplugins/` subfolder (create it if it doesn't exist).
3. Copy `bsplugins_en.qm` into `plugins/bsplugins/translations/`.
4. Launch MO2. The plugin loads automatically — no additional configuration needed.

> **Note:** Settings are saved to `plugins/bsplugins/settings.ini` alongside the DLL. This file is never overwritten by updates so your group names, custom groups, and thresholds survive upgrades.

---

## Quick Start

After installing:

1. **Enable it** — MO2 loads the plugin automatically. You'll see the Plugins panel as usual.
2. **Run LOOT** — click the Sort button. BSPlugins runs a post-sort Group Review dialog that classifies your plugins and flags unordered patches.
3. **Review suggestions** — confirm which patches to move and which groups to apply. Already-handled plugins are skipped on future runs.
4. **Done** — your load order now has meaningful groups (Patches, Visuals, World Changes, etc.) and patches load after their targets.

---

## What This Plugin Does

### Plugin List Panel

The main panel replaces MO2's built-in Plugins tab with an enhanced version that adds:

- A **compact tab strip** below the plugin list: **Info**, **Log**, and **Settings** tabs that give context without opening separate windows.
- **Patch Sort button** — a quick pass that reorders patches after you add 1–5 new mods, without running a full LOOT sort.
- **Plugin count tooltip** — clicking the counter shows a breakdown by type (ESM/ESP/ESL/ESH/Blueprint) with active vs. total counts.

### Info Tab

Select any plugin to see:

- **Mod origin** — which MO2 mod provides this plugin.
- **Type flags** — ESM, ESL, ESH, Overlay, Blueprint.
- **Classification** — auto-detected zone and group with a confidence indicator (●●●●● green/amber/gray scale) and the reasoning behind it.
- **Conflicts** — what this plugin overrides, what overrides it.
- **Inferred patch target** — if this plugin overrides many records from another without mastering it, the target is shown here.
- **Blueprint pair** — for blueprint plugins, the paired main plugin is shown.
- **Mod-author `.bs` hint** — if the mod author shipped a `.bs` hint file, its contents are shown with an `[Edit]` link.

### Log Tab

All BSPlugins-specific messages (warnings, classification decisions, update check results) appear here. Nothing is written to MO2's main log, keeping it clean.

### Settings Tab

- **Group Names** — rename any built-in zone group (Patches, Visuals, World Changes, Gameplay, NPCs & Content, Frameworks, Archive Loaders). Changes save immediately.
- **Custom Groups** — create groups with optional record-type detection rules. Custom groups are checked before built-in classification.
- **Patch threshold** — how many shared records are needed before a plugin is flagged as a likely patch.
- **Updates** — optional Nexus URL, manual update check.
- **Mod Author Tools** — generate a `.bs` hint file for the selected plugin.
- **Export** — copy a Markdown table of all plugins (group, zone, confidence, reason) to the clipboard for sharing or load-order help posts.

---

## Intelligent Plugin Classification

After every LOOT sort, a **Group Review** dialog appears with two tabs.

### Patch Order Tab

Plugins that override many records from another mod without declaring it as a master are likely patches in the wrong position. For each suggestion the dialog shows:

- Which plugin patches which target
- Confidence rating (●●●●●)
- The mod origin from the left panel
- Record count

Confirm to move the patch just after its target and optionally group it as "Patches". Already-correctly-ordered patches and plugins in named groups are silently skipped on future runs.

### Group Suggestions Tab

Unclassified plugins are automatically assigned to suggested groups using a **7-signal priority chain**:

| Priority | Signal | Confidence |
| -------- | ------ | ---------- |
| 0 | Mod-author `.bs` hint file | Up to 100% (author-set) |
| 0b | User custom group rules (record-type match) | 85% |
| 1 | Force-loaded by game (base game, DLC) | 100% → Core |
| 2 | Blueprint-flagged or blueprint-prefixed | 95% → Blueprints |
| 3 | No records + BA2 archive | 90% → Archive Loaders |
| 4 | Masters list framework detection | 85% |
| 5 | Record type histogram | 65–80% |
| 6 | Name keywords | 35–40% |
| 7 | Unknown — surfaces for manual review | 0% |

Confidence dots (●●●●●) colour-coded by confidence: green ≥70%, amber 40–69%, gray <40%. Only high-confidence suggestions (≥70%) are pre-checked.

### Custom Groups

Open **Group Manager** from the Settings tab to:

- Create custom groups with a name, zone, and optional record-type detection rules.
- Assign plugins by drag-and-drop or select + Move.
- Edit or delete existing custom groups.

The **Custom Group dialog** has Simple and Advanced modes:

- **Simple** — name + zone, with an explanation of where in the load order that zone sits.
- **Advanced** — add a record-type filter (e.g., "at least 30% LIGH records → Lighting group") with a live preview.

### Patch Sort (Quick Sort)

The **Patch Sort** button runs `applyInferredOrdering()` without running LOOT:

- Detects plugins overriding records from another without mastering it.
- Moves them to load after their inferred target using a bubble-sort pass (up to 3 passes for A→B→C chains).
- **Early-returns with no UI flicker** if the order is already correct — safe to click repeatedly.
- Threshold for detection is user-configurable in Settings.

---

## Starfield Plugin Type Support

### Blueprint Plugins (`blueprintships-*.esm`)

Blueprint plugins auto-enable with their paired main plugin.

- Force-disabled by default; activate automatically when their main plugin is toggled.
- Always sort to the end of the load order, after all non-blueprint plugins.
- Cannot be dragged across the blueprint zone boundary.
- Problem detection: warns if flagged-but-wrongly-named or named-but-missing-flag.

### Medium Plugins (ESH, `0x400` flag)

- Sit between full plugins and ESL plugins in the slot space.
- Assigned `FD:xxx` index space (256 slots, parallel to ESL's `FE:xxx`).
- Starfield-exclusive; not enabled for other games.

### Overlay Plugins (`0x200` flag)

- Display `XX` in the mod index column (no record space consumed).
- Correctly enabled for Starfield and Fallout 4.

### ObjectID Range Validation

- ESL plugins: ObjectIDs must be ≤ `0xFFF`
- ESH plugins: ObjectIDs must be ≤ `0xFF`
- Broken Creation Kit exports that violate these limits are flagged with the warning icon.

### ONAM Conflict Detection

Navmesh, landscape, dialog, and scene overrides declared in the TES4 `ONAM` subrecord are registered in the conflict system — these conflicts are detected without the slow full CELL/WRLD scan.

---

## Mod Author: `.bs` Hint Files

Ship a `.bs` file alongside your plugin to tell BSPlugins exactly where it belongs. Users with BSPlugins Extended installed will see your classification at 95% confidence (or higher if you set it), overriding all automated signals.

**Format** (plain UTF-8 text, no section headers):

```ini
group=Lux Compatibility Patches
zone=Patches
confidence=100
# confidence= is optional (1-100). Default: 95.
# Zones: Visuals, World Changes, Gameplay, NPCs & Content, Frameworks, Patches
```

Single-line shorthand (group name only):

```text
Lux Compatibility Patches
```

The `.bs` file must have the same base name as the plugin: `MyMod.esp` → `MyMod.esp.bs`. It is read from MO2's virtual file system so it can be in any mod folder that appears before the plugin in the left panel.

Use the **Mod Author Tools** section in the Settings tab to generate a template for the currently selected plugin.

---

## Advanced: Customising Classification

### Patch Detection Threshold

The **patch threshold** (Settings tab, default: 20) controls:

- How many shared records a plugin must override before it's flagged with the patch icon.
- Whether it appears pre-checked in the Group Review dialog.
- The lower "display threshold" for informational tooltips is automatically set to `threshold / 6`.

Lower the threshold for large patch-heavy load orders; raise it if you're seeing too many false positives.

### Custom Group Record Rules

In **Group Manager → New Group → Advanced**:

- Pick record types (ARMO, LIGH, NPC_, QUST, etc.) from a categorised, searchable list.
- Set a percentage threshold (e.g., "≥ 30% of records must be LIGH").
- Live preview shows how many of your current plugins would match.

Custom groups are checked at **85% confidence** — above name keywords and record histogram but below `.bs` hints.

### Export for Sharing

The **Copy group summary to clipboard** button in the Settings tab produces a Markdown table:

```markdown
| Plugin | Group | Zone | Confidence | Reason |
|--------|-------|------|------------|--------|
| MyMod.esp | Patches | Patches | 95% | Mod author hint (.bs file) |
...
```

Paste this into Nexus comments, Reddit posts, or a text file to diff before/after sorting.

---

## What Changed From the Original bsplugins

The original [`modorganizer-bsplugins`](https://github.com/Exit-9B/modorganizer-bsplugins) by Parapets was a solid foundation for pre-Starfield games. [`modorganizer-bsplugins-extended`](https://github.com/Alaxouche/modorganizer-bsplugins-extended) by Alaxouche added group tools and stability fixes.

This fork adds on top of both:

| Feature | Original | This Fork |
| ------- | -------- | --------- |
| Blueprint plugins | ✗ | ✓ Full auto-enable/disable and zone isolation |
| Medium plugins (ESH) | ✗ | ✓ FD:xxx slot space, flag detection |
| Overlay plugins | Hardcoded off | ✓ Enabled for Starfield + FO4 |
| Intelligent classification | ✗ | ✓ 7-signal chain with record histogram |
| Group Review dialog | ✗ | ✓ Post-LOOT patch + group suggestions |
| Inferred patch detection | ✗ | ✓ Conflict-driven, no filename guessing |
| Patch Sort button | ✗ | ✓ Quick sort without LOOT |
| `.bs` hint files | ✗ | ✓ Mod-author group hints with confidence |
| Custom groups | ✗ | ✓ Record-type rules, zone assignment |
| Group Manager window | ✗ | ✓ Floating dialog, drag-and-drop |
| Info / Log / Settings tabs | ✗ | ✓ Inline context panel below plugin list |
| Plugin list export | ✗ | ✓ Markdown table to clipboard |
| ONAM conflict detection | ✗ | ✓ Fast navmesh/landscape conflicts |
| ObjectID range validation | ✗ | ✓ ESL/ESH broken-export warning |
| Plugin header version | Always -1 | ✓ Correctly parsed from HEDR |
| Update checker | ✗ | ✓ GitHub/Nexus version check on startup |
| Lock load order position | Removed upstream | ✓ Restored |
| Group reset / merge / clean | ✗ | ✓ Mass group operations |
| Latest MO2 cmake_common | ✗ | ✓ `mo2_configure_plugin`, vcpkg manifest |

---

## Build Compatibility

- Requires MO2 **2.5.2 or later**
- Built with **Qt 6**, **C++23**, **MSVC 2022**
- vcpkg dependencies: `boost`, `fmt`, `zlib`, `lz4`, `mo2-dds-header`
- Supports: Oblivion · Fallout 3 · Fallout NV · Skyrim · Skyrim SE · Enderal · Enderal SE · Fallout 4 · Skyrim VR · Fallout 4 VR · **Starfield**

---

## Credits

Released under GPL v3. Based on work by:

- **[Exit-9B (Parapets)](https://github.com/Exit-9B/modorganizer-bsplugins)** — original bsplugins; foundational architecture, conflict detection, group management, LOOT integration
- **[Alaxouche](https://github.com/Alaxouche/modorganizer-bsplugins-extended)** — group tools, stability improvements, restored lock position option
- **MK-HATERS** — this fork; Starfield support, intelligent classification, smart sort, mod author tooling
