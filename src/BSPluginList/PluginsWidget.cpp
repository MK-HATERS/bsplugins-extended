#include "PluginsWidget.h"

#include "BSPluginInfo/PluginInfoDialog.h"
#include "GUI/ListDialog.h"
#include "GUI/MessageDialog.h"
#include "GUI/SelectionDialog.h"
#include "MOPlugin/Settings.h"
#include "BSPluginsLog.h"
#include "CustomGroupDialog.h"
#include "GroupManagerDialog.h"
#include "GroupReviewDialog.h"
#include "LootUserlistDialog.h"
#include "MOPlugin/BSPlugins.h"
#include "MOPlugin/BSPluginsINI.h"
#include "TESData/PluginClassifier.h"
#include "UpdateChecker.h"
#include "UpdateDialog.h"
#include "WelcomeDialog.h"
#include "MOTools/Loot.h"
#include "MOTools/LootGroups.h"
#include "PluginListContextMenu.h"
#include "PluginSortFilterProxyModel.h"
#include "ui_pluginswidget.h"

#include <game_features/igamefeatures.h>

#include <boost/range/adaptor/reversed.hpp>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QHBoxLayout>
#include <QIcon>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <QToolTip>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

PluginsWidget::PluginsWidget(MOBase::IOrganizer* organizer,
                             IPanelInterface* panelInterface, QWidget* parent)
    : QWidget(parent), ui{new Ui_PluginsWidget()}, m_PanelInterface{panelInterface},
      m_Organizer{organizer}
{
  ui->setupUi(this);

  m_PluginList      = new TESData::PluginList(organizer);
  m_PluginListModel = new PluginListModel(m_PluginList);
  m_SortProxy       = new PluginSortFilterProxyModel();
  m_SortProxy->setSourceModel(m_PluginListModel);
  m_GroupProxy = new PluginGroupProxyModel(organizer);
  m_GroupProxy->setSourceModel(m_SortProxy);
  ui->pluginList->setModel(m_SortProxy);
  ui->pluginList->setup();
  ui->pluginList->sortByColumn(PluginListModel::COL_PRIORITY, Qt::AscendingOrder);
  optionsMenu = listOptionsMenu();
  ui->listOptionsBtn->setMenu(optionsMenu);

  // ---- Supplementary tab panel: Info / Log / Settings ----
  // The plugin list stays always-visible as the main content; we add a
  // compact tabbed section below it for Info, Log and Settings.
  // (No "Plugins" tab here — this panel IS the Plugins tab.)
  {
    auto* rootLayout      = qobject_cast<QVBoxLayout*>(layout());
    auto* pluginsContainer = findChild<QWidget*>(u"pluginsContainer"_s);

    if (rootLayout && pluginsContainer) {
      auto* innerTabs = new QTabWidget(this);
      innerTabs->setDocumentMode(true);

      // ── Tab 1: Info ───────────────────────────────────────────────────
      // Shows conflict/classification details for the selected plugin.
      // Syncs via the pluginList selection model.
      auto* infoPage = new QWidget(innerTabs);
      auto* infoLay  = new QVBoxLayout(infoPage);
      infoLay->setContentsMargins(4, 4, 4, 4);
      infoLay->setSpacing(4);

      auto* infoHint = new QLabel(
          tr("Select a plugin in the list above to see details here."), infoPage);
      infoHint->setAlignment(Qt::AlignCenter);
      infoHint->setWordWrap(true);
      infoHint->setStyleSheet(u"color: gray;"_s);

      m_InfoBrowser = new QTextBrowser(infoPage);
      m_InfoBrowser->setOpenLinks(false);
      m_InfoBrowser->hide();
      // Handle [Edit] link for .bs file editing
      connect(m_InfoBrowser, &QTextBrowser::anchorClicked, this,
              [this](const QUrl& url) {
                const QString href = url.toString();
                // Open LOOT userlist editor for the selected plugin
                if (href == u"add_loot_rule"_s) {
                  const auto sel = ui->pluginList->selectionModel()->selectedIndexes();
                  if (sel.isEmpty()) return;
                  const auto* plugin = m_PluginList->getPlugin(
                      sel.first().data(PluginListModel::IndexRole).toInt());
                  if (!plugin) return;
                  const auto profilePath = QDir(m_Organizer->profilePath());
                  const QString userlist = QDir::cleanPath(
                      profilePath.absoluteFilePath(u"../../../LOOT/games/%1/userlist.yaml"_s
                          .arg(m_Organizer->managedGame()
                                   ? m_Organizer->managedGame()->gameName()
                                   : u"Starfield"_s)));
                  LootUserlistDialog dlg(plugin->name(), userlist, topLevelWidget());
                  if (dlg.exec() == QDialog::Accepted)
                    bsLog(tr("LOOT rule saved for %1.").arg(plugin->name()));
                  return;
                }
                if (href != u"edit_bs"_s) return;
                // Find the currently-selected plugin's .bs path and open it
                const auto sel = ui->pluginList->selectionModel()->selectedIndexes();
                if (sel.isEmpty()) return;
                const auto* plugin = m_PluginList->getPlugin(
                    sel.first().data(PluginListModel::IndexRole).toInt());
                if (!plugin || !plugin->hasBsHint()) return;
                const QString bsPath = m_Organizer->resolvePath(
                    plugin->name() + QStringLiteral(".bs"));
                if (bsPath.isEmpty()) {
                  // No .bs yet — create one with the current hint
                  const QString newPath = m_Organizer->profilePath() +
                      QStringLiteral("/bsplugins_overrides/") +
                      plugin->name() + QStringLiteral(".bs");
                  QDir().mkpath(QFileInfo(newPath).absolutePath());
                  QFile f(newPath);
                  if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream ts(&f);
                    ts << "group=" << plugin->bsGroupHint() << "\n";
                    if (!plugin->bsZoneHint().isEmpty())
                      ts << "zone=" << plugin->bsZoneHint() << "\n";
                  }
                  QDesktopServices::openUrl(QUrl::fromLocalFile(newPath));
                } else {
                  QDesktopServices::openUrl(QUrl::fromLocalFile(bsPath));
                }
              });

      infoLay->addWidget(infoHint);
      infoLay->addWidget(m_InfoBrowser, 1);
      innerTabs->addTab(infoPage, tr("Info"));
      innerTabs->setTabToolTip(0,
          tr("Details for the selected plugin: type, classification, what it "
             "overrides, what overrides it, inferred patch target."));

      // Sync Info tab when selection changes in the plugin list
      connect(ui->pluginList->selectionModel(),
              &QItemSelectionModel::selectionChanged,
              this, [this, infoHint](const QItemSelection& sel, const QItemSelection&) {
                if (sel.isEmpty()) {
                  m_InfoBrowser->hide();
                  infoHint->show();
                  return;
                }
                const auto* plugin = m_PluginList->getPlugin(
                    ui->pluginList->model()
                        ->data(sel.indexes().first(), PluginListModel::IndexRole)
                        .toInt());
                if (!plugin) return;
                infoHint->hide();
                m_InfoBrowser->show();
                refreshInfoTab(plugin);
              });

      // ── Tab 2: Log ────────────────────────────────────────────────────
      auto* logPage = new QWidget(innerTabs);
      auto* logLay  = new QVBoxLayout(logPage);
      logLay->setContentsMargins(2, 2, 2, 2);
      logLay->setSpacing(2);

      // Filter toolbar
      auto* logToolbar = new QWidget(logPage);
      auto* logTbar    = new QHBoxLayout(logToolbar);
      logTbar->setContentsMargins(0, 0, 0, 0);

      auto* logProxy = new QSortFilterProxyModel(this);
      logProxy->setSourceModel(&BSPluginsLog::instance());
      logProxy->setFilterRole(BSPluginsLog::LevelRole);

      for (auto [label, minLvl] : {
               std::pair{tr("All"),      -1},
               std::pair{tr("⚠ Warn"),   static_cast<int>(LogEntry::Level::Warning)},
               std::pair{tr("✕ Crit"),   static_cast<int>(LogEntry::Level::Critical)},
           }) {
        auto* btn = new QPushButton(label, logToolbar);
        btn->setCheckable(true);
        btn->setFlat(true);
        const int lvl = minLvl;
        connect(btn, &QPushButton::toggled, this, [logProxy, lvl](bool on) {
          if (!on) return;
          if (lvl < 0) {
            // "All" — remove filter
            logProxy->setFilterRegularExpression(QString());
          } else if (lvl == static_cast<int>(LogEntry::Level::Warning)) {
            // ≥ Warning: match "1" (Warning) or "2" (Critical)
            logProxy->setFilterRegularExpression(QStringLiteral("[12]"));
          } else {
            // ≥ Critical: exact match "2"
            logProxy->setFilterRegularExpression(QStringLiteral("2"));
          }
        });
        logTbar->addWidget(btn);
      }
      logTbar->addStretch();
      auto* clearBtn = new QPushButton(tr("Clear"), logToolbar);
      clearBtn->setFlat(true);
      connect(clearBtn, &QPushButton::clicked, &BSPluginsLog::instance(),
              &BSPluginsLog::clear);
      logTbar->addWidget(clearBtn);
      logLay->addWidget(logToolbar);

      auto* logView = new QListView(logPage);
      logView->setModel(logProxy);
      logView->setAlternatingRowColors(true);
      logView->setSelectionMode(QAbstractItemView::SingleSelection);
      logLay->addWidget(logView, 1);

      innerTabs->addTab(logPage, tr("Log"));
      innerTabs->setTabToolTip(1,
          tr("BSPlugins-only messages: warnings, classification decisions, "
             "update check results. Nothing is written to MO2's main log."));

      // ── Tab 3: Settings ───────────────────────────────────────────────
      // Inline settings backed by BSPluginsINI — no separate dialog needed.
      auto* settingsPage = buildSettingsTab(innerTabs);
      innerTabs->addTab(settingsPage, tr("Settings"));
      innerTabs->setTabToolTip(2,
          tr("Configure group names, patch detection threshold, update URL. "
             "Changes save immediately to settings.ini and survive plugin updates."));

      // Plugin list above (stretches), supplementary panel below (fixed start)
      auto* splitter = new QSplitter(Qt::Vertical, this);
      splitter->addWidget(pluginsContainer);
      splitter->addWidget(innerTabs);
      // Plugin list gets most of the space; supplementary panel starts small
      splitter->setSizes({10000, 180});
      splitter->setCollapsible(0, false);  // list is never fully collapsed
      splitter->setHandleWidth(5);

      rootLayout->addWidget(splitter);
    }
  }

  // Update Sort button tooltip to explain two-path workflow
  ui->sortButton->setToolTip(
      tr("<b>LOOT Sort</b> — Full load order sort using LOOT's masterlist rules.<br><br>"
         "Use this when:<br>"
         "• You've added many mods at once<br>"
         "• Your load order needs a full reset<br>"
         "• You haven't sorted in a while<br><br>"
         "<i>For just a few new mods, use the <b>Patch Sort</b> button instead.</i>"));

  // Patch Sort button: quick inferred-ordering pass without running LOOT
  auto* patchSortBtn = new QPushButton(tr("Patch Sort"), this);
  patchSortBtn->setObjectName(u"patchSortBtn"_s);
  patchSortBtn->setToolTip(
      tr("<b>Patch Sort</b> — Quickly reorder patches after adding a few mods.<br><br>"
         "Use this when:<br>"
         "• You've added 1–5 new mods<br>"
         "• Your existing load order is already good<br>"
         "• You just want new patches placed correctly<br><br>"
         "Detects mods overriding records from another without declaring it as a "
         "master, then moves them to load after their target.<br><br>"
         "<i>For a full re-sort, use the <b>Sort</b> (LOOT) button.</i>"));
  patchSortBtn->setVisible(Settings::instance()->enableSortButton());
  connect(patchSortBtn, &QPushButton::clicked, this, [this]() {
    m_PluginListModel->applyInferredOrdering();
    bsLog(tr("Patch Sort complete."));
  });

  // Insert Patch Sort next to the Sort button in the toolbar
  if (auto* toolLayout = ui->sortButton->parentWidget()
                             ? ui->sortButton->parentWidget()->layout()
                             : nullptr) {
    const int sortIdx = toolLayout->indexOf(ui->sortButton);
    if (sortIdx >= 0) {
      if (auto* hbox = qobject_cast<QHBoxLayout*>(toolLayout)) {
        hbox->insertWidget(sortIdx + 1, patchSortBtn);
      }
    }
  }

  // MO2's Sort (LOOT) button is always visible — we never hide it.
  // Only our own Patch Sort button respects the enableSortButton setting.
  updateGroupActionVisibility();

  // Show welcome / changelog dialog and kick off update check after UI is ready
  organizer->onUserInterfaceInitialized([this](QMainWindow*) {
    checkVersionOnStartup();

    // Async update check — fires updateAvailable() if a newer version exists
    const QString currentVer = u"2.9.b"_s;
    auto* checker = new UpdateChecker(currentVer, this);
    connect(checker, &UpdateChecker::updateAvailable, this,
            [this, currentVer](const QString& latest, const QString& url) {
              bsWarn(tr("Update available: v%1 → v%2").arg(currentVer, latest));
              if (MOPlugin::pluginINI().skipVersion() == latest) return;
              UpdateDialog dlg(latest, url, topLevelWidget());
              dlg.exec();
            });
    connect(checker, &UpdateChecker::checkFailed, this, [this]() {
      bsLog(tr("Update check failed — check your network connection."));
    });
    checker->check();

    // Log a startup notice so the panel shows something on first open
    bsLog(tr("BSPlugins Extended v2.9 Beta ready. Run LOOT sort to classify plugins."));
  });

    auto* const sortShortcut = new QShortcut(QKeySequence(tr("Ctrl+Shift+S")), this);
    sortShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(sortShortcut, &QShortcut::activated, this,
      &PluginsWidget::on_sortButton_clicked);

    auto* const cleanShortcut =
        new QShortcut(QKeySequence(tr("Ctrl+Shift+G")), this);
    cleanShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(cleanShortcut, &QShortcut::activated, this,
      &PluginsWidget::on_cleanGroupsButton_clicked);

    auto* const resetShortcut =
        new QShortcut(QKeySequence(tr("Ctrl+Alt+G")), this);
    resetShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(resetShortcut, &QShortcut::activated, this,
      &PluginsWidget::on_resetGroupsButton_clicked);

    auto* const renameGroupShortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
    renameGroupShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(renameGroupShortcut, &QShortcut::activated, this,
      &PluginsWidget::renameSelectedGroup);

    auto* const removeGroupShortcut =
        new QShortcut(QKeySequence::Delete, ui->pluginList);
    removeGroupShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(removeGroupShortcut, &QShortcut::activated, this,
      &PluginsWidget::removeSelectedGroup);

    auto* const mergeGroupShortcut =
        new QShortcut(QKeySequence(tr("Ctrl+Shift+M")), this);
    mergeGroupShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(mergeGroupShortcut, &QShortcut::activated, this,
      &PluginsWidget::mergeSelectedGroup);

  if (Settings::instance()->autoCleanGroupSeparatorsOnStartup()) {
    m_PluginListModel->cleanEmptyGroups();
  }


  topLevelWidget()->installEventFilter(this);
  ui->activePluginsCounter->installEventFilter(this);
  ui->activePluginsCounter->setCursor(Qt::PointingHandCursor);

  restoreState();

  connect(m_PluginList, &TESData::PluginList::pluginsListChanged, this,
          &PluginsWidget::updatePluginCount);

  // When the plugin list refreshes, log warnings and a health score
  connect(m_PluginList, &TESData::PluginList::pluginsListChanged, this, [this]() {
    BSPluginsLog::instance().clear();
    const int count = m_PluginList->pluginCount();

    // Per-plugin warnings
    int missingMasterCount = 0;
    int invalidFormIdCount = 0;
    for (int i = 0; i < count; ++i) {
      const auto* p = m_PluginList->getPlugin(i);
      if (!p || !p->enabled()) continue;
      if (p->hasInvalidFormIds()) {
        ++invalidFormIdCount;
        bsWarn(tr("ESL/ESH with out-of-range ObjectIDs — broken CK export"), p->name());
      }
      if (p->isBlueprintFlagged() && !p->isBlueprintPrefixed())
        bsWarn(tr("Blueprint-flagged but wrong filename prefix — game can't load it"), p->name());
      if (p->isBlueprintPrefixed() && !p->isBlueprintFlagged())
        bsWarn(tr("Blueprint-prefixed but missing blueprint flag — unintended autoload"), p->name());
      if (p->hasMissingMasters()) {
        ++missingMasterCount;
        bsCrit(tr("Missing masters: %1")
                   .arg(QStringList(p->missingMasters().begin(),
                                    p->missingMasters().end())
                            .join(u", "_s)),
               p->name());
      }
    }

    // Health score: count patches in order, patches needing attention, unclassified
    const int threshold    = MOPlugin::pluginINI().patchThreshold();
    const int dispThresh   = std::max(1, threshold / 6);
    const QString prefix   = m_PluginList->blueprintPrefix();
    int patchesOk = 0, patchesWrong = 0, unclassified = 0, noLoot = 0;
    for (int i = 0; i < count; ++i) {
      const auto* p = m_PluginList->getPlugin(i);
      if (!p || !p->enabled() || p->forceLoaded()) continue;

      // Inferred patch status
      const auto& inf = p->getInferredOverrides();
      const auto maxIt = std::max_element(inf.constBegin(), inf.constEnd());
      if (maxIt != inf.constEnd() && maxIt.value() >= dispThresh) {
        const auto* target = m_PluginList->getPlugin(maxIt.key());
        if (target) {
          if (p->priority() > target->priority()) ++patchesOk;
          else                                     ++patchesWrong;
        }
      }

      // Classification
      const TESData::Classification cls = TESData::classifyPlugin(*p, prefix);
      if (cls.zone == TESData::PluginZone::Unknown) ++unclassified;

      // LOOT masterlist coverage
      if (!m_PluginList->getLootReport(p->name())) ++noLoot;
    }

    bsLog(tr("Plugin list updated — %1 active | "
             "Patches: %2 ok, %3 need attention | "
             "Unclassified: %4 | Not in LOOT masterlist: %5")
              .arg(count).arg(patchesOk).arg(patchesWrong)
              .arg(unclassified).arg(noLoot));
  });

  connect(m_PluginListModel, &PluginListModel::pluginStatesChanged, ui->pluginList,
          &PluginListView::updateOverwriteMarkers);
  connect(m_PluginListModel, &PluginListModel::pluginOrderChanged, ui->pluginList,
          &PluginListView::updateOverwriteMarkers);
  connect(m_PluginListModel, &QAbstractItemModel::modelReset, ui->pluginList,
          &PluginListView::clearOverwriteMarkers);

  connect(m_PluginListModel, &PluginListModel::pluginStatesChanged, this,
          &PluginsWidget::updatePluginCount);

  connect(m_PluginListModel, &QAbstractItemModel::dataChanged, this,
          [this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
            if (m_IsRunningApp || m_PluginList->isRefreshing()) {
              return;
            }


            m_PluginList->writePluginLists();
          });

  connect(m_GroupProxy, &QAbstractItemModel::modelReset, [this]() {
    if (Settings::instance()->enablePluginGrouping()) {
      Settings::instance()->restoreTreeExpandState(ui->pluginList);
    }

    QTimer::singleShot(0, this, [this]() { restoreScrollPosition(); });
  });

  connect(ui->pluginList, &QTreeView::collapsed, this,
          &PluginsWidget::onGroupCollapsed);
  connect(ui->pluginList, &QTreeView::expanded, this, &PluginsWidget::onGroupExpanded);

  panelInterface->onPanelActivated(
      std::bind_front(&PluginsWidget::onPanelActivated, this));
  panelInterface->onSelectedOriginsChanged(
      std::bind_front(&PluginsWidget::onSelectedOriginsChanged, this));

  organizer->onAboutToRun(std::bind_front(&PluginsWidget::onAboutToRun, this));
  organizer->onFinishedRun(std::bind_front(&PluginsWidget::onFinishedRun, this));

  organizer->modList()->onModStateChanged(
      std::bind_front(&PluginsWidget::onModStateChanged, this));

  Settings::instance()->onSettingChanged(
      std::bind_front(&PluginsWidget::onSettingChanged, this));

  synchronizePluginLists(organizer);
  updatePluginCount();
}

