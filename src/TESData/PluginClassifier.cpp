#include "PluginClassifier.h"
#include "FileInfo.h"
#include "MOPlugin/BSPluginsINI.h"
#include "TESFile/Stream.h"

#include <QFileInfo>
#include <QSettings>

using namespace Qt::Literals::StringLiterals;

namespace TESData
{

// ---------------------------------------------------------------------------
// Known framework masters → group name.
// When a plugin lists one of these as a master, it is almost certainly a
// patch/addon for that framework and should share its group.
// ---------------------------------------------------------------------------
struct FrameworkEntry
{
  const char* master;
  PluginZone  zone;
};

static const FrameworkEntry kFrameworks[] = {
  // Starfield
  {"Starfield.esm",                        PluginZone::Patches},

  // Skyrim / generic BGS
  {"Unofficial Skyrim Special Edition Patch.esp", PluginZone::Patches},
  {"SkyUI_SE.esp",                         PluginZone::Patches},
  {"RaceMenu.esp",                         PluginZone::Frameworks},

  // Lighting frameworks (Lux ecosystem)
  {"Lux - Master plugin.esm",              PluginZone::Visuals},
  {"Lux Orbis - Master plugin.esm",        PluginZone::Visuals},
  {"Lux Via.esp",                          PluginZone::Visuals},
  {"Embers XD.esm",                        PluginZone::Visuals},
  {"Water for ENB.esm",                    PluginZone::Visuals},

  // World overhauls
  {"JKs Skyrim.esp",                       PluginZone::WorldChanges},
  {"LegacyoftheDragonborn.esm",            PluginZone::NPCsContent},
  {"Northern Roads.esp",                   PluginZone::WorldChanges},
  {"Landscape and Water Fixes.esp",        PluginZone::WorldChanges},

  // Follower / NPC frameworks
  {"Kaidan 2.esp",                         PluginZone::NPCsContent},
  {"3DNPC.esp",                            PluginZone::NPCsContent},
  {"VIGILANT.esm",                         PluginZone::NPCsContent},
};

// ---------------------------------------------------------------------------
// Record type dominance thresholds — what % of records must be this type
// for it to drive classification.
// ---------------------------------------------------------------------------
static constexpr float kDominantThreshold  = 0.40f;  // 40% = dominant
static constexpr float kSignificantThreshold = 0.20f; // 20% = significant signal

// Type code helpers (all uppercase 4-char codes from TES format)
static constexpr quint32 type(const char s[5])
{
  return static_cast<quint32>(s[0])        |
         static_cast<quint32>(s[1]) <<  8  |
         static_cast<quint32>(s[2]) << 16  |
         static_cast<quint32>(s[3]) << 24;
}

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

Classification classifyPlugin(const FileInfo& plugin, const QString& blueprintPrefix)
{
  // Return cached result if still valid (invalidated when priority changes)
  if (plugin.m_Metadata.classificationCached) {
    Classification cached;
    cached.zone       = static_cast<PluginZone>(plugin.m_Metadata.cachedZone);
    cached.confidence = plugin.m_Metadata.cachedConfidence;
    cached.groupName  = plugin.m_Metadata.cachedGroupName;
    cached.reason     = plugin.m_Metadata.cachedReason;
    return cached;
  }

  Classification result;

  // Helper to cache and return
  const auto cacheAndReturn = [&](Classification& r) -> Classification& {
    plugin.m_Metadata.classificationCached = true;
    plugin.m_Metadata.cachedZone           = static_cast<int>(r.zone);
    plugin.m_Metadata.cachedConfidence     = r.confidence;
    plugin.m_Metadata.cachedGroupName      = r.groupName;
    plugin.m_Metadata.cachedReason         = r.reason;
    return r;
  };

  // --- 0. Mod-author .bs hint (highest priority of all) ------------------
  if (plugin.hasBsHint()) {
    result.groupName  = plugin.bsGroupHint();
    result.confidence = 95;
    result.reason     = u"Mod author hint (.bs file)"_s;
    // Map the zone string to our enum; fall back to the hint's zone name as group
    // if the zone doesn't match a known name (allows custom zones too)
    const QString& z = plugin.bsZoneHint();
    if      (z.compare(u"Frameworks"_s,   Qt::CaseInsensitive) == 0) result.zone = PluginZone::Frameworks;
    else if (z.compare(u"World Changes"_s,Qt::CaseInsensitive) == 0) result.zone = PluginZone::WorldChanges;
    else if (z.compare(u"Gameplay"_s,     Qt::CaseInsensitive) == 0) result.zone = PluginZone::Gameplay;
    else if (z.compare(u"NPCs & Content"_s,Qt::CaseInsensitive)==0) result.zone = PluginZone::NPCsContent;
    else if (z.compare(u"Visuals"_s,      Qt::CaseInsensitive) == 0) result.zone = PluginZone::Visuals;
    else if (z.compare(u"Patches"_s,      Qt::CaseInsensitive) == 0) result.zone = PluginZone::Patches;
    else result.zone = PluginZone::Visuals;  // default zone for unknown hint
    return cacheAndReturn(result);
  }

  // --- 0b. User-defined custom group rules --------------------------------
  const auto customGroups = MOPlugin::pluginINI().customGroups();
  for (const auto& cg : customGroups) {
    if (cg.recordTypes.isEmpty()) continue;  // record filter needed for matching
    const auto& hist = plugin.recordTypeHistogram();
    if (hist.isEmpty()) continue;
    int total = 0;
    for (int v : hist) total += v;
    int matched = 0;
    for (const QString& rt : cg.recordTypes) {
      const QByteArray ba = rt.toLatin1();
      if (ba.size() < 4) continue;
      const quint32 key = quint32(ba[0]) | quint32(ba[1]) << 8 |
                          quint32(ba[2]) << 16 | quint32(ba[3]) << 24;
      matched += hist.value(key, 0);
    }
    if (total > 0 && matched * 100 / total >= cg.threshold) {
      result.groupName  = cg.name;
      result.confidence = 85;
      result.reason     = u"Custom group rule (%1%% match)"_s.arg(matched * 100 / total);
      // Map zone string to enum
      const QString& z = cg.zone;
      if      (z == u"Frameworks"_s)    result.zone = PluginZone::Frameworks;
      else if (z == u"World Changes"_s) result.zone = PluginZone::WorldChanges;
      else if (z == u"Gameplay"_s)      result.zone = PluginZone::Gameplay;
      else if (z == u"NPCs & Content"_s)result.zone = PluginZone::NPCsContent;
      else if (z == u"Visuals"_s)       result.zone = PluginZone::Visuals;
      else if (z == u"Patches"_s)       result.zone = PluginZone::Patches;
      else                              result.zone = PluginZone::Visuals;
      return cacheAndReturn(result);
    }
  }

  // --- 1. Force-loaded = Core (game manages position) --------------------
  if (plugin.forceLoaded()) {
    result.zone       = PluginZone::Core;
    result.confidence = 100;
    result.groupName  = u"Core"_s;
    result.reason     = u"Force-loaded by the game"_s;
    return cacheAndReturn(result);
  }

  // --- 2. Blueprint plugin -----------------------------------------------
  if (!blueprintPrefix.isEmpty() &&
      (plugin.isBlueprintFlagged() || plugin.isBlueprintPrefixed())) {
    result.zone       = PluginZone::Patches;
    result.confidence = 95;
    result.groupName  = u"Blueprints"_s;
    result.reason     = u"Blueprint plugin auto-loaded with its paired main plugin"_s;
    return cacheAndReturn(result);
  }

  // --- 3. Archive loader (dummy plugin + archives, no real records) -------
  if (plugin.hasNoRecords() && !plugin.archives().empty()) {
    result.zone          = PluginZone::Visuals;
    result.confidence    = 90;
    result.groupName     = u"Archive Loaders"_s;
    result.reason        = u"No records — exists to load BSA/BA2 archive(s)"_s;
    result.isArchiveLoader = true;
    return cacheAndReturn(result);
  }

  // --- 4. Framework patch detection (masters list) -----------------------
  const auto& masterList = plugin.masters();
  for (const auto& fw : kFrameworks) {
    const QString fwMaster = QString::fromLatin1(fw.master);
    if (masterList.contains(fwMaster, Qt::CaseInsensitive)) {
      result.zone       = fw.zone;
      result.confidence = 85;
      // Use user's custom zone name from INI, not the framework's hardcoded name
      result.groupName  = zoneGroupName(fw.zone);
      result.reason     = u"Masters %1"_s.arg(fwMaster);
      return cacheAndReturn(result);
    }
  }

  // --- 5. Record type histogram analysis ---------------------------------
  const auto& hist = plugin.recordTypeHistogram();
  if (!hist.isEmpty()) {
    int total = 0;
    for (int v : hist) total += v;

    const float fTotal = static_cast<float>(total);

    // Helpers to get fraction of a record type
    auto frac = [&](quint32 t) -> float {
      return hist.value(t, 0) / fTotal;
    };

    // Lighting mod: LIGH dominant
    if (frac(type("LIGH")) > kSignificantThreshold) {
      result.zone       = PluginZone::Visuals;
      result.confidence = 75;
      result.groupName  = u"Lighting"_s;
      result.reason     = u"Majority LIGH records (lighting data)"_s;
      return cacheAndReturn(result);
    }

    // Outfit / appearance: ARMO or CLOT dominant
    const float armoClot = frac(type("ARMO")) + frac(type("CLOT"));
    if (armoClot > kDominantThreshold) {
      result.zone       = PluginZone::Visuals;
      result.confidence = 70;
      result.groupName  = u"Outfits & Armor"_s;
      result.reason     = u"Dominant ARMO/CLOT records (outfits)"_s;
      return cacheAndReturn(result);
    }

    // Texture sets only → visual replacer
    if (frac(type("TXST")) > kDominantThreshold) {
      result.zone          = PluginZone::Visuals;
      result.confidence    = 80;
      result.groupName     = u"Texture Replacers"_s;
      result.reason        = u"Dominant TXST records (texture sets)"_s;
      result.isArchiveLoader = true;
      return cacheAndReturn(result);
    }

    // World: landscape / navmesh / world space
    const float worldSig = frac(type("WRLD")) + frac(type("LAND")) +
                           frac(type("LTEX")) + frac(type("CELL"));
    if (worldSig > kDominantThreshold) {
      result.zone       = PluginZone::WorldChanges;
      result.confidence = 70;
      result.groupName  = u"World Changes"_s;
      result.reason     = u"Dominant WRLD/LAND/CELL records (world edits)"_s;
      return cacheAndReturn(result);
    }

    // NPC / character
    const float npcSig = frac(type("NPC_")) + frac(type("RACE")) + frac(type("HDPT"));
    if (npcSig > kSignificantThreshold) {
      result.zone       = PluginZone::NPCsContent;
      result.confidence = 70;
      result.groupName  = u"NPCs & Characters"_s;
      result.reason     = u"Significant NPC_/RACE records (character edits)"_s;
      return cacheAndReturn(result);
    }

    // Quest / story content
    const float questSig = frac(type("QUST")) + frac(type("DIAL")) + frac(type("SCEN"));
    if (questSig > kSignificantThreshold) {
      result.zone       = PluginZone::NPCsContent;
      result.confidence = 70;
      result.groupName  = u"Quests & Content"_s;
      result.reason     = u"Significant QUST/DIAL/SCEN records (story content)"_s;
      return cacheAndReturn(result);
    }

    // Gameplay: perks, spells, economy
    const float gameplaySig = frac(type("PERK")) + frac(type("AVIF")) +
                              frac(type("ALCH")) + frac(type("SPEL")) +
                              frac(type("GMST"));
    if (gameplaySig > kSignificantThreshold) {
      result.zone       = PluginZone::Gameplay;
      result.confidence = 65;
      result.groupName  = u"Gameplay"_s;
      result.reason     = u"Significant perk/spell/game-setting records"_s;
      return cacheAndReturn(result);
    }

    // Weapons / combat
    const float weapSig = frac(type("WEAP")) + frac(type("AMMO")) + frac(type("PROJ"));
    if (weapSig > kSignificantThreshold) {
      result.zone       = PluginZone::Gameplay;
      result.confidence = 65;
      result.groupName  = u"Weapons & Combat"_s;
      result.reason     = u"Significant WEAP/AMMO records"_s;
      return cacheAndReturn(result);
    }
  }

  // --- 6. Name keyword fallback ------------------------------------------
  const QString name = QFileInfo(plugin.name()).completeBaseName().toLower();

  if (name.contains(u"patch") || name.contains(u"compat") || name.contains(u"fix")) {
    result.zone       = PluginZone::Patches;
    result.confidence = 40;
    result.groupName  = u"Patches"_s;
    result.reason     = u"Name contains patch/fix keyword"_s;
    return cacheAndReturn(result);
  }
  if (name.contains(u"light") || name.contains(u"lighting") || name.contains(u"lux")) {
    result.zone       = PluginZone::Visuals;
    result.confidence = 35;
    result.groupName  = u"Lighting"_s;
    result.reason     = u"Name suggests lighting mod"_s;
    return cacheAndReturn(result);
  }
  if (name.contains(u"skin") || name.contains(u"outfit") || name.contains(u"armor") ||
      name.contains(u"texture")) {
    result.zone       = PluginZone::Visuals;
    result.confidence = 35;
    result.groupName  = u"Visuals"_s;
    result.reason     = u"Name suggests visual mod"_s;
    return cacheAndReturn(result);
  }
  if (name.contains(u"follower") || name.contains(u"companion") ||
      name.contains(u"npc")) {
    result.zone       = PluginZone::NPCsContent;
    result.confidence = 35;
    result.groupName  = u"NPCs & Characters"_s;
    result.reason     = u"Name suggests NPC/follower mod"_s;
    return cacheAndReturn(result);
  }

  // --- 7. Unknown — needs user review -----------------------------------
  result.zone       = PluginZone::Unknown;
  result.confidence = 0;
  result.groupName  = u"Unclassified"_s;
  result.reason     = u"Could not determine category"_s;
  return result;
}

QString zoneName(PluginZone zone)
{
  switch (zone) {
  case PluginZone::Core:         return u"Core"_s;
  case PluginZone::Frameworks:   return u"Frameworks"_s;
  case PluginZone::WorldChanges: return u"World Changes"_s;
  case PluginZone::Gameplay:     return u"Gameplay"_s;
  case PluginZone::NPCsContent:  return u"NPCs & Content"_s;
  case PluginZone::Visuals:      return u"Visuals"_s;
  case PluginZone::Patches:      return u"Patches"_s;
  case PluginZone::Unknown:      return u"Unclassified"_s;
  }
  return u"Unknown"_s;
}

QString zoneGroupName(PluginZone zone)
{
  // Read user-customised names from the INI; fall back to built-in defaults
  // if the INI hasn't been loaded yet (e.g. during unit tests).
  const auto& ini = MOPlugin::pluginINI();
  switch (zone) {
  case PluginZone::Core:         return u"Core"_s;
  case PluginZone::Frameworks:   return ini.groupNameFrameworks();
  case PluginZone::WorldChanges: return ini.groupNameWorld();
  case PluginZone::Gameplay:     return ini.groupNameGameplay();
  case PluginZone::NPCsContent:  return ini.groupNameNPCs();
  case PluginZone::Visuals:      return ini.groupNameVisuals();
  case PluginZone::Patches:      return ini.groupNamePatches();
  case PluginZone::Unknown:      return u"Unclassified"_s;
  }
  return u"Unclassified"_s;
}

}  // namespace TESData
