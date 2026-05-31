#include "GroupManagerDialog.h"
#include "CustomGroupDialog.h"
#include "MOPlugin/BSPluginsINI.h"
#include "PluginListModel.h"
#include "TESData/PluginList.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

// locked zones shown as non-selectable with explanation
static const QStringList kLockedZones{
    u"Core"_s, u"Blueprints"_s, u"Archive Loaders"_s, u"Patches"_s,
};
static const char* kLockedExplainStr =
    QT_TRANSLATE_NOOP("GroupManagerDialog",
                      "This zone is managed automatically (not configurable by record types).");

// ── Construction ────────────────────────────────────────────────────────────

GroupManagerDialog::GroupManagerDialog(TESData::PluginList* pluginList,
                                       PluginListModel*      model,
                                       QWidget*             parent)
    : QDialog(parent), m_PluginList(pluginList), m_Model(model)
{
  setWindowTitle(tr("Manage Plugin Groups"));
  setMinimumSize(700, 480);

  auto* root = new QVBoxLayout(this);

  // ── Toolbar ───────────────────────────────────────────────────────────────
  auto* toolbar = new QHBoxLayout;
  auto* addBtn  = new QPushButton(tr("+ New Group"), this);
  auto* editBtn = new QPushButton(tr("Edit"),        this);
  auto* delBtn  = new QPushButton(tr("Delete"),      this);
  addBtn->setToolTip(tr("Create a new custom group with optional record-type rules."));
  editBtn->setToolTip(tr("Edit the selected custom group's name, zone and record types."));
  delBtn->setToolTip(tr("Delete the selected custom group (plugins move to Unassigned)."));
  toolbar->addWidget(addBtn);
  toolbar->addWidget(editBtn);
  toolbar->addWidget(delBtn);
  toolbar->addStretch();

  auto* helpLabel = new QLabel(
      tr("<small>🔒 Locked zones are managed automatically.</small>"), this);
  helpLabel->setStyleSheet(u"color: gray;"_s);
  toolbar->addWidget(helpLabel);
  root->addLayout(toolbar);

  // ── Main splitter ─────────────────────────────────────────────────────────
  auto* splitter = new QSplitter(Qt::Horizontal, this);

  // Left: group list
  auto* leftPane  = new QWidget(splitter);
  auto* leftLay   = new QVBoxLayout(leftPane);
  leftLay->setContentsMargins(0, 0, 0, 0);
  leftLay->addWidget(new QLabel(tr("Groups"), leftPane));

  m_GroupList = new QListWidget(leftPane);
  m_GroupList->setAcceptDrops(true);
  m_GroupList->setDragDropMode(QAbstractItemView::DropOnly);
  m_GroupList->setToolTip(tr("Drag plugins here from the right panel to assign them."));
  leftLay->addWidget(m_GroupList, 1);

  m_GroupInfo = new QLabel(leftPane);
  m_GroupInfo->setWordWrap(true);
  m_GroupInfo->setStyleSheet(u"color: gray; font-size: small;"_s);
  leftLay->addWidget(m_GroupInfo);

  splitter->addWidget(leftPane);

  // Right: unassigned plugins
  auto* rightPane = new QWidget(splitter);
  auto* rightLay  = new QVBoxLayout(rightPane);
  rightLay->setContentsMargins(0, 0, 0, 0);
  rightLay->addWidget(new QLabel(tr("Unassigned Plugins"), rightPane));

  m_UnassignedList = new QListWidget(rightPane);
  m_UnassignedList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_UnassignedList->setDragEnabled(true);
  m_UnassignedList->setDragDropMode(QAbstractItemView::DragOnly);
  m_UnassignedList->setToolTip(
      tr("Select one or more plugins, then click Move or drag to a group on the left."));
  rightLay->addWidget(m_UnassignedList, 1);

  auto* moveBtn = new QPushButton(tr("Move to selected group →"), rightPane);
  moveBtn->setToolTip(tr("Move selected plugins to the group highlighted on the left."));
  rightLay->addWidget(moveBtn);

  splitter->addWidget(rightPane);
  splitter->setSizes({250, 400});
  root->addWidget(splitter, 1);

  // ── Bottom buttons ────────────────────────────────────────────────────────
  auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(btns);

  // ── Wiring ────────────────────────────────────────────────────────────────
  connect(addBtn,  &QPushButton::clicked, this, &GroupManagerDialog::onAddGroup);
  connect(editBtn, &QPushButton::clicked, this, &GroupManagerDialog::onEditGroup);
  connect(delBtn,  &QPushButton::clicked, this, &GroupManagerDialog::onDeleteGroup);
  connect(moveBtn, &QPushButton::clicked, this, &GroupManagerDialog::onMoveSelected);
  connect(m_GroupList, &QListWidget::currentRowChanged,
          this, &GroupManagerDialog::onGroupSelectionChanged);

  populate();
}

// ── Populate ────────────────────────────────────────────────────────────────