PluginsWidget::~PluginsWidget() noexcept
{
  delete ui;
  delete optionsMenu;

  delete m_PluginList;
  delete m_PluginListModel;
  delete m_SortProxy;
  delete m_GroupProxy;
}

void PluginsWidget::updatePluginCount()
{
  int activeMasterCount       = 0;
  int activeLightMasterCount  = 0;
  int activeMediumMasterCount = 0;
  int activeOverlayCount      = 0;
  int activeRegularCount      = 0;
  int activeBlueprintCount    = 0;
  int masterCount             = 0;
  int lightMasterCount        = 0;
  int mediumMasterCount       = 0;
  int overlayCount            = 0;
  int regularCount            = 0;
  int blueprintCount          = 0;
  int activeVisibleCount      = 0;
  int conflictCount           = 0;

    const auto gameFeatures = m_Organizer->gameFeatures();
    const auto tesSupport = gameFeatures ? gameFeatures->gameFeature<MOBase::GamePlugins>() : nullptr;

  const bool lightPluginsAreSupported =
      tesSupport && tesSupport->lightPluginsAreSupported();
  const bool mediumPluginsAreSupported =
      tesSupport && tesSupport->mediumPluginsAreSupported();
  const bool blueprintPluginsAreSupported =
      tesSupport && tesSupport->blueprintPluginsAreSupported();
  const bool conflictManagementEnabled =
      Settings::instance()->enablePluginConflictManagement();

  for (int i = 0, count = m_PluginListModel->rowCount(); i < count; ++i) {
    const auto index = m_PluginListModel->index(i, 0);
    const auto id    = index.data(PluginListModel::IndexRole).toInt();
    const auto info  = m_PluginList->getPlugin(id);

    if (!info)
      continue;

    const bool active  = info->enabled() || info->isAlwaysEnabled();
    const bool visible = m_SortProxy->filterAcceptsRow(index.row(), index.parent());
    if (conflictManagementEnabled &&
        info->conflictState() != TESData::FileInfo::CONFLICT_NONE)
      ++conflictCount;

    // Blueprint count is separate — blueprints are also counted as masters/ESHs
    if (info->isBlueprintFlagged() || info->isBlueprintPrefixed()) {
      ++blueprintCount;
      activeBlueprintCount += active ? 1 : 0;
    }

    if (info->isMediumFlagged()) {
      ++mediumMasterCount;
      activeMediumMasterCount += active ? 1 : 0;
      activeVisibleCount += visible && active ? 1 : 0;
    } else if (info->isSmallFile()) {
      ++lightMasterCount;
      activeLightMasterCount += active ? 1 : 0;
      activeVisibleCount += visible && active ? 1 : 0;
    } else if (info->isMasterFile()) {
      ++masterCount;
      activeMasterCount += active ? 1 : 0;
      activeVisibleCount += visible && active ? 1 : 0;
    } else if (info->isOverlayFlagged()) {
      ++overlayCount;
      activeOverlayCount += active ? 1 : 0;
      activeVisibleCount += visible && active ? 1 : 0;
    } else {
      ++regularCount;
      activeRegularCount += active ? 1 : 0;
      activeVisibleCount += visible && active ? 1 : 0;
    }
  }

  const int activeCount = activeMasterCount + activeLightMasterCount +
                          activeMediumMasterCount + activeOverlayCount +
                          activeRegularCount;
  const int totalCount  = masterCount + lightMasterCount + mediumMasterCount +
                          overlayCount + regularCount;

  ui->activePluginsCounter->display(activeVisibleCount);

  QString toolTip;
  toolTip.reserve(700);
  toolTip += uR"(<table cellspacing="6">)"_s
             uR"(<tr><th>%1</th><th>%2</th><th>%3</th></tr>)"_s.arg(tr("Type"))
                 .arg(tr("Active"), -12)
                 .arg(tr("Total"));

  const QString row = uR"(<tr><td>%1:</td><td align=right>%2    </td>)"_s
                      uR"(<td align=right>%3</td></tr>)"_s;

  toolTip += row.arg(tr("All plugins")).arg(activeCount).arg(totalCount);
  toolTip += row.arg(tr("ESMs")).arg(activeMasterCount).arg(masterCount);
  toolTip += row.arg(tr("ESPs")).arg(activeRegularCount).arg(regularCount);
  toolTip += row.arg(tr("ESMs+ESPs"))
                 .arg(activeMasterCount + activeRegularCount)
                 .arg(masterCount + regularCount);
  if (mediumPluginsAreSupported)
    toolTip +=
        row.arg(tr("ESHs")).arg(activeMediumMasterCount).arg(mediumMasterCount);
  if (lightPluginsAreSupported)
    toolTip += row.arg(tr("ESLs")).arg(activeLightMasterCount).arg(lightMasterCount);
  if (blueprintPluginsAreSupported)
    toolTip +=
        row.arg(tr("Blueprint masters")).arg(activeBlueprintCount).arg(blueprintCount);
  if (conflictManagementEnabled && conflictCount > 0)
    toolTip +=
        uR"(<tr><td>%1:</td><td align=right colspan=2>%2</td></tr>)"_s.arg(
            tr("Conflicting plugins"), QString::number(conflictCount));
  toolTip += uR"(</table>)"_s;

  ui->activePluginsCounter->setToolTip(toolTip);
}

