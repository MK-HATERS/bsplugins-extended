#ifndef BSPLUGINLIST_LOOTUSERLISTDIALOG_H
#define BSPLUGINLIST_LOOTUSERLISTDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QPushButton;

namespace BSPluginList
{

// Dialog for adding or editing a plugin entry in LOOT's userlist.yaml.
// Supports the four most useful rule types: group assignment, load-after,
// require (hard dependency), and conflict (incompatibility).
class LootUserlistDialog final : public QDialog
{
  Q_OBJECT

public:
  LootUserlistDialog(const QString& pluginName,
                     const QString& userlistPath,
                     QWidget* parent = nullptr);

private:
  void load();
  void save();
  void addListItem(QListWidget* list);
  void removeListItem(QListWidget* list);

  QString     m_PluginName;
  QString     m_UserlistPath;

  QLineEdit*  m_GroupEdit       = nullptr;
  QListWidget* m_AfterList      = nullptr;
  QListWidget* m_RequireList    = nullptr;
  QListWidget* m_ConflictList   = nullptr;
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_LOOTUSERLISTDIALOG_H
