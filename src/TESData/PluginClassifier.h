#ifndef TESDATA_PLUGINCLASSIFIER_H
#define TESDATA_PLUGINCLASSIFIER_H

#include <QString>

namespace TESData
{

class FileInfo;

// Load-order zone a plugin belongs to.
// Zones are ordered — lower enum value = loads earlier.
enum class PluginZone
{
  Core        = 0,  // Force-loaded base game / DLC (not user-assigned)
  Frameworks  = 1,  // Libraries many other plugins depend on
  WorldChanges = 2, // Landscape, navmesh, weather, POI edits
  Gameplay    = 3,  // Mechanics, perks, economy, balance, game settings
  NPCsContent = 4,  // NPC edits, companions, quests, new locations
  Visuals     = 5,  // Lighting, texture sets, mesh replacers, outfit skins
  Patches     = 6,  // Compatibility patches for other mods
  Unknown     = 7,  // Needs user review
};

// Confidence that this classification is correct (0–100).
struct Classification
{
  PluginZone zone       = PluginZone::Unknown;
  int        confidence = 0;   // 0-100
  QString    groupName;        // suggested group name (may be framework-specific)
  QString    reason;           // short human-readable explanation
  bool       isArchiveLoader  = false; // plugin exists mainly to load a BA2/BSA
};

// Classify a single plugin using all available signals:
//   1. Force-loaded → Core
//   2. Masters list → framework patch detection
//   3. hasNoRecords + archives → archive loader
//   4. Record type histogram → dominant category
//   5. Name keywords → tie-breaker
Classification classifyPlugin(const FileInfo& plugin,
                              const QString&  blueprintPrefix);

// Human-readable zone label for display.
QString zoneName(PluginZone zone);

// Canonical group name for a zone (used when creating auto-groups).
QString zoneGroupName(PluginZone zone);

}  // namespace TESData

#endif  // TESDATA_PLUGINCLASSIFIER_H
