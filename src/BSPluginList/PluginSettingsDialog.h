#ifndef BSPLUGINLIST_PLUGINSETTINGSDIALOG_H
#define BSPLUGINLIST_PLUGINSETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QCheckBox;

namespace BSPluginList
{

// In-plugin settings dialog backed by BSPluginsINI.
// Replaces the need to use MO2's global settings dropdown for group names
// and classification options.  Changes are written to settings.ini live.
class PluginSettingsDialog final : public QDialog
{
  Q_OBJECT

public:
  explicit PluginSettingsDialog(QWidget* parent = nullptr);

private slots:
  void resetGroupNames();

private:
  void loadFromINI();
  void saveToINI();
  void connectAutoSave();

  // Group name fields
  QLineEdit* m_Patches     = nullptr;
  QLineEdit* m_Visuals     = nullptr;
  QLineEdit* m_World       = nullptr;
  QLineEdit* m_Gameplay    = nullptr;
  QLineEdit* m_NPCs        = nullptr;
  QLineEdit* m_Frameworks  = nullptr;
  QLineEdit* m_Archive     = nullptr;

  // Classification
  QSpinBox*  m_PatchThreshold = nullptr;
  QCheckBox* m_ArchiveDetect  = nullptr;
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_PLUGINSETTINGSDIALOG_H
