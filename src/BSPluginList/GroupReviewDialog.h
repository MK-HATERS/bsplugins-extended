#ifndef BSPLUGINLIST_GROUPREVIEWDIALOG_H
#define BSPLUGINLIST_GROUPREVIEWDIALOG_H

#include "TESData/PluginClassifier.h"

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;
class QTreeWidget;
class QLabel;

namespace TESData { class PluginList; }

namespace BSPluginList
{

class GroupReviewDialog final : public QDialog
{
  Q_OBJECT

public:
  struct PatchSuggestion
  {
    QString patchPlugin;
    QString patchOrigin;   // mod name from left panel
    QString targetPlugin;
    QString targetOrigin;
    int     recordCount;
    bool    preChecked;    // true when count >= 20
  };

  struct GroupSuggestion
  {
    QString                  pluginName;
    QString                  modOrigin;
    TESData::Classification  classification;
    bool                     preChecked;   // true when confidence >= 70
    bool                     alreadyGrouped; // skip if already in a named group
  };

  GroupReviewDialog(const QList<PatchSuggestion>& patches,
                    const QList<GroupSuggestion>& groups,
                    QWidget* parent = nullptr);

  // Patches the user confirmed — apply moveToPriority for these
  [[nodiscard]] QList<PatchSuggestion> confirmedPatches() const;

  // Group assignments the user confirmed
  [[nodiscard]] QList<GroupSuggestion> confirmedGroups() const;

  // Whether the user wants to create/use a "Patches" group
  [[nodiscard]] bool createPatchGroup() const;

private:
  void buildPatchTab(const QList<PatchSuggestion>& patches);
  void buildGroupTab(const QList<GroupSuggestion>& groups);
  static QString confidenceDots(int confidence);
  static QString originLabel(const QString& plugin, const QString& origin);

  QTreeWidget* m_PatchTree  = nullptr;
  QTreeWidget* m_GroupTree  = nullptr;
  QCheckBox*   m_PatchGroup = nullptr;
  QLabel*      m_SummaryLabel = nullptr;
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_GROUPREVIEWDIALOG_H
