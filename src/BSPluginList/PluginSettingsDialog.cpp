#include "PluginSettingsDialog.h"
#include "MOPlugin/BSPluginsINI.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

PluginSettingsDialog::PluginSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(tr("BSPlugins Extended — Settings"));
  setMinimumWidth(460);

  auto* root = new QVBoxLayout(this);
  root->setSpacing(12);

  // ---- Group Names --------------------------------------------------------
  auto* namesBox = new QGroupBox(tr("Group Names"), this);
  namesBox->setToolTip(
      tr("These names are used when plugins are automatically assigned to groups "
         "after a LOOT sort. Changes are saved immediately and survive updates."));

  auto* namesForm = new QFormLayout(namesBox);
  namesForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  auto makeField = [&](const QString& label, const QString& ph) -> QLineEdit* {
    auto* e = new QLineEdit(namesBox);
    e->setPlaceholderText(ph);
    namesForm->addRow(label, e);
    return e;
  };

  m_Patches    = makeField(tr("Patches:"),        u"Patches"_s);
  m_Visuals    = makeField(tr("Visuals:"),         u"Visuals"_s);
  m_World      = makeField(tr("World Changes:"),   u"World Changes"_s);
  m_Gameplay   = makeField(tr("Gameplay:"),        u"Gameplay"_s);
  m_NPCs       = makeField(tr("NPCs & Content:"),  u"NPCs & Content"_s);
  m_Frameworks = makeField(tr("Frameworks:"),      u"Frameworks"_s);
  m_Archive    = makeField(tr("Archive Loaders:"), u"Archive Loaders"_s);

  auto* resetBtn = new QPushButton(tr("Reset to defaults"), namesBox);
  connect(resetBtn, &QPushButton::clicked, this, &PluginSettingsDialog::resetGroupNames);
  namesForm->addRow(QString(), resetBtn);

  root->addWidget(namesBox);

  // ---- Classification -----------------------------------------------------
  auto* classBox  = new QGroupBox(tr("Classification"), this);
  auto* classForm = new QFormLayout(classBox);
  classForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  m_PatchThreshold = new QSpinBox(classBox);
  m_PatchThreshold->setRange(1, 200);
  m_PatchThreshold->setSuffix(tr(" records"));
  m_PatchThreshold->setToolTip(
      tr("Minimum number of shared records for a plugin to be suggested as a patch "
         "for another. Lower = more suggestions; higher = only high-confidence ones."));
  classForm->addRow(tr("Patch detection threshold:"), m_PatchThreshold);

  m_ArchiveDetect = new QCheckBox(tr("Detect archive-only plugins (BSA/BA2)"), classBox);
  m_ArchiveDetect->setToolTip(
      tr("Plugins with no records that only load a BSA/BA2 archive are flagged "
         "as 'Archive Loaders'. The left panel mod order matters for these, "
         "not the plugin load order."));
  classForm->addRow(QString(), m_ArchiveDetect);

  root->addWidget(classBox);

  // ---- About --------------------------------------------------------------
  auto* aboutLabel = new QLabel(
      tr("<small>BSPlugins Extended v0.2.0 by MK-HATERS<br>"
         "Based on work by Parapets and Alaxouche · "
         "<a href='https://github.com/MK-HATERS/bsplugins-extended'>GitHub</a>"
         "</small>"),
      this);
  aboutLabel->setOpenExternalLinks(true);
  aboutLabel->setAlignment(Qt::AlignCenter);
  root->addWidget(aboutLabel);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);

  loadFromINI();
  connectAutoSave();
}

void PluginSettingsDialog::loadFromINI()
{
  const auto& ini = MOPlugin::pluginINI();
  m_Patches->setText(ini.groupNamePatches());
  m_Visuals->setText(ini.groupNameVisuals());
  m_World->setText(ini.groupNameWorld());
  m_Gameplay->setText(ini.groupNameGameplay());
  m_NPCs->setText(ini.groupNameNPCs());
  m_Frameworks->setText(ini.groupNameFrameworks());
  m_Archive->setText(ini.groupNameArchive());
  m_PatchThreshold->setValue(ini.patchThreshold());
  m_ArchiveDetect->setChecked(ini.archiveDetectionEnabled());
}

void PluginSettingsDialog::saveToINI()
{
  auto& ini = MOPlugin::pluginINI();
  ini.setGroupNamePatches(m_Patches->text().trimmed().isEmpty()
                          ? u"Patches"_s : m_Patches->text().trimmed());
  ini.setGroupNameVisuals(m_Visuals->text().trimmed().isEmpty()
                          ? u"Visuals"_s : m_Visuals->text().trimmed());
  ini.setGroupNameWorld(m_World->text().trimmed().isEmpty()
                        ? u"World Changes"_s : m_World->text().trimmed());
  ini.setGroupNameGameplay(m_Gameplay->text().trimmed().isEmpty()
                           ? u"Gameplay"_s : m_Gameplay->text().trimmed());
  ini.setGroupNameNPCs(m_NPCs->text().trimmed().isEmpty()
                       ? u"NPCs & Content"_s : m_NPCs->text().trimmed());
  ini.setGroupNameFrameworks(m_Frameworks->text().trimmed().isEmpty()
                             ? u"Frameworks"_s : m_Frameworks->text().trimmed());
  ini.setGroupNameArchive(m_Archive->text().trimmed().isEmpty()
                          ? u"Archive Loaders"_s : m_Archive->text().trimmed());
  ini.setPatchThreshold(m_PatchThreshold->value());
  ini.setArchiveDetectionEnabled(m_ArchiveDetect->isChecked());
}

void PluginSettingsDialog::connectAutoSave()
{
  // Save on every edit — no "Apply" button needed
  for (auto* e : {m_Patches, m_Visuals, m_World, m_Gameplay,
                  m_NPCs, m_Frameworks, m_Archive}) {
    connect(e, &QLineEdit::textChanged, this, &PluginSettingsDialog::saveToINI);
  }
  connect(m_PatchThreshold, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &PluginSettingsDialog::saveToINI);
  connect(m_ArchiveDetect, &QCheckBox::toggled,
          this, &PluginSettingsDialog::saveToINI);
}

void PluginSettingsDialog::resetGroupNames()
{
  MOPlugin::pluginINI().resetGroupNamesToDefaults();
  loadFromINI();
}

}  // namespace BSPluginList