void PluginsWidget::on_espFilterEdit_textChanged(const QString& filter)
{
  m_SortProxy->updateFilter(filter);

  if (!filter.isEmpty()) {
    setStyleSheet("QTreeView { border: 2px ridge #f00; }");
    ui->activePluginsCounter->setStyleSheet("QLCDNumber { border: 2px ridge #f00; }");
  } else {
    setStyleSheet("");
    ui->activePluginsCounter->setStyleSheet("");
  }
  updatePluginCount();
}

bool PluginsWidget::eventFilter(QObject* watched, QEvent* event)
{
  if (watched == ui->activePluginsCounter &&
      event->type() == QEvent::MouseButtonRelease) {
    auto* const mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      QToolTip::showText(ui->activePluginsCounter->mapToGlobal(
                             mouseEvent->position().toPoint()),
                         ui->activePluginsCounter->toolTip(),
                         ui->activePluginsCounter);
      return true;
    }
  }

  if (event->type() == QEvent::Close) {
    saveState();
    m_PluginList->writePluginLists();
  }

  return QWidget::eventFilter(watched, event);
}

void PluginsWidget::changeEvent(QEvent* event)
{
  QWidget::changeEvent(event);
  switch (event->type()) {
  case QEvent::LanguageChange:
    ui->retranslateUi(this);
    break;
  default:
    break;
  }
}

void PluginsWidget::onGroupCollapsed(const QModelIndex& index)
{
  if (Settings::instance()->enablePluginGrouping()) {
    Settings::instance()->saveTreeExpandState(ui->pluginList);
  }

  if (ui->pluginList->selectionModel()->isSelected(index)) {
    onSelectionChanged();
  }
}

void PluginsWidget::onGroupExpanded(const QModelIndex& index)
{
  if (Settings::instance()->enablePluginGrouping()) {
    Settings::instance()->saveTreeExpandState(ui->pluginList);
  }

  if (ui->pluginList->selectionModel()->isSelected(index)) {
    onSelectionChanged();
  }
}

void PluginsWidget::onSelectionChanged()
{
  QList<QString> selectedFiles;
  std::function<void(const QModelIndex&)> addFiles;
  addFiles = [&](const QModelIndex& index) {
    if (index.model()->hasChildren(index)) {
      if (ui->pluginList->isExpanded(index)) {
        return;
      }

      for (int i = 0, count = index.model()->rowCount(index); i < count; ++i) {
        addFiles(index.model()->index(i, 0, index));
      }
    } else {
      selectedFiles.append(index.data(Qt::DisplayRole).toString());
    }
  };

  for (const auto& index : ui->pluginList->selectionModel()->selectedRows()) {
    addFiles(index);
  }

  m_PanelInterface->setSelectedFiles(selectedFiles);
}

void PluginsWidget::onPanelActivated()
{
  if (m_DeferPostLootRefresh) {
    m_DeferPostLootRefresh = false;
    QTimer::singleShot(1000, this, [this]() {
      refreshPluginListPreservingScroll();
    });
  }
}

void PluginsWidget::onSelectedOriginsChanged(const QList<QString>& origins)
{
  ui->pluginList->setHighlightedOrigins(origins);
}

void PluginsWidget::toggleHideForceEnabled()
{
  const bool doHide = toggleForceEnabled->isChecked();
  m_SortProxy->hideForceEnabledFiles(doHide);
  updatePluginCount();

  Settings::instance()->set("hide_force_enabled", doHide);
}

void PluginsWidget::toggleIgnoreMasterConflicts()
{
  if (!Settings::instance()->enablePluginConflictManagement()) {
    return;
  }

  const bool doIgnore = toggleIgnoreMasters->isChecked();
  Settings::instance()->set("ignore_master_conflicts", doIgnore);

  m_PluginListModel->invalidateConflicts();
}

constexpr auto PATTERN_BACKUP_GLOB  = R"/(.????_??_??_??_??_??)/";
constexpr auto PATTERN_BACKUP_REGEX = R"/(\.(\d\d\d\d_\d\d_\d\d_\d\d_\d\d_\d\d))/";
constexpr auto PATTERN_BACKUP_DATE  = R"/(yyyy_MM_dd_hh_mm_ss)/";

static QString queryRestore(const QString& filePath, QWidget* parent = nullptr)
{
  QFileInfo pluginFileInfo(filePath);
  QString pattern     = pluginFileInfo.fileName() + ".*";
  QFileInfoList files = pluginFileInfo.absoluteDir().entryInfoList(
      QStringList(pattern), QDir::Files, QDir::Name);

  GUI::SelectionDialog dialog(QObject::tr("Choose backup to restore"), parent);
  QRegularExpression exp(QRegularExpression::anchoredPattern(pluginFileInfo.fileName() +
                                                             PATTERN_BACKUP_REGEX));
  QRegularExpression exp2(
      QRegularExpression::anchoredPattern(pluginFileInfo.fileName() + "\\.(.*)"));
  for (const QFileInfo& info : boost::adaptors::reverse(files)) {
    auto match  = exp.match(info.fileName());
    auto match2 = exp2.match(info.fileName());
    if (match.hasMatch()) {
      QDateTime time = QDateTime::fromString(match.captured(1), PATTERN_BACKUP_DATE);
      dialog.addChoice(time.toString(), "", match.captured(1));
    } else if (match2.hasMatch()) {
      dialog.addChoice(match2.captured(1), "", match2.captured(1));
    }
  }

  if (dialog.numChoices() == 0) {
    QMessageBox::information(parent, QObject::tr("No Backups"),
                             QObject::tr("There are no backups to restore"));
    return QString();
  }

  if (dialog.exec() == QDialog::Accepted) {
    return dialog.getChoiceData().toString();
  } else {
    return QString();
  }
}

void PluginsWidget::displayPluginInformation(const QModelIndex& index)
{
  const int id        = index.data(PluginListModel::IndexRole).toInt();
  const auto fileName = m_PluginList->getPlugin(id)->name();
  const auto parent   = topLevelWidget();
  BSPluginInfo::PluginInfoDialog dialog{m_Organizer, m_PluginList, fileName, parent};
  dialog.exec();

  const bool ignoreMasters =
      Settings::instance()->get<bool>("ignore_master_conflicts", false);
  toggleIgnoreMasters->setChecked(ignoreMasters);
  m_PluginListModel->invalidateConflicts();
}

void PluginsWidget::on_pluginList_customContextMenuRequested(const QPoint& pos)
{
  PluginListContextMenu menu{ui->pluginList->indexAt(pos), m_PluginListModel,
                             ui->pluginList, m_Organizer->modList(), m_PluginList};

  connect(&menu, &PluginListContextMenu::openModInformation,
          [this](const QModelIndex& index) {
            const int id        = index.data(PluginListModel::IndexRole).toInt();
            const auto fileName = m_PluginList->getPlugin(id)->name();
            m_PanelInterface->displayOriginInformation(fileName);
          });

  connect(&menu, &PluginListContextMenu::openPluginInformation, this,
          &PluginsWidget::displayPluginInformation);

  const QPoint p = ui->pluginList->viewport()->mapToGlobal(pos);
  menu.exec(p);
}

void PluginsWidget::on_pluginList_doubleClicked(const QModelIndex& index)
{
  const int column = index.column();


  if (column == PluginListModel::COL_NOTES) {
    ui->pluginList->edit(index);
    return;
  }

  bool ok;
  const int id = index.data(PluginListModel::IndexRole).toInt(&ok);
  if (ok) {
    Qt::KeyboardModifiers modifiers = QApplication::queryKeyboardModifiers();
    if (modifiers.testFlag(Qt::ControlModifier)) {

      const auto origin  = m_PluginList->getOriginName(id);
      const auto modInfo = m_Organizer->modList()->getMod(origin);

      if (modInfo) {
        MOBase::shell::Explore(modInfo->absolutePath());
      }
    } else if (Settings::instance()->doubleClickOpensPluginInfo()) {

      displayPluginInformation(index);
    } else {

      const auto plugin = m_PluginList->getPlugin(id);
      if (plugin) {
        m_PanelInterface->displayOriginInformation(plugin->name());
      }
    }
  } else if (ui->pluginList->model()->hasChildren(index)) {

    ui->pluginList->setExpanded(index, !ui->pluginList->isExpanded(index));
  }
}

void PluginsWidget::on_pluginList_openOriginExplorer(const QModelIndex& index)
{
  const int id       = index.data(PluginListModel::IndexRole).toInt();
  const auto origin  = m_PluginList->getOriginName(id);
  const auto modInfo = m_Organizer->modList()->getMod(origin);

  if (modInfo == nullptr) {
    return;
  }

  MOBase::shell::Explore(modInfo->absolutePath());
}

void PluginsWidget::on_sortButton_clicked()
{
  const auto logLevel = Settings::instance()->lootLogLevel();
  const bool offline  = Settings::instance()->offlineMode();

  auto r = QMessageBox::No;

  if (offline) {
    r = QMessageBox::question(topLevelWidget(), tr("Sorting plugins"),
                              tr("Are you sure you want to sort your plugins list?") +
                                  "\r\n\r\n" +
                                  tr("Note: You are currently in offline mode and LOOT "
                                     "will not update the master list."),
                              QMessageBox::Yes | QMessageBox::No);
  } else {
    r = QMessageBox::question(topLevelWidget(), tr("Sorting plugins"),
                              tr("Are you sure you want to sort your plugins list?"),
                              QMessageBox::Yes | QMessageBox::No);
  }

  if (r != QMessageBox::Yes) {
    return;
  }


  const bool didUpdateMasterList = offline ? true : m_DidUpdateMasterList;

  if (MOTools::runLoot(topLevelWidget(), m_Organizer, m_PluginList, logLevel,
                       didUpdateMasterList, *Settings::instance())) {

    if (!offline) {
      m_DidUpdateMasterList = true;
    }

    importLootGroups();
    m_PluginListModel->invalidate();
    bsLog(tr("LOOT sort complete."));
    showGroupReviewDialog();
  }
}

