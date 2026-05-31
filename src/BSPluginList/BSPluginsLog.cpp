#include "BSPluginsLog.h"

#include <QColor>
#include <QIcon>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

BSPluginsLog& BSPluginsLog::instance()
{
  static BSPluginsLog inst;
  return inst;
}

void BSPluginsLog::add(LogEntry::Level level, const QString& message,
                       const QString& plugin)
{
  const int newRow = static_cast<int>(m_Entries.size());
  beginInsertRows({}, newRow, newRow);
  m_Entries.push_back({level, plugin, message, QDateTime::currentDateTime()});
  endInsertRows();
}

void BSPluginsLog::info(const QString& msg, const QString& plugin)
{
  add(LogEntry::Level::Info, msg, plugin);
}
void BSPluginsLog::warn(const QString& msg, const QString& plugin)
{
  add(LogEntry::Level::Warning, msg, plugin);
}
void BSPluginsLog::crit(const QString& msg, const QString& plugin)
{
  add(LogEntry::Level::Critical, msg, plugin);
}

void BSPluginsLog::clear()
{
  beginResetModel();
  m_Entries.clear();
  endResetModel();
}

int BSPluginsLog::rowCount(const QModelIndex& parent) const
{
  return parent.isValid() ? 0 : static_cast<int>(m_Entries.size());
}

QVariant BSPluginsLog::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() >= static_cast<int>(m_Entries.size())) return {};
  const auto& e = m_Entries.at(index.row());

  switch (role) {
  case Qt::DisplayRole: {
    const QString prefix = e.plugin.isEmpty() ? QString() : u"[%1] "_s.arg(e.plugin);
    return prefix + e.message;
  }
  case Qt::ForegroundRole:
    if (e.level == LogEntry::Level::Critical) return QColor(Qt::red);
    if (e.level == LogEntry::Level::Warning)  return QColor(200, 120, 0);
    return QVariant{};
  case Qt::DecorationRole:
    if (e.level == LogEntry::Level::Critical) return QIcon(u":/bsplugins/beacon-warning"_s);
    if (e.level == LogEntry::Level::Warning)  return QIcon(u":/MO/gui/warning"_s);
    return QIcon(u":/bsplugins/comms"_s);
  case LevelRole:     return static_cast<int>(e.level);
  case PluginRole:    return e.plugin;
  case MessageRole:   return e.message;
  case TimestampRole: return e.timestamp;
  }
  return {};
}

}  // namespace BSPluginList
