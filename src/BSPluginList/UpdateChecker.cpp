#include "UpdateChecker.h"
#include "MOPlugin/BSPluginsINI.h"

#include <log.h>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

UpdateChecker::UpdateChecker(const QString& currentVersion, QObject* parent)
    : QObject(parent), m_CurrentVersion(currentVersion)
{
  m_Network = new QNetworkAccessManager(this);
  connect(m_Network, &QNetworkAccessManager::finished,
          this, &UpdateChecker::onReply);
}

void UpdateChecker::check()
{
  // Throttle: don't check more than once per day
  auto& ini = MOPlugin::pluginINI();
  const QDateTime lastCheck =
      QDateTime::fromString(ini.lastCheckTimestamp(), Qt::ISODate);
  if (lastCheck.isValid() &&
      lastCheck.daysTo(QDateTime::currentDateTimeUtc()) < 1) {
    return;
  }

  QNetworkRequest req{QUrl(QString::fromLatin1(kReleasesUrl))};
  req.setRawHeader("Accept", "application/vnd.github+json");
  req.setRawHeader("User-Agent", "bsplugins-extended");
  m_Network->get(req);
}

void UpdateChecker::onReply(QNetworkReply* reply)
{
  reply->deleteLater();

  MOPlugin::pluginINI().setLastCheckTimestamp(
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

  if (reply->error() != QNetworkReply::NoError) {
    MOBase::log::warn("BSPlugins update check failed: {}",
                      reply->errorString().toStdString());
    emit checkFailed();
    return;
  }

  const auto doc = QJsonDocument::fromJson(reply->readAll());
  const QString tag = doc.object().value(u"tag_name"_s).toString();
  const QString url = doc.object().value(u"html_url"_s).toString();

  if (tag.isEmpty()) {
    emit checkFailed();
    return;
  }

  // Compare: strip leading 'v' from tag if present
  const QString latestClean = tag.startsWith(u'v') ? tag.mid(1) : tag;
  if (latestClean != m_CurrentVersion) {
    MOBase::log::info("BSPlugins update available: {} -> {}",
                      m_CurrentVersion.toStdString(), latestClean.toStdString());
    emit updateAvailable(latestClean, url);
  }
}

}  // namespace BSPluginList