void PluginsWidget::showGroupReviewDialog()
{
  // Prewarm conflict caches so getInferredOverrides() below doesn't
  // trigger lazy doConflictCheck() for every plugin on the GUI thread.
  const int pluginCount = m_PluginList->pluginCount();
  for (int i = 0; i < pluginCount; ++i) {
    if (const auto* p = m_PluginList->getPlugin(i)) {
      if (p->enabled()) static_cast<void>(p->getInferredOverrides());
    }
  }

  // --- Build zone→user-group-name mapping from already-classified plugins ---
  // If the user renamed "Visuals" to "Rabbit's Lights", new mods classified
  // as Visuals will be suggested under "Rabbit's Lights" instead.
  QMap<TESData::PluginZone, QString> userZoneNames;
  const QString blueprintPfx = m_PluginList->blueprintPrefix();
  for (int i = 0; i < pluginCount; ++i) {
    const auto* p = m_PluginList->getPlugin(i);
    if (!p || p->group().isEmpty() || p->group() == u"default"_s) continue;
    const TESData::Classification cls = TESData::classifyPlugin(*p, blueprintPfx);
    if (cls.zone != TESData::PluginZone::Unknown &&
        !userZoneNames.contains(cls.zone)) {
      userZoneNames[cls.zone] = p->group();
    }
  }

  // Determine which plugins are "new" (not yet reviewed in a previous run).
  // Stored in persistent() per-profile so each profile tracks its own state.
  const QStringList reviewed =
      m_Organizer->persistent(BSPlugins::NAME, u"reviewed_plugins"_s, QStringList())
          .toStringList();
  const bool hasPreviousRun = !reviewed.isEmpty();

  // After LOOT sort always show the full dialog — no pre-prompt.
  // For re-runs after adding a few mods, use Patch Sort button instead.
  // Only skip entirely if there's truly nothing new to suggest.
  // Always full review after LOOT sort — use Patch Sort for minor additions
  if (hasPreviousRun) {
    // Count new plugins to surface a log hint, but don't gate the dialog
    const int newCount = std::ranges::count_if(
        std::views::iota(0, pluginCount), [&](int i) {
          const auto* p = m_PluginList->getPlugin(i);
          return p && !reviewed.contains(p->name(), Qt::CaseInsensitive);
        });
    if (newCount > 0) {
      bsLog(tr("LOOT sort: %1 new plugin(s) since last review.").arg(newCount));
    }
    // Don't gate the dialog on newCount — always show after LOOT sort.
    // The dialog itself filters already-handled suggestions.
  }

  // --- Collect patch suggestions (inferred overrides >= display threshold) ---
  const int patchThreshold = MOPlugin::pluginINI().patchThreshold();
  const int displayThreshold = std::max(1, patchThreshold / 6);
  QList<GroupReviewDialog::PatchSuggestion> patches;
  for (int i = 0; i < pluginCount; ++i) {
    const auto* plugin = m_PluginList->getPlugin(i);
    if (!plugin || !plugin->enabled()) continue;
    const auto& inferred = plugin->getInferredOverrides();
    auto maxIt = std::max_element(inferred.constBegin(), inferred.constEnd());
    if (maxIt == inferred.constEnd() || maxIt.value() < displayThreshold) continue;
    const auto* target = m_PluginList->getPlugin(maxIt.key());
    if (!target) continue;

    // Skip if already correctly ordered (previous run handled it)
    if (plugin->priority() > target->priority()) continue;

    // Skip if already in a named group (user has handled this relationship)
    const QString& grp = plugin->group();
    if (!grp.isEmpty() && grp != u"default"_s) continue;

    GroupReviewDialog::PatchSuggestion ps;
    ps.patchPlugin  = plugin->name();
    ps.patchOrigin  = m_PluginList->getOriginName(i);
    ps.targetPlugin = target->name();
    ps.targetOrigin = m_PluginList->getOriginName(maxIt.key());
    ps.recordCount  = maxIt.value();
    ps.preChecked   = (ps.recordCount >= patchThreshold);
    patches.append(ps);
  }

  // --- Collect group suggestions (unclassified plugins only) ---
  QList<GroupReviewDialog::GroupSuggestion> groupSuggestions;
  const QString prefix = m_PluginList->blueprintPrefix();
  for (int i = 0; i < pluginCount; ++i) {
    const auto* plugin = m_PluginList->getPlugin(i);
    if (!plugin) continue;
    // Force-loaded plugins (base game, DLC) don't need group review
    if (plugin->forceLoaded()) continue;

    // In "new only" mode, skip plugins reviewed in a previous run
    // (no newOnly filter — always show full review after LOOT sort)

    const bool alreadyGrouped = !plugin->group().isEmpty() &&
                                plugin->group() != u"default"_s;
    const TESData::Classification cls = TESData::classifyPlugin(*plugin, prefix);

    GroupReviewDialog::GroupSuggestion gs;
    gs.pluginName      = plugin->name();
    gs.modOrigin       = m_PluginList->getOriginName(i);
    // Override suggested group name with user's custom name for that zone
    TESData::Classification adjusted = cls;
    if (userZoneNames.contains(cls.zone)) {
      adjusted.groupName = userZoneNames.value(cls.zone);
    }
    gs.classification  = adjusted;
    gs.preChecked      = adjusted.confidence >= 70 && !alreadyGrouped;
    gs.alreadyGrouped  = alreadyGrouped;
    groupSuggestions.append(gs);
  }

  // Skip dialog if nothing to suggest
  if (patches.isEmpty() && std::ranges::all_of(groupSuggestions,
        [](const GroupReviewDialog::GroupSuggestion& g) { return g.alreadyGrouped; })) {
    return;
  }

  GroupReviewDialog dlg(patches, groupSuggestions, topLevelWidget());
  if (dlg.exec() != QDialog::Accepted) return;

  // --- Apply only the patches the user confirmed (not all inferred ones) ---
  const auto confirmedPatches = dlg.confirmedPatches();
  if (!confirmedPatches.isEmpty()) {
    QModelIndexList patchIndices;
    for (const auto& p : confirmedPatches) {
      const int patchIdx  = m_PluginList->getIndex(p.patchPlugin);
      const int targetIdx = m_PluginList->getIndex(p.targetPlugin);
      if (patchIdx < 0 || targetIdx < 0) continue;

      const auto* patch  = m_PluginList->getPlugin(patchIdx);
      const auto* target = m_PluginList->getPlugin(targetIdx);
      if (!patch || !target) continue;

      // Only move if the patch currently loads before its target
      if (patch->priority() < target->priority()) {
        m_PluginListModel->sendToPriority(
            {m_PluginListModel->index(patchIdx, 0)}, target->priority() + 1);
      }

      if (dlg.createPatchGroup()) {
        patchIndices.append(m_PluginListModel->index(patchIdx, 0));
      }
    }

    if (!patchIndices.isEmpty()) {
      m_PluginListModel->setGroup(patchIndices, MOPlugin::pluginINI().groupNamePatches());
    }
  }

  // --- Apply confirmed group assignments ---
  for (const auto& gs : dlg.confirmedGroups()) {
    const int idx = m_PluginList->getIndex(gs.pluginName);
    if (idx >= 0) {
      m_PluginListModel->setGroup({m_PluginListModel->index(idx, 0)},
                                  gs.classification.groupName);
    }
  }

  m_PluginListModel->invalidate();

  // Log what was applied
  if (!confirmedPatches.isEmpty()) {
    bsLog(tr("Fix Patch Load Order: moved %1 plugin(s) after their inferred target.")
              .arg(confirmedPatches.size()));
  }
  for (const auto& gs : dlg.confirmedGroups()) {
    bsLog(tr("Grouped: %1 → %2").arg(gs.pluginName, gs.classification.groupName),
          gs.pluginName);
  }

  // Record every plugin shown in this review as "seen" for future re-runs
  // Update the reviewed list: start from previous session's list, add
  // only plugins that were actually shown in this dialog run, and prune
  // any that are no longer installed.
  const QSet<QString> installed = [&]() {
    QSet<QString> s;
    s.reserve(pluginCount);
    for (int i = 0; i < pluginCount; ++i) {
      if (const auto* p = m_PluginList->getPlugin(i)) s.insert(p->name());
    }
    return s;
  }();

  // Carry forward previous entries that are still installed (prunes removed mods)
  QStringList nowReviewed;
  for (const QString& name : reviewed) {
    if (installed.contains(name)) nowReviewed.append(name);
  }
  // Add newly-presented plugins (those shown in this dialog run)
  for (int i = 0; i < pluginCount; ++i) {
    const auto* p = m_PluginList->getPlugin(i);
    if (p && !nowReviewed.contains(p->name(), Qt::CaseInsensitive)) {
      // Only mark shown if they were not force-loaded (force-loaded plugins
      // are skipped in both the patch and group suggestion loops)
      if (!p->forceLoaded()) nowReviewed.append(p->name());
    }
  }
  m_Organizer->setPersistent(BSPlugins::NAME, u"reviewed_plugins"_s,
                              nowReviewed, false);
}

void PluginsWidget::on_resetGroupsButton_clicked()
{
  if (!Settings::instance()->enablePluginGrouping() ||
      !Settings::instance()->enableResetGroupsButton()) {
    return;
  }

  if (!confirmMassOperation(
          tr("Reset all groups and separators? Plugin load order will stay unchanged."))) {
    return;
  }

  m_PluginListModel->resetGroupsStructure();
}

void PluginsWidget::on_cleanGroupsButton_clicked()
{
  if (!Settings::instance()->enablePluginGrouping() ||
      !Settings::instance()->enableCleanGroupsButton()) {
    return;
  }

  m_PluginListModel->cleanEmptyGroups();
}

static bool tryRestore(const QString& filePath, const QString& identifier,
                       bool required, QWidget* parent = nullptr)
{
  const auto backupName = filePath + "." + identifier;
  if (required || QFileInfo::exists(backupName)) {
    return MOBase::shellCopy(backupName, filePath, true, parent);
  } else {
    return !QFileInfo::exists(filePath) || MOBase::shellDeleteQuiet(filePath, parent);
  }
}

void PluginsWidget::on_restoreButton_clicked()
{
  const auto app         = this->topLevelWidget();
  const auto profilePath = QDir(m_Organizer->profilePath());
  const auto pluginsName = QDir::cleanPath(profilePath.absoluteFilePath("plugins.txt"));

  QString choice = queryRestore(pluginsName, app);
  if (!choice.isEmpty()) {
    const auto groupsName =
        QDir::cleanPath(profilePath.absoluteFilePath("plugingroups.txt"));
    const auto loadOrderName =
        QDir::cleanPath(profilePath.absoluteFilePath("loadorder.txt"));

    if (!tryRestore(pluginsName, choice, true, app) ||
        !tryRestore(loadOrderName, choice, true, app) ||
        !tryRestore(groupsName, choice, false, app)) {
      const auto e = ::GetLastError();

      QMessageBox::critical(
          this, tr("Restore failed"),
          tr("Failed to restore the backup. Errorcode: %1")
              .arg(QString::fromStdWString(MOBase::formatSystemMessage(e))));
    }
    m_PluginListModel->invalidate();
  }
}

static bool createBackup(const QString& filePath, const QString& identifier,
                         QWidget* parent = nullptr)
{
  QString outPath = filePath + "." + identifier;
  if (MOBase::shellCopy(QStringList(filePath), QStringList(outPath), parent)) {
    QFileInfo fileInfo(filePath);
    MOBase::removeOldFiles(fileInfo.absolutePath(),
                           fileInfo.fileName() + PATTERN_BACKUP_GLOB, 10, QDir::Name);
    return true;
  } else {
    return false;
  }
}

static bool createBackup(const QString& filePath, const QDateTime& time,
                         QWidget* parent = nullptr)
{
  return createBackup(filePath, time.toString(PATTERN_BACKUP_DATE), parent);
}

void PluginsWidget::on_saveButton_clicked()
{
  m_PluginList->writePluginLists();

  const auto app         = this->topLevelWidget();
  const auto profilePath = QDir(m_Organizer->profilePath());
  const auto pluginsName = QDir::cleanPath(profilePath.absoluteFilePath("plugins.txt"));
  const auto groupsName =
      QDir::cleanPath(profilePath.absoluteFilePath("plugingroups.txt"));
  const auto loadOrderName =
      QDir::cleanPath(profilePath.absoluteFilePath("loadorder.txt"));
  const auto lockedOrderName =
      QDir::cleanPath(profilePath.absoluteFilePath("lockedorder.txt"));

  const QDateTime now = QDateTime::currentDateTime();

  if (createBackup(pluginsName, now, app) && createBackup(loadOrderName, now, app) &&
      createBackup(groupsName, now, app) && createBackup(lockedOrderName, now, app)) {
    GUI::MessageDialog::showMessage(tr("Backup of load order created"), app);
  }
}

