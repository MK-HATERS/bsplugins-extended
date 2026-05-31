#ifndef BSPLUGINLIST_UPDATEDIALOG_H
#define BSPLUGINLIST_UPDATEDIALOG_H

#include <QDialog>
#include <QString>

namespace BSPluginList
{

// Shown when a newer plugin version is available.
// Displays the exact batch script that will run so the user can verify
// it's not doing anything unexpected before confirming the restart.
class UpdateDialog final : public QDialog
{
  Q_OBJECT

public:
  UpdateDialog(const QString& latestVersion,
               const QString& downloadUrl,
               QWidget* parent = nullptr);

private slots:
  void downloadAndInstall();
  void openGitHub();

private:
  QString m_DownloadUrl;
  QString m_LatestVersion;
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_UPDATEDIALOG_H
