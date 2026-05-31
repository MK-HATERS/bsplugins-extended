#include "CustomGroupDialog.h"
#include "TESData/TypeStringNames.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

// ---- Static data ----------------------------------------------------------

const QStringList CustomGroupDialog::kOpenZones{
    u"Visuals"_s,
    u"World Changes"_s,
    u"Gameplay"_s,
    u"NPCs & Content"_s,
    u"Frameworks"_s,
};

static QString zoneExplanation(const QString& zone)
{
  if (zone == u"Visuals"_s)
    return QObject::tr("Loads late — after world structure is settled. Good for "
                       "texture replacers, lighting tweaks, outfit mods.");
  if (zone == u"World Changes"_s)
    return QObject::tr("Loads early-middle — sets up the world before visual mods "
                       "overlay it. Good for landscape, weather, POI overhauls.");
  if (zone == u"Gameplay"_s)
    return QObject::tr("Loads middle — mechanics and balance changes. Good for "
                       "perks, economy, game-setting tweaks.");
  if (zone == u"NPCs & Content"_s)
    return QObject::tr("Loads middle-late — new content and character edits. Good "
                       "for NPC overhauls, companions, quest mods.");
  if (zone == u"Frameworks"_s)
    return QObject::tr("Loads early — libraries many other mods depend on. "
                       "Manually assign mods here; the masters-list detector "
                       "handles most frameworks automatically.");
  return QString();
}

QList<CustomGroupDialog::RecordCategory> CustomGroupDialog::recordCategories()
{
  return {
    { QObject::tr("Visual"),   { u"LIGH"_s, u"TXST"_s, u"ARMO"_s, u"CLOT"_s, u"HDPT"_s, u"IMGS"_s } },
    { QObject::tr("World"),    { u"WRLD"_s, u"CELL"_s, u"LAND"_s, u"LTEX"_s, u"NAVM"_s, u"STAT"_s, u"FURN"_s } },
    { QObject::tr("NPCs"),     { u"NPC_"_s, u"RACE"_s, u"FACT"_s, u"CLAS"_s, u"HDPT"_s } },
    { QObject::tr("Gameplay"), { u"PERK"_s, u"AVIF"_s, u"GMST"_s, u"ALCH"_s, u"SPEL"_s, u"MGEF"_s } },
    { QObject::tr("Combat"),   { u"WEAP"_s, u"AMMO"_s, u"PROJ"_s, u"EXPL"_s, u"ENCH"_s } },
    { QObject::tr("Quests"),   { u"QUST"_s, u"DIAL"_s, u"SCEN"_s } },
  };
}

// ---- Construction ---------------------------------------------------------

CustomGroupDialog::CustomGroupDialog(QWidget* parent) : QDialog(parent)
{
  buildUi();
}

CustomGroupDialog::CustomGroupDialog(const MOPlugin::BSPluginsINI::CustomGroup& g,
                                     QWidget* parent)
    : QDialog(parent)
{
  buildUi();
  m_Name->setText(g.name);
  const int zoneIdx = kOpenZones.indexOf(g.zone);
  if (zoneIdx >= 0) m_Zone->setCurrentIndex(zoneIdx);
  m_Threshold->setValue(g.threshold);

  // Re-check record types
  for (int i = 0; i < m_RecordList->count(); ++i) {
    auto* item = m_RecordList->item(i);
    if (g.recordTypes.contains(item->data(Qt::UserRole).toString())) {
      item->setCheckState(Qt::Checked);
    }
  }
  if (!g.recordTypes.isEmpty()) toggleAdvanced(true);
}

// ---- UI build -------------------------------------------------------------

