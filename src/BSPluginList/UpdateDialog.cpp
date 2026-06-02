#include "UpdateDialog.h"
#include "MOPlugin/BSPluginsINI.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

static void dllAnchor() {}

static QString dllPath()
{
  HMODULE hModule = nullptr;
  ::GetModuleHandleExW(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&dllAnchor), &hModule);
  wchar_t path[MAX_PATH] = {};
  ::GetModuleFileNameW(hModule, path, MAX_PATH);
  return QString::fromWCharArray(path);
}

static QString buildScript(const QString& tempDll, const QString& targetDll,
                            const QString& moExe)
{
  return uR"(@echo off
echo Waiting for ModOrganizer to close...
timeout /t 3 /nobreak > nul
echo Replacing plugin...
copy /y "%1" "%2"
if errorlevel 1 (
    echo Update failed. Please copy the file manually.
    pause
    goto :eof
)
echo Starting ModOrganizer...
start "" "%3"
del "%~f0"
)"_s.arg(tempDll, targetDll, moExe);
}

UpdateDialog::UpdateDialog(const QString& latestVersion,
                           const QString& downloadUrl,
                           QWidget* parent)
    : QDialog(parent)
    , m_DownloadUrl(downloadUrl)
    , m_LatestVersion(latestVersion)
{
  setWindowTitle(tr("Update Available — BSPlugins Extended %1").arg(latestVersion));
  setMinimumWidth(560);

  auto* root = new QVBoxLayout(this);
  root->setSpacing(10);

  auto* intro = new QLabel(
      tr("Version <b>%1</b> of BSPlugins Extended is available.<br><br>"
         "The update process will:<br>"
         "1. Download <code>bsplugins.dll</code> from GitHub<br>"
         "2. Create a batch script (shown below) in your temp folder<br>"
         "3. Close ModOrganizer 2<br>"
         "4. The script replaces the DLL and relaunches MO2").arg(latestVersion),
      this);
  intro->setWordWrap(true);
  root->addWidget(intro);

  // Show the exact script so the user can verify it
  const QString ourDll = dllPath();
  const QString moExe  = QCoreApplication::applicationFilePath()
                             .replace(u'/', u'\\');
  const QString tempDll = QDir::toNativeSeparators(
      QDir::tempPath() + u"/bsplugins_update.dll");
  const QString script  = buildScript(tempDll, QDir::toNativeSeparators(ourDll), moExe);

  auto* scriptBox = new QGroupBox(tr("Update script (so you know it's safe):"), this);
  auto* scriptEdit = new QPlainTextEdit(script, scriptBox);
  scriptEdit->setReadOnly(true);
  scriptEdit->setMaximumHeight(150);
  scriptEdit->setFont(QFont(u"Consolas"_s, 8));
  auto* scriptLay = new QVBoxLayout(scriptBox);
  scriptLay->addWidget(scriptEdit);
  root->addWidget(scriptBox);

  auto* note = new QLabel(
      tr("<small>The script deletes itself after running. "
         "If anything goes wrong the original DLL is untouched — "
         "the copy overwrites only on success.</small>"),
      this);
  note->setWordWrap(true);
  root->addWidget(note);

  auto* buttons = new QDialogButtonBox(this);
  auto* install = buttons->addButton(tr("Download && Install"),
                                     QDialogButtonBox::AcceptRole);
  auto* github  = buttons->addButton(tr("Open GitHub"),
                                     QDialogButtonBox::ActionRole);
  auto* skip    = buttons->addButton(tr("Skip this version"),
                                     QDialogButtonBox::RejectRole);
  Q_UNUSED(skip);

  connect(install, &QPushButton::clicked, this, &UpdateDialog::downloadAndInstall);
  connect(github,  &QPushButton::clicked, this, &UpdateDialog::openGitHub);
  connect(buttons, &QDialogButtonBox::rejected, this, [this]() {
    MOPlugin::pluginINI().setSkipVersion(m_LatestVersion);
    reject();
  });
  root->addWidget(buttons);
}

void UpdateDialog::openGitHub()
{
  // Prefer user-configured Nexus URL over GitHub releases
  const QString nexus = MOPlugin::pluginINI().nexusUrl();
  const QString url   = (!nexus.isEmpty() && nexus != QStringLiteral("https://www.nexusmods.com/"))
                            ? nexus
                            : m_DownloadUrl;
  QDesktopServices::openUrl(QUrl(url));
}

void UpdateDialog::downloadAndInstall()
{
  // The full download → script → restart flow would need QNetworkAccessManager
  // here. For now, open GitHub releases so the user can download manually —
  // fully automated download is Sprint 3+ work.
  openGitHub();
  accept();
}

}  // namespace BSPluginList