void GroupManagerDialog::populate()
{
  m_GroupList->clear();

  // Build plugin-per-group counts in a single O(n) pass
  QHash<QString, int> groupCounts;
  for (int i = 0; i < m_PluginList->pluginCount(); ++i) {
    if (const auto* p = m_PluginList->getPlugin(i)) {
      if (!p->group().isEmpty()) groupCounts[p->group()]++;
    }
  }

  // Locked zones first
  for (const QString& z : kLockedZones) {
    auto* item = new QListWidgetItem(u"🔒 %1"_s.arg(z), m_GroupList);
    item->setFlags(Qt::ItemIsEnabled);  // not selectable for assignment
    item->setForeground(Qt::gray);
    item->setData(Qt::UserRole, z);
    item->setData(Qt::UserRole + 1, true);  // isLocked
  }

  // Built-in open zones — O(1) lookup from pre-built counts
  for (const QString& z : CustomGroupDialog::kOpenZones) {
    auto* item = new QListWidgetItem(
        u"%1  (%2)"_s.arg(z).arg(groupCounts.value(z, 0)), m_GroupList);
    item->setData(Qt::UserRole, z);
  }

  // User custom groups (★ prefix)
  for (const auto& cg : MOPlugin::pluginINI().customGroups()) {
    auto* item = new QListWidgetItem(
        u"★ %1  (%2)"_s.arg(cg.name).arg(groupCounts.value(cg.name, 0)), m_GroupList);
    item->setData(Qt::UserRole, cg.name);
  }

  refreshUnassigned(groupCounts);
}

// groupCounts is pre-built by populate() — avoids a second O(n) pass
void GroupManagerDialog::refreshUnassigned(const QHash<QString, int>& /*groupCounts*/)
{
  m_UnassignedList->clear();
  for (int i = 0; i < m_PluginList->pluginCount(); ++i) {
    const auto* p = m_PluginList->getPlugin(i);
    if (!p) continue;
    const bool ungrouped = p->group().isEmpty() || p->group() == u"default"_s;
    if (!ungrouped) continue;
    auto* item = new QListWidgetItem(p->name(), m_UnassignedList);
    item->setData(Qt::UserRole, p->name());
    item->setToolTip(tr("Select and click 'Move to selected group' to assign."));
  }
}

// ── Slots ───────────────────────────────────────────────────────────────────

void GroupManagerDialog::onGroupSelectionChanged()
{
  auto* cur = m_GroupList->currentItem();
  if (!cur) { m_GroupInfo->clear(); return; }

  const bool locked = cur->data(Qt::UserRole + 1).toBool();
  if (locked) {
    m_GroupInfo->setText(tr(kLockedExplainStr));
    return;
  }
  const QString name = cur->data(Qt::UserRole).toString();
  // Check if it's a custom group
  for (const auto& cg : MOPlugin::pluginINI().customGroups()) {
    if (cg.name == name) {
      m_GroupInfo->setText(
          tr("Zone: %1 | Records: %2 | Threshold: %3%")
              .arg(cg.zone,
                   cg.recordTypes.isEmpty() ? tr("any") : cg.recordTypes.join(u", "_s),
                   QString::number(cg.threshold)));
      return;
    }
  }
  m_GroupInfo->setText(tr("Built-in zone — drag or select plugins to assign."));
}

void GroupManagerDialog::onMoveSelected()
{
  auto* groupItem = m_GroupList->currentItem();
  if (!groupItem || groupItem->data(Qt::UserRole + 1).toBool()) return;
  const QString targetGroup = groupItem->data(Qt::UserRole).toString();

  QModelIndexList indices;
  for (auto* item : m_UnassignedList->selectedItems()) {
    const QString pluginName = item->data(Qt::UserRole).toString();
    const int idx = m_PluginList->getIndex(pluginName);
    if (idx >= 0) indices.append(m_Model->index(idx, 0));
  }
  if (!indices.isEmpty()) {
    m_Model->setGroup(indices, targetGroup);
    populate();
  }
}

void GroupManagerDialog::onAddGroup()
{
  CustomGroupDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted) return;
  const auto g = dlg.result();
  if (g.name.isEmpty()) return;
  MOPlugin::pluginINI().addCustomGroup(g);
  populate();
}

void GroupManagerDialog::onEditGroup()
{
  auto* cur = m_GroupList->currentItem();
  if (!cur || cur->data(Qt::UserRole + 1).toBool()) return;
  const QString name = cur->data(Qt::UserRole).toString();
  for (const auto& cg : MOPlugin::pluginINI().customGroups()) {
    if (cg.name == name) {
      CustomGroupDialog dlg(cg, this);
      if (dlg.exec() != QDialog::Accepted) return;
      auto groups = MOPlugin::pluginINI().customGroups();
      for (auto& g : groups) {
        if (g.name == name) { g = dlg.result(); break; }
      }
      MOPlugin::pluginINI().setCustomGroups(groups);
      populate();
      return;
    }
  }
  // Built-in zone — can't edit
  QMessageBox::information(this, tr("Group Manager"),
                           tr("Built-in zones cannot be edited."));
}

void GroupManagerDialog::onDeleteGroup()
{
  auto* cur = m_GroupList->currentItem();
  if (!cur || cur->data(Qt::UserRole + 1).toBool()) return;
  const QString name = cur->data(Qt::UserRole).toString();

  // Only allow deleting custom groups, not built-in zones
  bool isCustom = false;
  for (const auto& cg : MOPlugin::pluginINI().customGroups()) {
    if (cg.name == name) { isCustom = true; break; }
  }
  if (!isCustom) {
    QMessageBox::information(this, tr("Group Manager"),
                             tr("Built-in zones cannot be deleted."));
    return;
  }

  const auto reply = QMessageBox::question(
      this, tr("Delete Group"),
      tr("Delete custom group \"%1\"? Plugins in it will become Unassigned.").arg(name),
      QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) return;

  // Move plugins out of this group
  QModelIndexList indices;
  for (int i = 0; i < m_PluginList->pluginCount(); ++i) {
    if (const auto* p = m_PluginList->getPlugin(i)) {
      if (p->group() == name) indices.append(m_Model->index(i, 0));
    }
  }
  if (!indices.isEmpty()) m_Model->setGroup(indices, QString());

  MOPlugin::pluginINI().removeCustomGroup(name);
  populate();
}

}  // namespace BSPluginList