void CustomGroupDialog::buildUi()
{
  setWindowTitle(tr("Custom Group"));
  setMinimumWidth(420);

  auto* root = new QVBoxLayout(this);
  root->setSpacing(8);

  // ── Simple section ──────────────────────────────────────────────────────
  auto* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  m_Name = new QLineEdit(this);
  m_Name->setPlaceholderText(tr("e.g. Rabbit's Lights"));
  form->addRow(tr("Name:"), m_Name);

  m_Zone = new QComboBox(this);
  m_Zone->addItems(kOpenZones);
  form->addRow(tr("Zone:"), m_Zone);

  m_ZoneExplain = new QLabel(this);
  m_ZoneExplain->setWordWrap(true);
  m_ZoneExplain->setStyleSheet(u"color: gray; font-size: small;"_s);
  form->addRow(QString(), m_ZoneExplain);

  root->addLayout(form);

  // ── Advanced toggle ─────────────────────────────────────────────────────
  m_AdvBtn = new QPushButton(tr("[Advanced ▸] Record type filter (optional)"), this);
  m_AdvBtn->setFlat(true);
  m_AdvBtn->setStyleSheet(u"text-align: left; color: palette(link);"_s);
  connect(m_AdvBtn, &QPushButton::clicked, this, [this]() {
    toggleAdvanced(!m_Advanced);
  });
  root->addWidget(m_AdvBtn);

  // ── Advanced panel (hidden by default) ──────────────────────────────────
  m_AdvPanel = new QWidget(this);
  m_AdvPanel->hide();
  auto* advLay = new QVBoxLayout(m_AdvPanel);
  advLay->setContentsMargins(0, 0, 0, 0);

  auto* advInfo = new QLabel(
      tr("Match plugins where these record types make up at least this % of all records.\n"
         "Leave empty to match by zone only."), m_AdvPanel);
  advInfo->setWordWrap(true);
  advInfo->setStyleSheet(u"color: gray; font-size: small;"_s);
  advLay->addWidget(advInfo);

  // Search + threshold row
  auto* searchRow = new QHBoxLayout;
  m_RecordSearch = new QLineEdit(m_AdvPanel);
  m_RecordSearch->setPlaceholderText(tr("Search record types…"));
  searchRow->addWidget(new QLabel(tr("Search:"), m_AdvPanel));
  searchRow->addWidget(m_RecordSearch, 1);
  searchRow->addWidget(new QLabel(tr("Threshold:"), m_AdvPanel));
  m_Threshold = new QSpinBox(m_AdvPanel);
  m_Threshold->setRange(1, 100);
  m_Threshold->setValue(15);
  m_Threshold->setSuffix(u"%"_s);
  searchRow->addWidget(m_Threshold);
  advLay->addLayout(searchRow);

  // Record type list organised by category
  m_RecordList = new QListWidget(m_AdvPanel);
  m_RecordList->setMaximumHeight(180);
  for (const auto& cat : recordCategories()) {
    auto* header = new QListWidgetItem(u"── %1 ──"_s.arg(cat.label));
    header->setFlags(Qt::NoItemFlags);
    header->setForeground(Qt::gray);
    m_RecordList->addItem(header);
    for (const QString& code : cat.types) {
      auto* item = new QListWidgetItem(u"%1 (%2)"_s.arg(
          TESData::formTypeName(code), code));
      item->setData(Qt::UserRole, code);
      item->setCheckState(Qt::Unchecked);
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      m_RecordList->addItem(item);
    }
  }
  advLay->addWidget(m_RecordList);

  // Filter record list on search
  connect(m_RecordSearch, &QLineEdit::textChanged, this, [this](const QString& text) {
    for (int i = 0; i < m_RecordList->count(); ++i) {
      auto* item = m_RecordList->item(i);
      const bool header = !(item->flags() & Qt::ItemIsEnabled);
      item->setHidden(!header && !text.isEmpty() &&
                      !item->text().contains(text, Qt::CaseInsensitive));
    }
  });

  // Live preview
  m_Preview = new QLabel(m_AdvPanel);
  m_Preview->setWordWrap(true);
  m_Preview->setStyleSheet(u"color: palette(link); font-size: small; font-style: italic;"_s);
  advLay->addWidget(m_Preview);

  root->addWidget(m_AdvPanel);

  // ── Buttons ─────────────────────────────────────────────────────────────
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);

  // Wire live updates
  connect(m_Zone, &QComboBox::currentTextChanged, this, [this](const QString& z) {
    updateZoneExplanation(z);
    updatePreview();
  });
  connect(m_Name,      &QLineEdit::textChanged,     this, [this]{ updatePreview(); });
  connect(m_Threshold, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this]{ updatePreview(); });
  connect(m_RecordList, &QListWidget::itemChanged, this, [this]{ updatePreview(); });

  updateZoneExplanation(kOpenZones.first());
  updatePreview();
}

void CustomGroupDialog::toggleAdvanced(bool expanded)
{
  m_Advanced = expanded;
  m_AdvPanel->setVisible(expanded);
  m_AdvBtn->setText(expanded ? tr("[Advanced ▾] Record type filter (optional)")
                              : tr("[Advanced ▸] Record type filter (optional)"));
  adjustSize();
}

void CustomGroupDialog::updateZoneExplanation(const QString& zone)
{
  m_ZoneExplain->setText(zoneExplanation(zone));
}

void CustomGroupDialog::updatePreview()
{
  const QString name = m_Name->text().trimmed();
  const QString zone = m_Zone->currentText();
  if (name.isEmpty()) { m_Preview->clear(); return; }

  QStringList checked;
  for (int i = 0; i < m_RecordList->count(); ++i) {
    const auto* item = m_RecordList->item(i);
    if (item->checkState() == Qt::Checked)
      checked << item->data(Qt::UserRole).toString();
  }

  if (checked.isEmpty()) {
    m_Preview->setText(tr("New mods classified as %1 → suggested to \"%2\"")
                          .arg(zone, name));
  } else {
    m_Preview->setText(
        tr("Mods where %1 ≥ %2% of records → suggested to \"%3\" (%4 zone)")
            .arg(checked.join(u"+"_s))
            .arg(m_Threshold->value())
            .arg(name, zone));
  }
}

// ---- Result ---------------------------------------------------------------

MOPlugin::BSPluginsINI::CustomGroup CustomGroupDialog::result() const
{
  MOPlugin::BSPluginsINI::CustomGroup g;
  g.name      = m_Name->text().trimmed();
  g.zone      = m_Zone->currentText();
  g.threshold = m_Threshold ? m_Threshold->value() : 15;

  if (m_Advanced && m_RecordList) {
    for (int i = 0; i < m_RecordList->count(); ++i) {
      const auto* item = m_RecordList->item(i);
      if (item->checkState() == Qt::Checked) {
        g.recordTypes << item->data(Qt::UserRole).toString();
      }
    }
  }
  return g;
}

}  // namespace BSPluginList