QMenu* PluginsWidget::listOptionsMenu()
{
  QMenu* const menu  = new QMenu(this);
  toggleForceEnabled = menu->addAction(tr("Hide force-enabled files"), this,
                                       &PluginsWidget::toggleHideForceEnabled);
  toggleForceEnabled->setCheckable(true);

  toggleIgnoreMasters = menu->addAction(tr("Ignore conflicts with masters"), this,
                                        &PluginsWidget::toggleIgnoreMasterConflicts);
  toggleIgnoreMasters->setCheckable(true);

  menu->addSeparator();

  menu->addAction(tr("Collapse all"), [this]() {
    ui->pluginList->collapseAll();
    ui->pluginList->scrollToTop();
  });
  menu->addAction(tr("Expand all"), [this]() {
    ui->pluginList->expandAll();
    ui->pluginList->scrollToTop();
  });

  menu->addSeparator();

  menu->addAction(tr("Enable all"), [this]() {
    if (confirmMassOperation(tr("Really enable all plugins?"))) {
      m_PluginListModel->setEnabledAll(true);
    }
  });
  menu->addAction(tr("Disable all"), [this]() {
    if (confirmMassOperation(tr("Really disable all plugins?"))) {
      m_PluginListModel->setEnabledAll(false);
    }
  });

  menu->addSeparator();
  resetGroupsAction = menu->addAction(tr("Reset Group Structure"), [this]() {
    if (confirmMassOperation(tr(
            "Reset all groups and separators? Plugin load order will stay unchanged."))) {
      m_PluginListModel->resetGroupsStructure();
    }
  });
  cleanGroupsAction = menu->addAction(tr("Clean Groups"), [this]() {
    m_PluginListModel->cleanEmptyGroups();
  });

  updateGroupActionVisibility();

  return menu;
}

void PluginsWidget::saveState()
{
  auto* const settings = Settings::instance();
  saveScrollPosition();
  settings->saveState(ui->pluginList->header());
  if (settings->enablePluginGrouping()) {
    settings->saveTreeExpandState(ui->pluginList);
  }
}

void PluginsWidget::restoreState()
{
  const auto* const settings = Settings::instance();
  settings->restoreState(ui->pluginList->header());
  applyGroupingSetting();

  if (settings->enablePluginGrouping()) {
    settings->restoreTreeExpandState(ui->pluginList);
  }

  const bool doHide = settings->get<bool>("hide_force_enabled", false);
  toggleForceEnabled->setChecked(doHide);
  toggleHideForceEnabled();

  const bool doIgnore = settings->get<bool>("ignore_master_conflicts", false);
  toggleIgnoreMasters->setChecked(doIgnore);
  toggleIgnoreMasterConflicts();

  applyConflictManagementSetting();

  QTimer::singleShot(0, this, [this]() { restoreScrollPosition(); });
}

void PluginsWidget::saveScrollPosition() const
{
  if (const auto* const scrollBar = ui->pluginList->verticalScrollBar()) {
    Settings::instance()->set("plugin_list_scroll", scrollBar->value());
  }
}

void PluginsWidget::restoreScrollPosition()
{
  auto* const scrollBar = ui->pluginList->verticalScrollBar();
  if (!scrollBar) {
    m_PendingScrollPosition = -1;
    return;
  }

  const int persisted =
      Settings::instance()->get<int>("plugin_list_scroll", scrollBar->value());
  const int target = m_PendingScrollPosition >= 0 ? m_PendingScrollPosition : persisted;
  scrollBar->setValue(target);
  m_PendingScrollPosition = -1;
}

void PluginsWidget::refreshPluginListPreservingScroll()
{
  if (const auto* const scrollBar = ui->pluginList->verticalScrollBar()) {
    m_PendingScrollPosition = scrollBar->value();
  } else {
    m_PendingScrollPosition = -1;
  }

  saveScrollPosition();
  m_PluginListModel->refresh();
}

static bool containsPlugin(const MOBase::IModInterface* mod)
{
  const auto fileTree = mod ? mod->fileTree() : nullptr;
  if (!fileTree)
    return false;

  return std::ranges::any_of(*fileTree, [&](auto&& entry) {
    if (!entry)
      return false;
    const QString filename = entry->name();
    return filename.endsWith(u".esp"_s, Qt::CaseInsensitive) ||
           filename.endsWith(u".esm"_s, Qt::CaseInsensitive) ||
           filename.endsWith(u".esl"_s, Qt::CaseInsensitive);
  });
}

void PluginsWidget::onModStateChanged(
    const std::map<QString, MOBase::IModList::ModStates>& mods)
{


  const auto modList = m_Organizer->modList();
  if (!modList)
    return;

  for (const auto& [modName, modState] : mods) {
    const auto mod = modList->getMod(modName);
    if (containsPlugin(mod)) {
      m_PluginList->notifyPendingState(modName, modState);
    }
  }
  refreshPluginListPreservingScroll();
}

bool PluginsWidget::onAboutToRun([[maybe_unused]] const QString& binary)
{
  m_PluginList->writePluginLists();

  const auto profilePath = QDir(m_Organizer->profilePath());
  const auto pluginsName = QDir::cleanPath(profilePath.absoluteFilePath("plugins.txt"));
  const auto loadOrderName =
      QDir::cleanPath(profilePath.absoluteFilePath("loadorder.txt"));
    const auto lockedOrderName =
      QDir::cleanPath(profilePath.absoluteFilePath("lockedorder.txt"));
  const auto parent = this->topLevelWidget();

  if (QFileInfo::exists(pluginsName + ".snapshot")) {
    MOBase::shellDeleteQuiet(pluginsName + ".snapshot", parent);
  }

  if (QFileInfo::exists(loadOrderName + ".snapshot")) {
    MOBase::shellDeleteQuiet(loadOrderName + ".snapshot", parent);
  }

  if (QFileInfo::exists(lockedOrderName + ".snapshot")) {
    MOBase::shellDeleteQuiet(lockedOrderName + ".snapshot", parent);
  }

  if (QFileInfo(binary).fileName().compare("lootcli.exe", Qt::CaseInsensitive) != 0) {
    createBackup(pluginsName, "snapshot", parent);
    createBackup(loadOrderName, "snapshot", parent);
    createBackup(lockedOrderName, "snapshot", parent);
    m_IsRunningApp = true;
  }

  return true;
}

void PluginsWidget::onFinishedRun(const QString& binary,
                                  [[maybe_unused]] unsigned int exitCode)
{
  const auto binaryName = QFileInfo(binary).fileName();
  if (binaryName.compare("lootcli.exe", Qt::CaseInsensitive) == 0) {
    return;
  }

  const bool isLootGui =
      binaryName.compare("Loot.exe", Qt::CaseInsensitive) == 0;

  if (isLootGui) {
    const auto profilePath = QDir(m_Organizer->profilePath());
    const auto pluginsName =
        QDir::cleanPath(profilePath.absoluteFilePath("plugins.txt"));
    const auto loadOrderName =
        QDir::cleanPath(profilePath.absoluteFilePath("loadorder.txt"));
    const auto lockedOrderName =
        QDir::cleanPath(profilePath.absoluteFilePath("lockedorder.txt"));
    const auto parent = this->topLevelWidget();

    MOBase::shellDeleteQuiet(pluginsName + ".snapshot", parent);
    MOBase::shellDeleteQuiet(loadOrderName + ".snapshot", parent);
    MOBase::shellDeleteQuiet(lockedOrderName + ".snapshot", parent);

    m_DeferPostLootRefresh = true;
    m_IsRunningApp         = false;
    m_ExternalStatesChanged = false;
    return;
  }



  m_Organizer->onNextRefresh([=, this]() {
    m_PluginListModel->refresh();
    checkLoadOrderChanged(binaryName);

    m_IsRunningApp          = false;
    m_ExternalStatesChanged = false;
  });
}

void PluginsWidget::onSettingChanged(const QString& key,
                                     [[maybe_unused]] const QVariant& oldValue,
                                     const QVariant& newValue)
{
  if (key == u"enable_sort_button"_s) {
    // Only control our Patch Sort button — never touch MO2's Sort button.
    if (auto* b = findChild<QPushButton*>(u"patchSortBtn"_s)) {
      b->setVisible(newValue.value<bool>());
    }
  } else if (key == u"enable_plugin_grouping"_s) {
    applyGroupingSetting();
    updateGroupActionVisibility();
  } else if (key == u"enable_reset_groups_button"_s ||
             key == u"enable_clean_groups_button"_s) {
    updateGroupActionVisibility();
  } else if (key == u"enable_plugin_conflict_management"_s) {
    applyConflictManagementSetting();
  }
}

void PluginsWidget::updateGroupActionVisibility()
{
  const auto* const settings = Settings::instance();
  const bool groupingEnabled = settings->enablePluginGrouping();
  const bool showReset = groupingEnabled && settings->enableResetGroupsButton();
  const bool showClean = groupingEnabled && settings->enableCleanGroupsButton();

  ui->resetGroupsButton->setVisible(showReset);
  ui->cleanGroupsButton->setVisible(showClean);

  if (resetGroupsAction) {
    resetGroupsAction->setVisible(showReset);
  }

  if (cleanGroupsAction) {
    cleanGroupsAction->setVisible(showClean);
  }
}

void PluginsWidget::applyConflictManagementSetting()
{
  const bool enabled = Settings::instance()->enablePluginConflictManagement();

  ui->pluginList->setColumnHidden(PluginListModel::COL_CONFLICTS, !enabled);

  if (toggleIgnoreMasters) {
    toggleIgnoreMasters->setEnabled(enabled);
    if (!enabled) {
      toggleIgnoreMasters->setChecked(false);
    }
  }

  m_PluginListModel->invalidateConflicts();
  ui->pluginList->updateOverwriteMarkers();
}

bool PluginsWidget::confirmMassOperation(const QString& text) const
{
  if (!Settings::instance()->confirmMassOperations()) {
    return true;
  }

  return QMessageBox::question(topLevelWidget(), tr("Confirm"), text,
                               QMessageBox::Yes | QMessageBox::No) ==
         QMessageBox::Yes;
}

bool PluginsWidget::selectedSingleGroup(QString* groupName) const
{
  if (!Settings::instance()->enablePluginGrouping()) {
    return false;
  }

  const auto selected = ui->pluginList->selectionModel()->selectedRows();
  if (selected.size() != 1) {
    return false;
  }

  const auto idx = selected.first();
  if (!idx.isValid() || !idx.model()->hasChildren(idx)) {
    return false;
  }

  if (groupName) {
    *groupName = idx.data().toString();
  }

  return true;
}

void PluginsWidget::renameSelectedGroup()
{
  QString oldGroup;
  if (!selectedSingleGroup(&oldGroup)) {
    return;
  }

  bool ok = false;
  const QString group = QInputDialog::getText(topLevelWidget(), tr("Rename Group..."),
                                              tr("Please enter a name:"),
                                              QLineEdit::Normal, oldGroup, &ok);

  if (!ok || group.isEmpty() || group == oldGroup) {
    return;
  }

  m_PluginListModel->renameGroup(oldGroup, group);
}

void PluginsWidget::removeSelectedGroup()
{
  QString group;
  if (!selectedSingleGroup(&group)) {
    return;
  }

  if (!confirmMassOperation(
          tr("Are you sure you want to remove \"%1\"?").arg(group))) {
    return;
  }

  m_PluginListModel->removeGroup(group);
}

void PluginsWidget::mergeSelectedGroup()
{
  QString fromGroup;
  if (!selectedSingleGroup(&fromGroup)) {
    return;
  }

  GUI::ListDialog dialog{*Settings::instance(), topLevelWidget()};
  dialog.setWindowTitle(tr("Merge Group Into..."));

  QStringList choices = m_PluginListModel->groups();
  choices.removeAll(fromGroup);
  dialog.setChoices(choices);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const QString toGroup = dialog.getChoice();
  if (toGroup.isEmpty()) {
    return;
  }

  m_PluginListModel->mergeGroup(fromGroup, toGroup);
}

void PluginsWidget::applyGroupingSetting()
{
  auto* const model = Settings::instance()->enablePluginGrouping()
                          ? static_cast<QAbstractItemModel*>(m_GroupProxy)
                          : static_cast<QAbstractItemModel*>(m_SortProxy);

  if (ui->pluginList->model() != model) {
    ui->pluginList->setModel(model);
    ui->pluginList->sortByColumn(PluginListModel::COL_PRIORITY, Qt::AscendingOrder);
  }

  if (m_ViewSelectionChangedConnection) {
    disconnect(m_ViewSelectionChangedConnection);
  }
  m_ViewSelectionChangedConnection =
      connect(ui->pluginList->selectionModel(), &QItemSelectionModel::selectionChanged,
              this, &PluginsWidget::onSelectionChanged);

  if (!Settings::instance()->enablePluginGrouping()) {
    ui->pluginList->collapseAll();
  }
}

