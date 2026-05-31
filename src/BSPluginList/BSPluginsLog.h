#ifndef BSPLUGINLIST_BSPLUGINSLOG_H
#define BSPLUGINLIST_BSPLUGINSLOG_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace BSPluginList
{

struct LogEntry
{
  enum class Level { Info, Warning, Critical };

  Level     level;
  QString   plugin;   // associated plugin name (empty = general)
  QString   message;
  QDateTime timestamp;
};

// In-memory log for BSPlugins-specific messages.
// Keeps our output out of MO2's main log while still being accessible
// through the Log panel in the plugin widget.
class BSPluginsLog final : public QAbstractListModel
{
  Q_OBJECT

public:
  enum Role { LevelRole = Qt::UserRole, PluginRole, MessageRole, TimestampRole };

  static BSPluginsLog& instance();

  void add(LogEntry::Level level, const QString& message,
           const QString& plugin = {});

  void info(const QString& msg, const QString& plugin = {});
  void warn(const QString& msg, const QString& plugin = {});
  void crit(const QString& msg, const QString& plugin = {});

  void clear();

  // QAbstractListModel
  int      rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
  BSPluginsLog() = default;
  QVector<LogEntry> m_Entries;
};

// Convenience free functions so call sites don't need to type the singleton
inline void bsLog(const QString& msg, const QString& plugin = {})
{
  BSPluginsLog::instance().info(msg, plugin);
}
inline void bsWarn(const QString& msg, const QString& plugin = {})
{
  BSPluginsLog::instance().warn(msg, plugin);
}
inline void bsCrit(const QString& msg, const QString& plugin = {})
{
  BSPluginsLog::instance().crit(msg, plugin);
}

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_BSPLUGINSLOG_H
