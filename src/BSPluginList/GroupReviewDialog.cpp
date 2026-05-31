#include "GroupReviewDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString GroupReviewDialog::confidenceDots(int confidence)
{
  // Returns 1-5 filled/empty dots representing classification confidence
  const int filled = std::clamp((confidence + 10) / 20, 1, 5);
  return QString(u'●').repeated(filled) + QString(u'○').repeated(5 - filled);
}

QString GroupReviewDialog::originLabel(const QString& plugin, const QString& origin)
{
  // Plain text only — QTreeWidget items don't render HTML
  if (origin.isEmpty() || origin == plugin) {
    return plugin;
  }
  return u"%1  (%2)"_s.arg(plugin, origin);
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

GroupReviewDialog::GroupReviewDialog(const QList<PatchSuggestion>& patches,
                                     const QList<GroupSuggestion>& groups,
                                     QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(tr("Review Load Order"));
  setMinimumSize(820, 560);

  auto* root = new QVBoxLayout(this);
  root->setSpacing(8);

  // --- Summary label ---
  m_SummaryLabel = new QLabel(
      tr("LOOT has sorted your plugins. Review the suggestions below and confirm "
         "which to apply. Items with more dots (●●●●●) are higher confidence."),
      this);
  m_SummaryLabel->setWordWrap(true);
  root->addWidget(m_SummaryLabel);

  // --- Tab widget ---
  auto* tabs = new QTabWidget(this);
  root->addWidget(tabs, 1);

  // Patch order tab
  auto* patchWidget = new QWidget;
  auto* patchLayout = new QVBoxLayout(patchWidget);
  patchLayout->setContentsMargins(0, 4, 0, 0);

  auto* patchInfo = new QLabel(
      tr("These plugins appear to patch another mod but don't declare it as a master. "
         "Confirm which ones should load after their target."),
      patchWidget);
  patchInfo->setWordWrap(true);
  patchLayout->addWidget(patchInfo);

  m_PatchTree = new QTreeWidget(patchWidget);
  m_PatchTree->setHeaderLabels({tr("Patch Plugin"), tr("Confidence"), tr("Patches →"), tr("Records")});
  m_PatchTree->setRootIsDecorated(false);
  m_PatchTree->setAlternatingRowColors(true);
  m_PatchTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_PatchTree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
  m_PatchTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  m_PatchTree->header()->setSectionResizeMode(3, QHeaderView::Fixed);
  m_PatchTree->header()->resizeSection(1, 90);
  m_PatchTree->header()->resizeSection(3, 80);
  buildPatchTab(patches);
  patchLayout->addWidget(m_PatchTree, 1);

  m_PatchGroup = new QCheckBox(tr("Add confirmed patches to a \"Patches\" group"), patchWidget);
  m_PatchGroup->setChecked(true);
  patchLayout->addWidget(m_PatchGroup);

  const int patchChecked = std::ranges::count_if(
      patches, [](const PatchSuggestion& p) { return p.preChecked; });
  tabs->addTab(patchWidget,
               tr("Patch Order (%1 / %2)").arg(patchChecked).arg(patches.size()));

  // Group suggestions tab
  auto* groupWidget = new QWidget;
  auto* groupLayout = new QVBoxLayout(groupWidget);
  groupLayout->setContentsMargins(0, 4, 0, 0);

  auto* groupInfo = new QLabel(
      tr("These plugins haven't been assigned to a group. We've suggested categories "
         "based on what records they edit and what archives they load. "
         "Uncheck any you'd like to assign manually."),
      groupWidget);
  groupInfo->setWordWrap(true);
  groupLayout->addWidget(groupInfo);

  m_GroupTree = new QTreeWidget(groupWidget);
  m_GroupTree->setHeaderLabels({tr("Plugin"), tr("Suggested Group"), tr("Confidence"), tr("Reason")});
  m_GroupTree->setRootIsDecorated(false);
  m_GroupTree->setAlternatingRowColors(true);
  m_GroupTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_GroupTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_GroupTree->header()->setSectionResizeMode(2, QHeaderView::Fixed);
  m_GroupTree->header()->setSectionResizeMode(3, QHeaderView::Stretch);
  m_GroupTree->header()->resizeSection(2, 90);
  buildGroupTab(groups);
  groupLayout->addWidget(m_GroupTree, 1);

  auto* archiveNote = new QLabel(
      tr("<b>📦 Archive loaders</b> — plugins marked with this icon exist only to load "
         "BSA/BA2 archives. Their load order position has minimal effect; "
         "adjust their position in the <b>left panel (mod list)</b> to control "
         "which textures/meshes win."),
      groupWidget);
  archiveNote->setWordWrap(true);
  archiveNote->setStyleSheet(u"color: gray; font-size: small;"_s);
  groupLayout->addWidget(archiveNote);

  const int groupChecked = std::ranges::count_if(
      groups, [](const GroupSuggestion& g) { return g.preChecked && !g.alreadyGrouped; });
  const int groupTotal = std::ranges::count_if(
      groups, [](const GroupSuggestion& g) { return !g.alreadyGrouped; });
  tabs->addTab(groupWidget,
               tr("Group Suggestions (%1 / %2)").arg(groupChecked).arg(groupTotal));

  // --- Backup tip ---
  auto* backupTip = new QLabel(
      tr("<small><i>Tip: use the <b>Save</b> button in the toolbar to create a "
         "timestamped backup before applying changes — you can restore it at any "
         "time from the same button.</i></small>"),
      this);
  backupTip->setWordWrap(true);
  backupTip->setStyleSheet(u"color: gray;"_s);
  root->addWidget(backupTip);

  // --- Buttons ---
  auto* buttons = new QDialogButtonBox(this);
  auto* apply   = buttons->addButton(tr("Apply Selected"), QDialogButtonBox::AcceptRole);
  auto* skip    = buttons->addButton(tr("Skip"), QDialogButtonBox::RejectRole);
  Q_UNUSED(apply); Q_UNUSED(skip);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);
}

