#include "BSPluginsINI.h"

#include <log.h>

#include <QDir>
#include <QFileInfo>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Qt::Literals::StringLiterals;

namespace MOPlugin
{

// ---------------------------------------------------------------------------
// DLL self-location
// ---------------------------------------------------------------------------

// Marker function whose address is passed to GetModuleHandleEx so Windows
// tells us which DLL contains it — i.e. our own DLL.
static void dllAnchor() {}

QString BSPluginsINI::pluginDir()
{
  HMODULE hModule = nullptr;
  ::GetModuleHandleExW(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&dllAnchor), &hModule);

  wchar_t path[MAX_PATH] = {};
  ::GetModuleFileNameW(hModule, path, MAX_PATH);
  return QFileInfo(QString::fromWCharArray(path)).absolutePath();
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

BSPluginsINI& pluginINI()
{
  static BSPluginsINI instance;
  return instance;
}

// ---------------------------------------------------------------------------
// Load / migrate
// ---------------------------------------------------------------------------

bool BSPluginsINI::load()
{
  const QString path = iniPath();

  // Ensure the directory exists (first install)
  QDir().mkpath(QFileInfo(path).absolutePath());

  m_Settings = new QSettings(path, QSettings::IniFormat);
  m_Settings->setParent(nullptr);  // unparented, lives until plugin unloads

  const int savedSchema = m_Settings->value(u"Meta/schema_version"_s, 0).toInt();

  if (savedSchema < SCHEMA_VERSION) {
    migrate(savedSchema);
    m_Settings->setValue(u"Meta/schema_version"_s, SCHEMA_VERSION);
    m_Settings->sync();
  }

  MOBase::log::debug("BSPluginsINI loaded from {}", path.toStdString());
  return m_GroupNamesMigrated;
}

void BSPluginsINI::migrate(int fromSchema)
{
  // v0 → v1: seed defaults for all new keys.
  // Group names that already exist are preserved unchanged.
  if (fromSchema < 1) {
    m_Settings->beginGroup(u"GroupNames"_s);
    const QStringList existing = m_Settings->allKeys();

    // Only write defaults for keys not already present (preserves user edits)
    const bool hadAny = !existing.isEmpty();
    auto seed = [&](const QString& key, const QString& def) {
      if (!existing.contains(key)) {
        m_Settings->setValue(key, def);
      }
    };
    seed(u"patches"_s,       u"Patches"_s);
    seed(u"visuals"_s,       u"Visuals"_s);
    seed(u"world_changes"_s, u"World Changes"_s);
    seed(u"gameplay"_s,      u"Gameplay"_s);
    seed(u"npcs_content"_s,  u"NPCs & Content"_s);
    seed(u"frameworks"_s,    u"Frameworks"_s);
    seed(u"archive"_s,       u"Archive Loaders"_s);
    m_Settings->endGroup();

    // If there were already group names, tell the UI they were preserved
    if (hadAny) {
      m_GroupNamesMigrated = true;
    }
  }

  // Future migrations go here as: if (fromSchema < 2) { ... }
}

// ---------------------------------------------------------------------------
// Group names
// ---------------------------------------------------------------------------

QString BSPluginsINI::groupNamePatches()    const
{
  return m_Settings->value(u"GroupNames/patches"_s,       u"Patches"_s).toString();
}
QString BSPluginsINI::groupNameVisuals()    const
{
  return m_Settings->value(u"GroupNames/visuals"_s,       u"Visuals"_s).toString();
}
QString BSPluginsINI::groupNameWorld()      const
{
  return m_Settings->value(u"GroupNames/world_changes"_s, u"World Changes"_s).toString();
}
QString BSPluginsINI::groupNameGameplay()   const
{
  return m_Settings->value(u"GroupNames/gameplay"_s,      u"Gameplay"_s).toString();
}
QString BSPluginsINI::groupNameNPCs()       const
{
  return m_Settings->value(u"GroupNames/npcs_content"_s,  u"NPCs & Content"_s).toString();
}
QString BSPluginsINI::groupNameFrameworks() const
{
  return m_Settings->value(u"GroupNames/frameworks"_s,    u"Frameworks"_s).toString();
}
QString BSPluginsINI::groupNameArchive()    const
{
  return m_Settings->value(u"GroupNames/archive"_s,       u"Archive Loaders"_s).toString();
}

void BSPluginsINI::setGroupNamePatches(const QString& v)
{
  m_Settings->setValue(u"GroupNames/patches"_s, v);       m_Settings->sync();
}
void BSPluginsINI::setGroupNameVisuals(const QString& v)
{
  m_Settings->setValue(u"GroupNames/visuals"_s, v);       m_Settings->sync();
}
void BSPluginsINI::setGroupNameWorld(const QString& v)
{
  m_Settings->setValue(u"GroupNames/world_changes"_s, v); m_Settings->sync();
}
void BSPluginsINI::setGroupNameGameplay(const QString& v)
{
  m_Settings->setValue(u"GroupNames/gameplay"_s, v);      m_Settings->sync();
}
void BSPluginsINI::setGroupNameNPCs(const QString& v)
{
  m_Settings->setValue(u"GroupNames/npcs_content"_s, v);  m_Settings->sync();
}
void BSPluginsINI::setGroupNameFrameworks(const QString& v)
{
  m_Settings->setValue(u"GroupNames/frameworks"_s, v);    m_Settings->sync();
}
void BSPluginsINI::setGroupNameArchive(const QString& v)
{
  m_Settings->setValue(u"GroupNames/archive"_s, v);       m_Settings->sync();
}

void BSPluginsINI::resetGroupNamesToDefaults()
{
  m_Settings->beginGroup(u"GroupNames"_s);
  m_Settings->setValue(u"patches"_s,       u"Patches"_s);
  m_Settings->setValue(u"visuals"_s,       u"Visuals"_s);
  m_Settings->setValue(u"world_changes"_s, u"World Changes"_s);
  m_Settings->setValue(u"gameplay"_s,      u"Gameplay"_s);
  m_Settings->setValue(u"npcs_content"_s,  u"NPCs & Content"_s);
  m_Settings->setValue(u"frameworks"_s,    u"Frameworks"_s);
  m_Settings->setValue(u"archive"_s,       u"Archive Loaders"_s);
  m_Settings->endGroup();
  m_Settings->sync();
}

// ---------------------------------------------------------------------------
// Classification
// ---------------------------------------------------------------------------

int  BSPluginsINI::patchThreshold()          const
{
  if (m_PatchThresholdCached) return m_PatchThresholdCache;
  m_PatchThresholdCache  = m_Settings->value(u"Classification/patch_threshold"_s, 20).toInt();
  m_PatchThresholdCached = true;
  return m_PatchThresholdCache;
}
bool BSPluginsINI::archiveDetectionEnabled() const
{
  return m_Settings->value(u"Classification/archive_detection"_s,       true).toBool();
}
void BSPluginsINI::setPatchThreshold(int v)
{
  m_Settings->setValue(u"Classification/patch_threshold"_s, v); m_Settings->sync();
  invalidatePatchThresholdCache();
}
void BSPluginsINI::setArchiveDetectionEnabled(bool v)
{
  m_Settings->setValue(u"Classification/archive_detection"_s, v); m_Settings->sync();
}

// ---------------------------------------------------------------------------
// Updates
// ---------------------------------------------------------------------------

QString BSPluginsINI::skipVersion()        const
{
  return m_Settings->value(u"Updates/skip_version"_s,        QString()).toString();
}
QString BSPluginsINI::lastCheckTimestamp() const
{
  return m_Settings->value(u"Updates/last_check"_s,          QString()).toString();
}
void BSPluginsINI::setSkipVersion(const QString& v)
{
  m_Settings->setValue(u"Updates/skip_version"_s, v); m_Settings->sync();
}
void BSPluginsINI::setLastCheckTimestamp(const QString& v)
{
  m_Settings->setValue(u"Updates/last_check"_s,    v); m_Settings->sync();
}
QString BSPluginsINI::nexusUrl() const
{
  return m_Settings->value(u"Updates/nexus_url"_s, QString()).toString();
}
void BSPluginsINI::setNexusUrl(const QString& v)
{
  m_Settings->setValue(u"Updates/nexus_url"_s, v); m_Settings->sync();
}

// ---------------------------------------------------------------------------
// Custom groups
// ---------------------------------------------------------------------------

QList<BSPluginsINI::CustomGroup> BSPluginsINI::customGroups() const
{
  if (m_CustomGroupsCached) return m_CustomGroupsCache;

  m_CustomGroupsCache.clear();
  const int count = m_Settings->value(u"CustomGroups/count"_s, 0).toInt();
  for (int i = 0; i < count; ++i) {
    const QString section = u"CustomGroup_%1"_s.arg(i);
    CustomGroup g;
    g.name        = m_Settings->value(u"%1/name"_s.arg(section)).toString();
    g.zone        = m_Settings->value(u"%1/zone"_s.arg(section)).toString();
    g.recordTypes = m_Settings->value(u"%1/records"_s.arg(section))
                        .toString().split(u',', Qt::SkipEmptyParts);
    g.threshold   = m_Settings->value(u"%1/threshold"_s.arg(section), 15).toInt();
    if (!g.name.isEmpty()) m_CustomGroupsCache.append(std::move(g));
  }
  m_CustomGroupsCached = true;
  return m_CustomGroupsCache;
}

void BSPluginsINI::setCustomGroups(const QList<CustomGroup>& groups)
{
  // Remove all existing custom group sections
  const int old = m_Settings->value(u"CustomGroups/count"_s, 0).toInt();
  for (int i = 0; i < old; ++i) {
    m_Settings->remove(u"CustomGroup_%1"_s.arg(i));
  }

  m_Settings->setValue(u"CustomGroups/count"_s, groups.size());
  for (int i = 0; i < groups.size(); ++i) {
    const QString s = u"CustomGroup_%1"_s.arg(i);
    m_Settings->setValue(u"%1/name"_s.arg(s),      groups.at(i).name);
    m_Settings->setValue(u"%1/zone"_s.arg(s),      groups.at(i).zone);
    m_Settings->setValue(u"%1/records"_s.arg(s),   groups.at(i).recordTypes.join(u','));
    m_Settings->setValue(u"%1/threshold"_s.arg(s), groups.at(i).threshold);
  }
  m_Settings->sync();
  invalidateCustomGroupCache();
}

void BSPluginsINI::addCustomGroup(const CustomGroup& g)
{
  auto list = customGroups();
  list.append(g);
  setCustomGroups(list);
}

void BSPluginsINI::removeCustomGroup(const QString& name)
{
  auto list = customGroups();
  list.erase(std::remove_if(list.begin(), list.end(),
                            [&](const CustomGroup& g) { return g.name == name; }),
             list.end());
  setCustomGroups(list);
}

// ---------------------------------------------------------------------------
// Lifecycle state
// ---------------------------------------------------------------------------

bool    BSPluginsINI::firstRunDone()       const
{
  return m_Settings->value(u"State/first_run_done"_s,    false).toBool();
}
QString BSPluginsINI::lastPluginVersion()  const
{
  return m_Settings->value(u"State/last_version"_s,      QString()).toString();
}
void BSPluginsINI::setFirstRunDone(bool v)
{
  m_Settings->setValue(u"State/first_run_done"_s, v); m_Settings->sync();
}
void BSPluginsINI::setLastPluginVersion(const QString& v)
{
  m_Settings->setValue(u"State/last_version"_s,   v); m_Settings->sync();
}

}  // namespace MOPlugin