static QByteArray hashFile(const QString& filePath)
{
  QCryptographicHash hash{QCryptographicHash::Sha1};
  QFile file{filePath};
  if (file.open(QIODevice::ReadOnly)) {
    hash.addData(file.readAll());
  } else {
    return ""_ba;
  }

  return hash.result();
}

void PluginsWidget::checkLoadOrderChanged(const QString& binaryName)
{
  const auto profilePath = QDir(m_Organizer->profilePath());
  const auto pluginsName = QDir::cleanPath(profilePath.absoluteFilePath("plugins.txt"));
  const auto loadOrderName =
      QDir::cleanPath(profilePath.absoluteFilePath("loadorder.txt"));
    const auto lockedOrderName =
      QDir::cleanPath(profilePath.absoluteFilePath("lockedorder.txt"));
  const auto parent = this->topLevelWidget();

  const auto pluginsSnapshot   = pluginsName + ".snapshot";
  const auto loadOrderSnapshot = loadOrderName + ".snapshot";
    const auto lockedOrderSnapshot = lockedOrderName + ".snapshot";

  const auto pluginsFile   = QFileInfo(pluginsName);
  const auto loadOrderFile = QFileInfo(loadOrderName);

  if (!QFileInfo(loadOrderSnapshot).exists())
    return;


  const bool enableWarning    = Settings::instance()->externalChangeWarning();
  const bool loadOrderChanged = m_ExternalStatesChanged ||
                                hashFile(loadOrderName) != hashFile(loadOrderSnapshot);

  if (loadOrderChanged) {
    if (binaryName.compare("Loot.exe", Qt::CaseInsensitive) != 0) {


      bool shouldRestore = true;
      if (enableWarning) {
        const auto answer = QMessageBox::question(
            this, tr("Load Order Changed"),
            tr("%1 has modified the load order. Do you want to apply these changes?")
                .arg(binaryName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        shouldRestore = (answer == QMessageBox::No);
      }

      if (shouldRestore) {
        if (!tryRestore(pluginsName, "snapshot", true, parent) ||
            !tryRestore(loadOrderName, "snapshot", true, parent) ||
            !tryRestore(lockedOrderName, "snapshot", false, parent)) {
          const auto e = ::GetLastError();

          QMessageBox::critical(
              this, tr("Restore failed"),
              tr("Failed to restore the backup. Errorcode: %1")
                  .arg(QString::fromStdWString(MOBase::formatSystemMessage(e))));

          return;
        }
      }
    }
  }

  MOBase::shellDeleteQuiet(pluginsSnapshot, parent);
  MOBase::shellDeleteQuiet(loadOrderSnapshot, parent);
  MOBase::shellDeleteQuiet(lockedOrderSnapshot, parent);
  m_PluginListModel->invalidate();
}

void PluginsWidget::importLootGroups()
{
  const auto profilePath = QDir(m_Organizer->profilePath());
  const auto plugingroups =
      QDir::cleanPath(profilePath.absoluteFilePath(u"plugingroups.txt"_s));

  const auto* const managedGame = m_Organizer->managedGame();
  const auto gameName           = managedGame->gameName();
  const auto localAppData =
      QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
  const auto masterlist =
      localAppData.filePath(u"LOOT/games/%1/masterlist.yaml"_s.arg(gameName));
  const auto userlist =
      localAppData.filePath(u"LOOT/games/%1/userlist.yaml"_s.arg(gameName));

  MOTools::importLootGroups(m_PluginList, plugingroups, masterlist, userlist);
}

void PluginsWidget::synchronizePluginLists(MOBase::IOrganizer* organizer)
{
  MOBase::IPluginList* const ipluginlist = organizer->pluginList();
  if (ipluginlist == nullptr || ipluginlist == m_PluginList) {
    return;
  }

  std::function<void()> startRefresh = [this] {
    m_OrganizerRefreshing = true;
    m_PluginList->flushPendingStates();



    if (const auto* const sb = ui->pluginList->verticalScrollBar())
      m_PendingScrollPosition = sb->value();




    m_PluginListModel->invalidate();
  };

  organizer->onNextRefresh(startRefresh, false);

  ipluginlist->onRefreshed([this, organizer, startRefresh]() {
    if (m_OrganizerRefreshing) {
      m_OrganizerRefreshing = false;
      organizer->onNextRefresh(startRefresh, false);
    }
  });

  ipluginlist->onPluginMoved(
      [this](const QString& name, int oldPriority, int newPriority) {
        if (m_OrganizerRefreshing)
          return;
        m_PluginListModel->movePlugin(name, oldPriority, newPriority);
      });

  ipluginlist->onPluginStateChanged(
      [this](const std::map<QString, MOBase::IPluginList::PluginStates>& infos) {
        if (m_OrganizerRefreshing)
          return;
        m_PluginListModel->changePluginStates(infos);
      });

  m_PluginList->onPluginMoved([=, this](const QString& name,
                                        [[maybe_unused]] int oldPriority,
                                        int newPriority) {
    if (m_PluginList->isRefreshing())
      return;

    ipluginlist->setPriority(name, newPriority);
  });

  m_PluginList->onPluginStateChanged(
      [=, this](const std::map<QString, MOBase::IPluginList::PluginStates>& infos) {
        if (m_IsRunningApp && !m_PluginList->isRefreshing() && !infos.empty()) {
          m_ExternalStatesChanged = true;
        }

        if (m_PluginList->isRefreshing() || infos.empty())
          return;

        for (const auto& [name, state] : infos) {
          m_PanelInterface->setPluginState(name,
                                           state == MOBase::IPluginList::STATE_ACTIVE);
        }
      });
}

// ---------------------------------------------------------------------------
// Info tab: rich text overview for the currently selected plugin.
// Synced from the plugin list's selection model.
// ---------------------------------------------------------------------------
void PluginsWidget::refreshInfoTab(const TESData::FileInfo* plugin)
{
  if (!plugin || !m_InfoBrowser) return;

  const int id = m_PluginList->getIndex(plugin->name());

  QString html = u"<h3>%1</h3>"_s.arg(plugin->name());

  // Origin mod
  const QString origin = m_PluginList->getOriginName(id);
  if (!origin.isEmpty()) {
    html += u"<b>%1</b>: %2<br>"_s.arg(tr("Mod"), origin);
  }

  // Type flags
  QStringList types;
  if (plugin->isMasterFlagged() || plugin->hasMasterExtension()) types << u"ESM"_s;
  if (plugin->isLightFlagged()  || plugin->hasLightExtension())  types << u"ESL"_s;
  if (plugin->isMediumFlagged())   types << u"ESH"_s;
  if (plugin->isOverlayFlagged())  types << u"Overlay"_s;
  if (plugin->isBlueprintFlagged()) types << u"Blueprint"_s;
  if (!types.isEmpty()) {
    html += u"<b>%1</b>: %2<br>"_s.arg(tr("Type"), types.join(u", "_s));
  }

  // Classification from record analysis
  const TESData::Classification cls =
      TESData::classifyPlugin(*plugin, m_PluginList->blueprintPrefix());
  if (cls.zone != TESData::PluginZone::Unknown) {
    // Confidence colour: green ≥70, amber 40-69, gray <40
    const char* confColor = cls.confidence >= 70 ? "#4caf50"
                          : cls.confidence >= 40 ? "#ff9800"
                                                 : "#9e9e9e";
    const int filled = std::clamp((cls.confidence + 10) / 20, 1, 5);
    const QString dots = u"<span style='color:%3'>%1%2</span>"_s
                             .arg(QString(filled, u'●'), QString(5 - filled, u'○'))
                             .arg(QString::fromLatin1(confColor));
    html += u"<b>%1</b>: %2 %3 <i style='color:gray'>(%4)</i><br>"_s
                .arg(tr("Classified as"), cls.groupName, dots, cls.reason);
  }

  // Current group
  if (!plugin->group().isEmpty()) {
    html += u"<b>%1</b>: %2<br>"_s.arg(tr("Group"), plugin->group());
  }

  // LOOT masterlist status
  {
    const auto* loot = m_PluginList->getLootReport(plugin->name());
    if (loot) {
      html += u"<b>%1</b>: <span style='color:#4caf50'>%2</span><br>"_s
                  .arg(tr("LOOT"), tr("In masterlist — sorting managed automatically"));
    } else {
      html += u"<b>%1</b>: <span style='color:#9e9e9e'>%2 "
              "<a href='add_loot_rule'>%3</a></span><br>"_s
                  .arg(tr("LOOT"), tr("Not in masterlist."), tr("[Add rule…]"));
    }
  }

  html += u"<hr>"_s;

  // Conflicts: what this plugin overrides
  const auto& winning = plugin->getPluginOverriding();
  if (!winning.isEmpty()) {
    html += u"<b>%1 (%2):</b><ul>"_s.arg(tr("Overrides"), QString::number(winning.size()));
    int shown = 0;
    for (const int idx : winning) {
      if (shown++ >= 10) { html += u"<li><i>…and %1 more</i></li>"_s.arg(winning.size() - 10); break; }
      if (const auto* other = m_PluginList->getPlugin(idx)) {
        html += u"<li>%1</li>"_s.arg(other->name());
      }
    }
    html += u"</ul>"_s;
  }

  // Conflicts: what overrides this plugin
  const auto& losing = plugin->getPluginOverridden();
  if (!losing.isEmpty()) {
    html += u"<b>%1 (%2):</b><ul>"_s.arg(tr("Overridden by"), QString::number(losing.size()));
    int shown = 0;
    for (const int idx : losing) {
      if (shown++ >= 10) { html += u"<li><i>…and %1 more</i></li>"_s.arg(losing.size() - 10); break; }
      if (const auto* other = m_PluginList->getPlugin(idx)) {
        html += u"<li>%1</li>"_s.arg(other->name());
      }
    }
    html += u"</ul>"_s;
  }

  // Inferred patch suggestion
  const auto& inferred = plugin->getInferredOverrides();
  auto maxIt = std::max_element(inferred.constBegin(), inferred.constEnd());
  const int infoThreshold = std::max(1, MOPlugin::pluginINI().patchThreshold() / 6);
  if (maxIt != inferred.constEnd() && maxIt.value() >= infoThreshold) {
    if (const auto* target = m_PluginList->getPlugin(maxIt.key())) {
      html += u"<b>%1</b>: %2 <i>(%3 shared records)</i><br>"_s
                  .arg(tr("Likely patches"), target->name(),
                       QString::number(maxIt.value()));
    }
  }

  // Master chain (dependency graph, up to 2 levels)
  {
    const auto& masters = plugin->masters();
    if (!masters.isEmpty()) {
      html += u"<b>%1:</b><ul>"_s.arg(tr("Masters"));
      for (const QString& m : masters) {
        const auto* mp = m_PluginList->getPluginByName(m);
        if (mp && !mp->masters().isEmpty()) {
          // Show one level of grandmasters
          QStringList gm;
          for (const QString& g : mp->masters()) gm << g;
          html += u"<li>%1 <span style='color:gray'>← %2</span></li>"_s
                      .arg(m, gm.join(u", "_s));
        } else {
          html += u"<li>%1%2</li>"_s.arg(m,
              mp ? QString() : u" <span style='color:#e57373'>(%1)</span>"_s.arg(tr("missing")));
        }
      }
      html += u"</ul>"_s;
    }
  }

  // Record type breakdown (top 5 by count)
  {
    const auto& hist = plugin->recordTypeHistogram();
    if (!hist.isEmpty()) {
      // Sort by count descending
      QList<QPair<quint32, int>> sorted;
      for (auto it = hist.constBegin(); it != hist.constEnd(); ++it)
        sorted.append({it.key(), it.value()});
      std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b){ return a.second > b.second; });
      int total = 0;
      for (const auto& p : sorted) total += p.second;
      html += u"<b>%1</b> (%2 total)<br>"_s.arg(tr("Record types"), QString::number(total));
      html += u"<table cellspacing='2'>"_s;
      const int shown = std::min(static_cast<int>(sorted.size()), 5);
      for (int i = 0; i < shown; ++i) {
        const quint32 key = sorted[i].first;
        const int cnt = sorted[i].second;
        // Decode 4-byte little-endian type tag back to ASCII string
        char buf[5] = {};
        buf[0] = static_cast<char>(key & 0xFF);
        buf[1] = static_cast<char>((key >> 8)  & 0xFF);
        buf[2] = static_cast<char>((key >> 16) & 0xFF);
        buf[3] = static_cast<char>((key >> 24) & 0xFF);
        const QString code = QString::fromLatin1(buf, 4);
        const int pct = total > 0 ? cnt * 100 / total : 0;
        html += u"<tr><td>%1</td><td align='right'>%2</td>"
                u"<td>&nbsp;<span style='color:gray'>%3%</span></td></tr>"_s
                    .arg(code, QString::number(cnt), QString::number(pct));
      }
      if (sorted.size() > 5) {
        html += u"<tr><td colspan='3'><i>…and %1 more types</i></td></tr>"_s
                    .arg(sorted.size() - 5);
      }
      html += u"</table>"_s;
    }
  }

  // Blueprint pair
  if (plugin->isBlueprintPrefixed()) {
    const QString prefix = m_PluginList->blueprintPrefix();
    const QString base   = QFileInfo(plugin->name()).completeBaseName().mid(prefix.length());
    for (const auto* ext : {".esm", ".esp", ".esl"}) {
      if (const auto* paired = m_PluginList->getPluginByName(base + QString::fromLatin1(ext))) {
        html += u"<b>%1</b>: %2<br>"_s.arg(tr("Main plugin"), paired->name());
        break;
      }
    }
  }

  // Mod-author .bs hint
  if (plugin->hasBsHint()) {
    html += u"<hr><b>%1</b> &nbsp; "_s.arg(tr("Mod author suggestion (.bs):"));
    html += u"<a href='edit_bs'>%1</a><br>"_s.arg(tr("[Edit]"));
    html += u"<code>group=%1</code><br>"_s.arg(plugin->bsGroupHint());
    if (!plugin->bsZoneHint().isEmpty())
      html += u"<code>zone=%1</code><br>"_s.arg(plugin->bsZoneHint());
    if (plugin->bsConfidence() >= 1 && plugin->bsConfidence() <= 100)
      html += u"<code>confidence=%1</code><br>"_s.arg(plugin->bsConfidence());
    html += u"<small><i>%1</i></small>"_s.arg(
        tr("Assign in Group Manager to override this suggestion."));
  }

  m_InfoBrowser->setHtml(html);
}

