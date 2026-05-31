#ifndef BSPLUGINLIST_WELCOMEDIALOG_H
#define BSPLUGINLIST_WELCOMEDIALOG_H

#include <QDialog>
#include <QString>

class QCheckBox;

namespace BSPluginList
{

// Shown on first install OR after a version update.
// Offers to back up the current load order and profile before any
// classification run is attempted.
class WelcomeDialog final : public QDialog
{
  Q_OBJECT

public:
  enum class Trigger { FirstInstall, Update };

  WelcomeDialog(Trigger trigger,
                const QString& previousVersion,
                const QString& currentVersion,
                QWidget* parent = nullptr);

  [[nodiscard]] bool shouldBackup()         const;
  [[nodiscard]] bool freshRunRecommended()  const;

private:
  QCheckBox* m_Backup    = nullptr;
  QCheckBox* m_FreshRun  = nullptr;
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_WELCOMEDIALOG_H
