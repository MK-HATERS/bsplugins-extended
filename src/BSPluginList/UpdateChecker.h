#ifndef BSPLUGINLIST_UPDATECHECKER_H
#define BSPLUGINLIST_UPDATECHECKER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace BSPluginList
{

// Async GitHub releases check. Fires updateAvailable() if a newer version
// exists. The download URL and version string are passed to the signal so
// the caller can show a notification / restart dialog.
class UpdateChecker final : public QObject
{
  Q_OBJECT

public:
  static constexpr const char* kReleasesUrl =
      "https://api.github.com/repos/MK-HATERS/bsplugins-extended/releases/latest";

  explicit UpdateChecker(const QString& currentVersion, QObject* parent = nullptr);

  // Start the async check. Safe to call from the GUI thread.
  void check();

signals:
  void updateAvailable(const QString& latestVersion, const QString& downloadUrl);
  void checkFailed();

private slots:
  void onReply(QNetworkReply* reply);

private:
  QString                m_CurrentVersion;
  QNetworkAccessManager* m_Network = nullptr;
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_UPDATECHECKER_H
