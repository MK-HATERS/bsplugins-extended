#ifndef MOPLUGIN_BSPLUGINSINI_H
#define MOPLUGIN_BSPLUGINSINI_H

#include <QSettings>
#include <QString>

namespace MOPlugin
{

// Persistent INI-backed settings stored in the plugin's own subfolder
// (alongside bsplugins.dll). Survives plugin updates — the INI is never
// overwritten by the installer since it's user-created on first run.
//
// Schema versioning: when settings keys change in a major update we increment
// SCHEMA_VERSION, run migrate(), preserve group name customizations, and set
// wasGroupNamesMigrated() so the UI can warn the user.
class BSPluginsINI final
{
public:
  static constexpr int SCHEMA_VERSION = 1;

  // Returns the directory that contains bsplugins.dll (our subfolder).
  // Resolved via GetModuleFileName at runtime — works regardless of install path.
  [[nodiscard]] static QString pluginDir();

  // Returns path to settings.ini inside the plugin subfolder.
  [[nodiscard]] static QString iniPath() { return pluginDir() + QStringLiteral("/settings.ini"); }

  // Returns path to the backup directory inside the plugin subfolder.
  [[nodiscard]] static QString backupDir() { return pluginDir() + QStringLiteral("/backup"); }

  // Load / create the INI. Returns true if a schema migration ran that
  // the user should be told about (group names were preserved).
  bool load();

  [[nodiscard]] bool wasGroupNamesMigrated() const { return m_GroupNamesMigrated; }

  // ---- Group names (user-customisable, always preserved on migration) ------
  [[nodiscard]] QString groupNamePatches()      const;
  [[nodiscard]] QString groupNameVisuals()      const;
  [[nodiscard]] QString groupNameWorld()        const;
  [[nodiscard]] QString groupNameGameplay()     const;
  [[nodiscard]] QString groupNameNPCs()         const;
  [[nodiscard]] QString groupNameFrameworks()   const;
  [[nodiscard]] QString groupNameArchive()      const;

  void setGroupNamePatches(const QString& v);
  void setGroupNameVisuals(const QString& v);
  void setGroupNameWorld(const QString& v);
  void setGroupNameGameplay(const QString& v);
  void setGroupNameNPCs(const QString& v);
  void setGroupNameFrameworks(const QString& v);
  void setGroupNameArchive(const QString& v);

  void resetGroupNamesToDefaults();

  // ---- Classification settings --------------------------------------------
  [[nodiscard]] int  patchThreshold()          const;
  [[nodiscard]] bool archiveDetectionEnabled() const;

  void setPatchThreshold(int v);
  void setArchiveDetectionEnabled(bool v);

  // ---- Update tracking ----------------------------------------------------
  [[nodiscard]] QString skipVersion()           const;
  [[nodiscard]] QString lastCheckTimestamp()    const;
  [[nodiscard]] QString nexusUrl()              const;

  void setSkipVersion(const QString& v);
  void setLastCheckTimestamp(const QString& v);
  void setNexusUrl(const QString& v);

  // ---- Custom groups -------------------------------------------------------
  // Each custom group maps to a zone and optional record-type filter.
  struct CustomGroup
  {
    QString name;
    QString zone;                // zone name as shown in Settings
    QStringList recordTypes;     // 4-char codes, empty = no record filter
    int     threshold = 15;      // minimum % of matching records
  };

  [[nodiscard]] QList<CustomGroup> customGroups() const;
  void setCustomGroups(const QList<CustomGroup>& groups);
  void addCustomGroup(const CustomGroup& g);
  void removeCustomGroup(const QString& name);

  // ---- Plugin lifecycle state (first run, version seen) -------------------
  [[nodiscard]] bool    firstRunDone()          const;
  [[nodiscard]] QString lastPluginVersion()     const;

  void setFirstRunDone(bool v);
  void setLastPluginVersion(const QString& v);

private:
  void migrate(int fromSchema);
  void invalidateCustomGroupCache() { m_CustomGroupsCached = false; }
  void invalidatePatchThresholdCache() { m_PatchThresholdCached = false; }

  QSettings* m_Settings          = nullptr;
  bool       m_GroupNamesMigrated = false;

  mutable bool              m_CustomGroupsCached  = false;
  mutable QList<CustomGroup> m_CustomGroupsCache;
  mutable bool              m_PatchThresholdCached = false;
  mutable int               m_PatchThresholdCache  = 20;
};

// Global singleton — init once in BSPlugins::initPlugin()
BSPluginsINI& pluginINI();

}  // namespace MOPlugin

#endif  // MOPLUGIN_BSPLUGINSINI_H