// ---------------------------------------------------------------------------
// Build patch tab rows
// ---------------------------------------------------------------------------

void GroupReviewDialog::buildPatchTab(const QList<PatchSuggestion>& patches)
{
  // Sort by record count descending (highest confidence first)
  QList<PatchSuggestion> sorted = patches;
  std::ranges::sort(sorted, [](const PatchSuggestion& a, const PatchSuggestion& b) {
    return a.recordCount > b.recordCount;
  });

  for (const auto& p : sorted) {
    const int confidence = std::clamp(p.recordCount * 2, 0, 100);
    auto* item = new QTreeWidgetItem(m_PatchTree);
    item->setCheckState(0, p.preChecked ? Qt::Checked : Qt::Unchecked);
    item->setText(0, originLabel(p.patchPlugin, p.patchOrigin));
    item->setText(1, confidenceDots(confidence));
    item->setText(2, originLabel(p.targetPlugin, p.targetOrigin));
    item->setText(3, tr("%1 records").arg(p.recordCount));
    item->setData(0, Qt::UserRole, p.patchPlugin);
    item->setData(2, Qt::UserRole, p.targetPlugin);
  }
}

// ---------------------------------------------------------------------------
// Build group tab rows
// ---------------------------------------------------------------------------

void GroupReviewDialog::buildGroupTab(const QList<GroupSuggestion>& groups)
{
  // Group by zone, sort by confidence within zone
  QList<GroupSuggestion> sorted = groups;
  std::ranges::sort(sorted, [](const GroupSuggestion& a, const GroupSuggestion& b) {
    if (a.classification.zone != b.classification.zone) {
      return static_cast<int>(a.classification.zone) <
             static_cast<int>(b.classification.zone);
    }
    return a.classification.confidence > b.classification.confidence;
  });

  for (const auto& g : sorted) {
    if (g.alreadyGrouped) continue;

    auto* item = new QTreeWidgetItem(m_GroupTree);
    item->setCheckState(0, g.preChecked ? Qt::Checked : Qt::Unchecked);

    // Plugin + origin
    const QString display = g.modOrigin.isEmpty() || g.modOrigin == g.pluginName
        ? g.pluginName
        : u"%1  (%2)"_s.arg(g.pluginName, g.modOrigin);
    item->setText(0, display);

    // Group name with archive note
    QString groupDisplay = g.classification.groupName;
    if (g.classification.isArchiveLoader) {
      groupDisplay = u"📦 "_s + groupDisplay;
    }
    item->setText(1, groupDisplay);
    item->setText(2, confidenceDots(g.classification.confidence));
    item->setText(3, g.classification.reason);
    item->setData(0, Qt::UserRole, g.pluginName);
    item->setData(1, Qt::UserRole, g.classification.groupName);  // clean name
  }
}

// ---------------------------------------------------------------------------
// Result accessors
// ---------------------------------------------------------------------------

QList<GroupReviewDialog::PatchSuggestion> GroupReviewDialog::confirmedPatches() const
{
  QList<PatchSuggestion> result;
  for (int i = 0; i < m_PatchTree->topLevelItemCount(); ++i) {
    const auto* item = m_PatchTree->topLevelItem(i);
    if (item->checkState(0) == Qt::Checked) {
      PatchSuggestion p;
      p.patchPlugin  = item->data(0, Qt::UserRole).toString();
      p.targetPlugin = item->data(2, Qt::UserRole).toString();
      p.recordCount  = item->text(3).split(u' ').first().toInt();
      result.append(p);
    }
  }
  return result;
}

QList<GroupReviewDialog::GroupSuggestion> GroupReviewDialog::confirmedGroups() const
{
  QList<GroupSuggestion> result;
  for (int i = 0; i < m_GroupTree->topLevelItemCount(); ++i) {
    const auto* item = m_GroupTree->topLevelItem(i);
    if (item->checkState(0) == Qt::Checked) {
      GroupSuggestion g;
      g.pluginName             = item->data(0, Qt::UserRole).toString();
      // Group name stored separately in UserRole+1 to avoid stripping display prefix
      g.classification.groupName = item->data(1, Qt::UserRole).toString();
      result.append(g);
    }
  }
  return result;
}

bool GroupReviewDialog::createPatchGroup() const
{
  return m_PatchGroup->isChecked();
}

}  // namespace BSPluginList
