#include "WelcomeDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

WelcomeDialog::WelcomeDialog(Trigger trigger,
                             const QString& previousVersion,
                             const QString& currentVersion,
                             QWidget* parent)
    : QDialog(parent)
{
  setMinimumWidth(520);

  auto* root = new QVBoxLayout(this);
  root->setSpacing(10);

  if (trigger == Trigger::FirstInstall) {
    setWindowTitle(tr("Welcome to BSPlugins Extended %1").arg(currentVersion));

    auto* heading = new QLabel(
        u"<h2>"_s + tr("Bethesda Plugin Manager Extended") + u"</h2>"_s, this);
    heading->setWordWrap(true);
    root->addWidget(heading);

    auto* intro = new QLabel(
        tr("Thanks for installing BSPlugins Extended. This plugin adds intelligent "
           "plugin classification, patch detection, conflict analysis, and a "
           "Starfield-aware load order system to Mod Organizer 2."),
        this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* featuresLabel = new QLabel(
        tr("<b>Key features:</b><ul>"
           "<li>Automatic plugin classification into named groups (Visuals, Patches, etc.)</li>"
           "<li>Inferred patch detection — identifies mods that patch others without declaring masters</li>"
           "<li>Starfield blueprint, medium (ESH), and overlay plugin support</li>"
           "<li>Post-LOOT sort review dialog with group suggestions</li>"
           "</ul>"),
        this);
    featuresLabel->setWordWrap(true);
    root->addWidget(featuresLabel);

  } else {
    setWindowTitle(tr("BSPlugins Extended Updated — %1 → %2")
                       .arg(previousVersion, currentVersion));

    auto* heading = new QLabel(
        u"<h2>"_s + tr("Plugin Updated to %1").arg(currentVersion) + u"</h2>"_s, this);
    root->addWidget(heading);

    auto* changeNote = new QLabel(
        tr("BSPlugins Extended has been updated. Classification rules may have "
           "improved — a fresh run will re-analyse your plugins with the latest "
           "logic and update group assignments for any newly added mods."),
        this);
    changeNote->setWordWrap(true);
    root->addWidget(changeNote);

    // Settings migration note shown in calling code via wasGroupNamesMigrated()
  }

  root->addSpacing(4);

  m_Backup = new QCheckBox(
      tr("Back up plugins.txt, plugingroups.txt and modlist.txt before first run"), this);
  m_Backup->setChecked(true);
  root->addWidget(m_Backup);

  m_FreshRun = new QCheckBox(
      tr("Run fresh plugin classification on next LOOT sort"), this);
  m_FreshRun->setChecked(true);
  root->addWidget(m_FreshRun);

  auto* whyLabel = new QLabel(
      tr("<small><i>Why a fresh run? Classification rules improve with each version. "
         "Running now ensures new mods are categorised correctly and existing groups "
         "reflect the latest detection logic.</i></small>"),
      this);
  whyLabel->setWordWrap(true);
  root->addWidget(whyLabel);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Get Started"));
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  root->addWidget(buttons);
}

bool WelcomeDialog::shouldBackup()        const { return m_Backup->isChecked(); }
bool WelcomeDialog::freshRunRecommended() const { return m_FreshRun->isChecked(); }

}  // namespace BSPluginList