// ---------------------------------------------------------------------------
// Settings tab: inline form backed by BSPluginsINI — no dialog needed.
// ---------------------------------------------------------------------------
QWidget* PluginsWidget::buildSettingsTab(QWidget* parent)
{
  auto& ini = MOPlugin::pluginINI();

  auto* page = new QWidget(parent);
  auto* vbox = new QVBoxLayout(page);
  vbox->setContentsMargins(6, 6, 6, 6);
  vbox->setSpacing(8);

  // ---- Group Names ----
  auto* namesGroup = new QGroupBox(tr("Group Names"), page);
  auto* namesForm  = new QFormLayout(namesGroup);
  namesForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  auto addRow = [&](const QString& label, const QString& val,
                    std::function<void(const QString&)> setter) {
    auto* edit = new QLineEdit(val, namesGroup);
    connect(edit, &QLineEdit::textChanged, page, [setter](const QString& t) {
      if (!t.trimmed().isEmpty()) setter(t.trimmed());
    });
    namesForm->addRow(label, edit);
  };

  addRow(tr("Patches:"),         ini.groupNamePatches(),    [&ini](const QString& v){ ini.setGroupNamePatches(v); });
  addRow(tr("Visuals:"),         ini.groupNameVisuals(),    [&ini](const QString& v){ ini.setGroupNameVisuals(v); });
  addRow(tr("World Changes:"),   ini.groupNameWorld(),      [&ini](const QString& v){ ini.setGroupNameWorld(v); });
  addRow(tr("Gameplay:"),        ini.groupNameGameplay(),   [&ini](const QString& v){ ini.setGroupNameGameplay(v); });
  addRow(tr("NPCs & Content:"),  ini.groupNameNPCs(),       [&ini](const QString& v){ ini.setGroupNameNPCs(v); });
  addRow(tr("Frameworks:"),      ini.groupNameFrameworks(), [&ini](const QString& v){ ini.setGroupNameFrameworks(v); });
  addRow(tr("Archive Loaders:"), ini.groupNameArchive(),   [&ini](const QString& v){ ini.setGroupNameArchive(v); });

  // Collect edit pointers so the reset button can repopulate them.
  // addRow() above appended each QLineEdit as the field item in the form.
  QList<QLineEdit*> groupEdits;
  for (int r = 0; r < namesForm->rowCount(); ++r) {
    if (auto* item = namesForm->itemAt(r, QFormLayout::FieldRole)) {
      if (auto* e = qobject_cast<QLineEdit*>(item->widget())) {
        groupEdits.append(e);
      }
    }
  }

  auto* resetBtn = new QPushButton(tr("Reset to defaults"), namesGroup);
  connect(resetBtn, &QPushButton::clicked, page,
          [&ini, groupEdits]() {
            ini.resetGroupNamesToDefaults();
            // Repopulate fields in the same order addRow() added them
            const QStringList defaults{
                ini.groupNamePatches(), ini.groupNameVisuals(),
                ini.groupNameWorld(),   ini.groupNameGameplay(),
                ini.groupNameNPCs(),    ini.groupNameFrameworks(),
                ini.groupNameArchive()};
            for (int i = 0; i < groupEdits.size() && i < defaults.size(); ++i) {
              QSignalBlocker blocker(groupEdits.at(i));
              groupEdits.at(i)->setText(defaults.at(i));
            }
          });
  namesForm->addRow(QString(), resetBtn);
  vbox->addWidget(namesGroup);

  // ---- Custom Groups ----
  auto* cgGroup = new QGroupBox(tr("Custom Groups"), page);
  auto* cgLay   = new QVBoxLayout(cgGroup);
  auto* cgInfo  = new QLabel(
      tr("Define your own groups with optional record-type detection rules.\n"
         "Custom groups are checked before built-in classification."), cgGroup);
  cgInfo->setWordWrap(true);
  cgInfo->setStyleSheet(u"color: gray; font-size: small;"_s);
  cgLay->addWidget(cgInfo);

  // Inline list showing existing custom groups (read-only summary)
  auto* cgListWidget = new QListWidget(cgGroup);
  cgListWidget->setMaximumHeight(90);
  cgListWidget->setSelectionMode(QAbstractItemView::NoSelection);
  cgListWidget->setStyleSheet(u"font-size: small;"_s);
  const QPointer<QListWidget> cgListPtr(cgListWidget);
  const auto refreshCgList = [cgListPtr]() {
    if (!cgListPtr) return;  // widget was destroyed
    cgListPtr->clear();
    for (const auto& cg : MOPlugin::pluginINI().customGroups()) {
      const QString label = cg.recordTypes.isEmpty()
          ? u"★ %1  →  %2"_s.arg(cg.name, cg.zone)
          : u"★ %1  →  %2  [%3 ≥%4%]"_s.arg(
                cg.name, cg.zone, cg.recordTypes.join(u','),
                QString::number(cg.threshold));
      cgListPtr->addItem(label);
    }
    if (cgListPtr->count() == 0) {
      auto* empty = new QListWidgetItem(
          QObject::tr("No custom groups defined yet."), cgListPtr.data());
      empty->setForeground(Qt::gray);
    }
  };
  refreshCgList();
  cgLay->addWidget(cgListWidget);

  auto* manageBtn = new QPushButton(tr("Open Group Manager…"), cgGroup);
  manageBtn->setToolTip(
      tr("Manage all plugin groups, create custom groups, and assign plugins "
         "by dragging or selecting."));
  connect(manageBtn, &QPushButton::clicked, page, [this, refreshCgList, page]() {
    auto* dlg = new GroupManagerDialog(m_PluginList, m_PluginListModel, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &QDialog::finished, page, [refreshCgList]{ refreshCgList(); });
    dlg->show();
  });
  cgLay->addWidget(manageBtn);
  vbox->addWidget(cgGroup);

  // ---- Updates ----
  auto* updGroup = new QGroupBox(tr("Updates"), page);
  auto* updForm  = new QFormLayout(updGroup);
  auto* nexusEdit = new QLineEdit(
      ini.nexusUrl().isEmpty() ? QStringLiteral("https://www.nexusmods.com/")
                               : ini.nexusUrl(),
      updGroup);
  nexusEdit->setPlaceholderText(tr("Nexus Mods page URL for this plugin"));
  nexusEdit->setToolTip(
      tr("When an update is found, open this URL instead of GitHub releases."));
  connect(nexusEdit, &QLineEdit::textChanged, updGroup, [&ini](const QString& t) {
    ini.setNexusUrl(t.trimmed());
  });
  updForm->addRow(tr("Nexus URL:"), nexusEdit);

  auto* checkNowBtn = new QPushButton(tr("Check for update now"), updGroup);
  connect(checkNowBtn, &QPushButton::clicked, page, [this]() {
    auto* checker = new UpdateChecker(u"2.9.b"_s, this);
    connect(checker, &UpdateChecker::updateAvailable, this,
            [this](const QString& latest, const QString& url) {
              bsWarn(tr("Update available: v%1").arg(latest));
              UpdateDialog dlg(latest, url, topLevelWidget());
              dlg.exec();
            });
    checker->check();
  });
  updForm->addRow(QString(), checkNowBtn);
  vbox->addWidget(updGroup);

  // ---- About ----
  auto* aboutLabel = new QLabel(
      tr("<small>BSPlugins Extended v2.9 Beta (BETA) by MK-HATERS<br>"
         "Based on work by Parapets and Alaxouche<br>"
         "<a href='https://github.com/MK-HATERS/bsplugins-extended'>GitHub</a>"
         "</small>"),
      page);
  aboutLabel->setOpenExternalLinks(true);
  aboutLabel->setAlignment(Qt::AlignCenter);
  vbox->addWidget(aboutLabel);

  // ---- .bs Generator ----
  auto* bsGroup = new QGroupBox(tr("Mod Author Tools"), page);
  auto* bsLay   = new QVBoxLayout(bsGroup);
  auto* bsInfo  = new QLabel(
      tr("Create a <code>.bs</code> hint file that tells BSPlugins which group "
         "a plugin belongs to. Ship it alongside the plugin in your mod archive."),
      bsGroup);
  bsInfo->setWordWrap(true);
  bsInfo->setStyleSheet(u"color: gray; font-size: small;"_s);
  bsLay->addWidget(bsInfo);

  auto* bsGenBtn = new QPushButton(tr("Generate .bs file for selected plugin…"), bsGroup);
  bsGenBtn->setToolTip(
      tr("Creates PluginName.esp.bs in the mod's folder. Opens in your text "
         "editor so you can set the group name and zone."));
  connect(bsGenBtn, &QPushButton::clicked, page, [this]() {
    const auto sel = ui->pluginList->selectionModel()->selectedIndexes();
    if (sel.isEmpty()) {
      QMessageBox::information(this, tr("Generate .bs"),
                               tr("Select a plugin in the list first."));
      return;
    }
    const auto* plugin = m_PluginList->getPlugin(
        sel.first().data(PluginListModel::IndexRole).toInt());
    if (!plugin) return;

    // Find the mod origin folder
    const QString origin = m_PluginList->getOriginName(
        m_PluginList->getIndex(plugin->name()));
    const auto* mod = m_Organizer->modList()->getMod(origin);
    const QString modPath = mod ? mod->absolutePath() : QString();

    QString bsPath;
    if (!modPath.isEmpty()) {
      bsPath = modPath + QStringLiteral("/") + plugin->name() + QStringLiteral(".bs");
    } else {
      bsPath = QDir::homePath() + QStringLiteral("/") + plugin->name() + QStringLiteral(".bs");
    }

    if (!QFile::exists(bsPath)) {
      QFile f(bsPath);
      if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "group=Your Group Name\n";
        ts << "zone=Visuals\n";
        ts << "# confidence=95   (optional: 1-100, default 95. Use 100 to lock placement.)\n";
        ts << "# Zones: Visuals, World Changes, Gameplay, NPCs & Content, Frameworks, Patches\n";
      }
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(bsPath));
  });
  bsLay->addWidget(bsGenBtn);

  auto* bsBatchBtn = new QPushButton(
      tr("Generate .bs for all plugins in selected mod's origin…"), bsGroup);
  bsBatchBtn->setToolTip(
      tr("Generates a .bs hint file for every plugin that comes from the same mod as "
         "the currently selected plugin. Each file is pre-filled with the current "
         "classification so you can adjust and ship them with the mod."));
  connect(bsBatchBtn, &QPushButton::clicked, page, [this]() {
    const auto sel = ui->pluginList->selectionModel()->selectedIndexes();
    if (sel.isEmpty()) {
      QMessageBox::information(this, tr("Batch .bs Generator"),
                               tr("Select a plugin in the list first."));
      return;
    }
    const auto* plugin = m_PluginList->getPlugin(
        sel.first().data(PluginListModel::IndexRole).toInt());
    if (!plugin) return;
    const int id          = m_PluginList->getIndex(plugin->name());
    const QString origin  = m_PluginList->getOriginName(id);
    const auto* mod       = m_Organizer->modList()->getMod(origin);
    if (!mod) {
      QMessageBox::information(this, tr("Batch .bs Generator"),
                               tr("Cannot determine mod origin for this plugin."));
      return;
    }
    const QString modPath = mod->absolutePath();
    const QString prefix  = m_PluginList->blueprintPrefix();
    int generated = 0;
    const int count = m_PluginList->pluginCount();
    for (int i = 0; i < count; ++i) {
      const auto* p = m_PluginList->getPlugin(i);
      if (!p) continue;
      if (m_PluginList->getOriginName(i) != origin) continue;
      const TESData::Classification cls = TESData::classifyPlugin(*p, prefix);
      const QString bsPath = modPath + u"/"_s + p->name() + u".bs"_s;
      if (QFile::exists(bsPath)) continue;  // don't overwrite existing hints
      QFile f(bsPath);
      if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "group=" << (cls.groupName.isEmpty() ? u"Unclassified"_s : cls.groupName) << "\n";
        if (cls.zone != TESData::PluginZone::Unknown)
          ts << "zone=" << TESData::zoneName(cls.zone) << "\n";
        ts << "# confidence=95   (optional: 1-100, 0 = let BSPlugins auto-classify)\n";
        ts << "# Edit group= and zone= to reflect where this plugin belongs.\n";
        ++generated;
      }
    }
    QMessageBox::information(this, tr("Batch .bs Generator"),
                             tr("Generated %1 new .bs file(s) for mod \"%2\".\n\n"
                                "Files already existing were skipped. "
                                "Open the mod folder to edit them.")
                                 .arg(generated).arg(origin));
    if (generated > 0)
      MOBase::shell::Explore(modPath);
  });
  bsLay->addWidget(bsBatchBtn);
  vbox->addWidget(bsGroup);

  // ---- Named Snapshots ----
  auto* snapGroup = new QGroupBox(tr("Load Order Snapshots"), page);
  auto* snapLay   = new QVBoxLayout(snapGroup);
  auto* snapInfo  = new QLabel(
      tr("Save named snapshots of your load order — useful as stable restore points "
         "before experimenting with new mods. Stored in <code>plugins/bsplugins/snapshots/</code>."),
      snapGroup);
  snapInfo->setWordWrap(true);
  snapInfo->setStyleSheet(u"color: gray; font-size: small;"_s);
  snapLay->addWidget(snapInfo);

  auto* snapBtnRow = new QHBoxLayout;
  auto* saveSnapBtn = new QPushButton(tr("Save snapshot…"), snapGroup);
  saveSnapBtn->setToolTip(
      tr("Saves the current plugins.txt, loadorder.txt, plugingroups.txt and "
         "lockedorder.txt under a name you choose. Use to bookmark a known-good state."));
  auto* restoreSnapBtn = new QPushButton(tr("Restore snapshot…"), snapGroup);
  restoreSnapBtn->setToolTip(
      tr("Lists all saved snapshots and restores the selected one, replacing the "
         "current load order files. The plugin list reloads automatically."));
  snapBtnRow->addWidget(saveSnapBtn);
  snapBtnRow->addWidget(restoreSnapBtn);
  snapBtnRow->addStretch();
  snapLay->addLayout(snapBtnRow);

  connect(saveSnapBtn, &QPushButton::clicked, page, [this]() {
    bool ok = false;
    const QString name = QInputDialog::getText(
        topLevelWidget(), tr("Save Snapshot"),
        tr("Snapshot name (no spaces or special characters):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    const QString safe = QString(name.trimmed()).replace(QRegularExpression(u"[^A-Za-z0-9_\\-]"_s), u"_"_s);
    const QString snapDir = MOPlugin::BSPluginsINI::pluginDir() +
                            u"/snapshots/"_s + safe;
    if (!QDir().mkpath(snapDir)) {
      QMessageBox::critical(topLevelWidget(), tr("Save Snapshot"),
                            tr("Could not create snapshot directory:\n%1").arg(snapDir));
      return;
    }
    const auto profilePath = QDir(m_Organizer->profilePath());
    const QStringList files{u"plugins.txt"_s, u"loadorder.txt"_s,
                            u"plugingroups.txt"_s, u"lockedorder.txt"_s};
    for (const QString& f : files) {
      const QString src = QDir::cleanPath(profilePath.absoluteFilePath(f));
      const QString dst = snapDir + u"/"_s + f;
      if (QFileInfo::exists(src))
        MOBase::shellCopy(src, dst, true, topLevelWidget());
    }
    bsLog(tr("Snapshot \"%1\" saved.").arg(safe));
  });

  connect(restoreSnapBtn, &QPushButton::clicked, page, [this]() {
    const QString snapRoot = MOPlugin::BSPluginsINI::pluginDir() + u"/snapshots"_s;
    const QStringList snapshots = QDir(snapRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                            QDir::Name);
    if (snapshots.isEmpty()) {
      QMessageBox::information(topLevelWidget(), tr("Restore Snapshot"),
                               tr("No snapshots found. Use 'Save snapshot…' first."));
      return;
    }
    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        topLevelWidget(), tr("Restore Snapshot"),
        tr("Choose a snapshot to restore:"), snapshots, 0, false, &ok);
    if (!ok || chosen.isEmpty()) return;
    const auto reply = QMessageBox::question(
        topLevelWidget(), tr("Restore Snapshot"),
        tr("Restore snapshot \"%1\"? Your current load order will be replaced.").arg(chosen),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    const QString snapDir = snapRoot + u"/"_s + chosen;
    const auto profilePath = QDir(m_Organizer->profilePath());
    const QStringList files{u"plugins.txt"_s, u"loadorder.txt"_s,
                            u"plugingroups.txt"_s, u"lockedorder.txt"_s};
    for (const QString& f : files) {
      const QString src = snapDir + u"/"_s + f;
      const QString dst = QDir::cleanPath(profilePath.absoluteFilePath(f));
      if (QFileInfo::exists(src))
        MOBase::shellCopy(src, dst, true, topLevelWidget());
    }
    m_PluginListModel->invalidate();
    bsLog(tr("Snapshot \"%1\" restored.").arg(chosen));
  });
  vbox->addWidget(snapGroup);

  // ---- Export ----
  auto* exportGroup = new QGroupBox(tr("Export"), page);
  auto* exportLay   = new QVBoxLayout(exportGroup);
  auto* exportInfo  = new QLabel(
      tr("Copy a Markdown table of all plugins — name, group, zone, confidence, "
         "and classification reason — to the clipboard for sharing or logging."),
      exportGroup);
  exportInfo->setWordWrap(true);
  exportInfo->setStyleSheet(u"color: gray; font-size: small;"_s);
  exportLay->addWidget(exportInfo);

  auto* exportBtn = new QPushButton(tr("Copy group summary to clipboard"), exportGroup);
  exportBtn->setToolTip(
      tr("Generates a Markdown table listing every plugin with its group, zone, "
         "confidence and reason. Paste into Nexus posts, load-order help threads, "
         "or a text file for diffing before and after sorting."));
  connect(exportBtn, &QPushButton::clicked, page, [this]() {
    const QString prefix = m_PluginList->blueprintPrefix();
    QString md = u"| Plugin | Group | Zone | Confidence | Reason |\n"_s;
    md         += u"|--------|-------|------|------------|--------|\n"_s;
    const int count = m_PluginList->pluginCount();
    for (int i = 0; i < count; ++i) {
      const auto* p = m_PluginList->getPlugin(i);
      if (!p) continue;
      const TESData::Classification cls = TESData::classifyPlugin(*p, prefix);
      const bool ungrouped = p->group().isEmpty() || p->group() == u"default"_s;
      const QString group = ungrouped ? cls.groupName : p->group();
      const QString zone  = TESData::zoneName(cls.zone);
      // Escape pipes so the table isn't broken by plugin names with | in them
      auto esc = [](const QString& s) { return QString(s).replace(u'|', u'｜'); };
      md += u"| %1 | %2 | %3 | %4% | %5 |\n"_s
                .arg(esc(p->name()), esc(group), esc(zone),
                     QString::number(cls.confidence), esc(cls.reason));
    }
    QGuiApplication::clipboard()->setText(md);
    bsLog(tr("Group summary (%1 plugins) copied to clipboard.").arg(count));
  });
  exportLay->addWidget(exportBtn);
  vbox->addWidget(exportGroup);

  vbox->addStretch();
  return page;
}

// ---------------------------------------------------------------------------
// Backup: MO2 already maintains timestamped backups of plugins.txt and
// modlist.txt in <profilePath>/backups/. We point the user there and
// optionally trigger MO2's built-in backup via the profile interface.
// ---------------------------------------------------------------------------
void PluginsWidget::backupLoadOrder(const QString& /*label*/) const
{
  const QString backupDir = m_Organizer->profilePath() + QStringLiteral("/backups");
  QDir().mkpath(backupDir);  // ensure it exists

  QMessageBox::information(
      topLevelWidget(), tr("Load Order Backup"),
      tr("MO2 automatically keeps timestamped backups of your load order.\n\n"
         "Backup location:\n%1\n\n"
         "To create a manual snapshot now, use the profile panel in MO2 "
         "(top-left profile selector → Manage Profiles → Backup).")
          .arg(QDir::toNativeSeparators(backupDir)));
}

// ---------------------------------------------------------------------------
// Version check — shown once after MO2's UI is fully loaded.
// ---------------------------------------------------------------------------
void PluginsWidget::checkVersionOnStartup()
{
  auto& ini = MOPlugin::pluginINI();

  const QString storedVersion = ini.lastPluginVersion();
  const auto    verInfo       = MOBase::VersionInfo(2, 9, 0, 0);
  const QString ver           = verInfo.displayString(3);

  const bool isFirstInstall = storedVersion.isEmpty();
  const bool isUpdate       = !isFirstInstall && storedVersion != ver;

  if (!isFirstInstall && !isUpdate) {
    return;  // Normal run — nothing to show
  }

  WelcomeDialog::Trigger trigger =
      isFirstInstall ? WelcomeDialog::Trigger::FirstInstall
                     : WelcomeDialog::Trigger::Update;

  WelcomeDialog dlg(trigger, storedVersion, ver, topLevelWidget());

  if (ini.wasGroupNamesMigrated()) {
    // Caller already ran migrate() — add a note (handled inside WelcomeDialog
    // future revision; for now append inline)
  }

  dlg.exec();

  if (dlg.shouldBackup()) {
    const QString label = QDate::currentDate().toString(u"yyyy-MM-dd") +
                          u"-v" + ver;
    backupLoadOrder(label);
  }

  if (dlg.freshRunRecommended()) {
    m_Organizer->setPersistent(BSPlugins::NAME, u"fresh_run_pending"_s, true, false);
  }

  ini.setLastPluginVersion(ver);
  ini.setFirstRunDone(true);
}

}  // namespace BSPluginList
