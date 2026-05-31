#ifndef BSPLUGINLIST_GROUPMANAGERDIALOG_H
#define BSPLUGINLIST_GROUPMANAGERDIALOG_H

#include <QDialog>
#include <QHash>

class QListWidget;
class QLabel;

namespace TESData { class PluginList; }
namespace BSPluginList { class PluginListModel; }

namespace BSPluginList
{

// Floating group manager window.
// Left panel: all groups (built-in zones + user custom groups).
// Right panel: plugins not yet in any named group.
// Drag from right to left, or select + click Move, to assign.
class GroupManagerDialog final : public QDialog
{
  Q_OBJECT

public:
  GroupManagerDialog(TESData::PluginList* pluginList,
                     PluginListModel*      model,
                     QWidget*             parent = nullptr);

private slots:
  void onAddGroup();
  void onEditGroup();
  void onDeleteGroup();
  void onMoveSelected();
  void onGroupSelectionChanged();

private:
  void populate();
  void refreshUnassigned(const QHash<QString, int>& groupCounts = {});

  TESData::PluginList* m_PluginList;
  PluginListModel*      m_Model;

  QListWidget* m_GroupList      = nullptr;
  QListWidget* m_UnassignedList = nullptr;
  QLabel*      m_GroupInfo      = nullptr;
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_GROUPMANAGERDIALOG_H
