#ifndef BSPLUGINLIST_CUSTOMGROUPDIALOG_H
#define BSPLUGINLIST_CUSTOMGROUPDIALOG_H

#include "MOPlugin/BSPluginsINI.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QWidget;

namespace BSPluginList
{

// Create or edit a user-defined custom group.
// Simple mode: name + zone dropdown (10 seconds to create).
// Advanced mode: adds record type picker + threshold.
class CustomGroupDialog final : public QDialog
{
  Q_OBJECT

public:
  // Zone options available for custom groups (locked zones excluded)
  static const QStringList kOpenZones;

  // Record type categories shown in Advanced mode
  struct RecordCategory { QString label; QStringList types; };
  static QList<RecordCategory> recordCategories();

  explicit CustomGroupDialog(QWidget* parent = nullptr);
  // Pre-populate for editing an existing group
  explicit CustomGroupDialog(const MOPlugin::BSPluginsINI::CustomGroup& existing,
                             QWidget* parent = nullptr);

  [[nodiscard]] MOPlugin::BSPluginsINI::CustomGroup result() const;

private:
  void buildUi();
  void toggleAdvanced(bool expanded);
  void updateZoneExplanation(const QString& zone);
  void updatePreview();

  QLineEdit*   m_Name       = nullptr;
  QComboBox*   m_Zone       = nullptr;
  QLabel*      m_ZoneExplain= nullptr;
  QPushButton* m_AdvBtn     = nullptr;
  QWidget*     m_AdvPanel   = nullptr;
  QLineEdit*   m_RecordSearch = nullptr;
  QListWidget* m_RecordList   = nullptr;
  QSpinBox*    m_Threshold    = nullptr;
  QLabel*      m_Preview      = nullptr;
  bool         m_Advanced     = false;
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_CUSTOMGROUPDIALOG_H
