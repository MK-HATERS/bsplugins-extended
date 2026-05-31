#include "PluginsWidget.h"

#include "BSPluginInfo/PluginInfoDialog.h"
#include "GUI/ListDialog.h"
#include "GUI/MessageDialog.h"
#include "GUI/SelectionDialog.h"
#include "MOPlugin/Settings.h"
#include "BSPluginsLog.h"
#include "GroupReviewDialog.h"
#include "MOPlugin/BSPlugins.h"
#include "MOPlugin/BSPluginsINI.h"
#include "TESData/PluginClassifier.h"
#include "UpdateChecker.h"
#include "UpdateDialog.h"
#include "WelcomeDialog.h"
#include "MOTools/Loot.h"
#include "MOTools/LootGroups.h"
#include "TESData/PluginClassifier.h"
#include "PluginListContextMenu.h"
#include "PluginSortFilterProxyModel.h"
#include "ui_pluginswidget.h"

#include <game_features/igamefeatures.h>

#include <boost/range/adaptor/reversed.hpp>

#include <QApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QHBoxLayout>
#include <QIcon>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
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

  // ---- Internal tab widget: Plugins / Info / Log / Settings ----
  // The .ui wraps pluginList+filter in "pluginsContainer" so we can lift
  // the whole thing into the first tab with one call.
  {
    auto* rootLayout      = qobject_cast<QVBoxLayout*>(layout());
    auto* pluginsContainer = findChild<QWidget*>(u"pluginsContainer"_s);

    if (rootLayout && pluginsContainer) {
      rootLayout->removeWidget(pluginsContainer);

      auto* innerTabs = new QTabWidget(this);
      innerTabs->setDocumentMode(true);   // flush with panel edge, no border

      // ── Tab 0: Plugins ────────────────────────────────────────────────
      innerTabs->addTab(pluginsContainer, tr("Plugins"));

      // ── Tab 1: Info ───────────────────────────────────────────────────
      // Shows conflict/classification details for the selected plugin.
      // Syncs via the pluginList selection model.
      auto* infoPage = new QWidget(innerTabs);
      auto* infoLay  = new QVBoxLayout(infoPage);
      infoLay->setContentsMargins(4, 4, 4, 4);
      infoLay->setSpacing(4);

      auto* infoHint = new QLabel(
          tr("Select a plugin in the Plugins tab to see details here."), infoPage);
      infoHint->setAlignment(Qt::AlignCenter);
      infoHint->setWordWrap(true);
      infoHint->setStyleSheet(u"color: gray;"_s);

      m_InfoBrowser = new QTextBrowser(infoPage);
      m_InfoBrowser->setOpenLinks(false);
      m_InfoBrowser->hide();

      infoLay->addWidget(infoHint);
      infoLay->addWidget(m_InfoBrowser, 1);
      innerTabs->addTab(infoPage, tr("Info"));

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

      // ── Tab 3: Settings ───────────────────────────────────────────────
      // Inline settings backed by BSPluginsINI — no separate dialog needed.
      auto* settingsPage = buildSettingsTab(innerTabs);
      innerTabs->addTab(settingsPage, tr("Settings"));

      rootLayout->addWidget(innerTabs);
    }
  }

  ui->sortButton->setVisible(Settings::instance()->enableSortButton());
  updateGroupActionVisibility();

  // Show welcome / changelog dialog and kick off update check after UI is ready
  organizer->onUserInterfaceInitialized([this](QMainWindow*) {
    checkVersionOnStartup();

    // Async update check — fires updateAvailable() if a newer version exists
    const QString currentVer = u"0.2.0"_s;
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
    bsLog(tr("BSPlugins Extended v0.2.0 ready. Run LOOT sort to classify plugins."));
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

  // When the plugin list refreshes, log warnings for problematic plugins
  connect(m_PluginList, &TESData::PluginList::pluginsListChanged, this, [this]() {
    BSPluginsLog::instance().clear();
    bsLog(tr("Plugin list updated — %1 plugins active.")
              .arg(m_PluginList->pluginCount()));
    const int count = m_PluginList->pluginCount();
    for (int i = 0; i < count; ++i) {
      const auto* p = m_PluginList->getPlugin(i);
      if (!p || !p->enabled()) continue;
      if (p->hasInvalidFormIds()) {
        bsWarn(tr("ESL/ESH with out-of-range ObjectIDs — broken CK export"),
               p->name());
      }
      if (p->isBlueprintFlagged() && !p->isBlueprintPrefixed()) {
        bsWarn(tr("Blueprint-flagged but wrong filename prefix — game can't load it"),
               p->name());
      }
      if (p->isBlueprintPrefixed() && !p->isBlueprintFlagged()) {
        bsWarn(tr("Blueprint-prefixed but missing blueprint flag — unintended autoload"),
               p->name());
      }
      if (p->hasMissingMasters()) {
        bsCrit(tr("Missing masters: %1")
                   .arg(QStringList(p->missingMasters().begin(),
                                    p->missingMasters().end())
                            .join(u", "_s)),
               p->name());
      }
    }
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

  // If this is a re-run, ask: review new additions only or everything?
  bool newOnly = false;
  if (hasPreviousRun) {
    const int newCount = [&]() {
      int n = 0;
      for (int i = 0; i < pluginCount; ++i) {
        if (const auto* p = m_PluginList->getPlugin(i)) {
          if (!reviewed.contains(p->name(), Qt::CaseInsensitive)) ++n;
        }
      }
      return n;
    }();

    if (newCount > 0) {
      const auto choice = QMessageBox::question(
          topLevelWidget(), tr("Review Load Order"),
          tr("You have <b>%1 new plugin(s)</b> since your last review.<br><br>"
             "Would you like to review new additions only, or do a full review?")
              .arg(newCount),
          tr("New additions only"), tr("Full review"), tr("Skip"), 0, 2);
      if (choice == 2) return;  // Skip
      newOnly = (choice == 0);
    } else {
      return;  // Nothing new — skip dialog entirely
    }
  }

  // --- Collect patch suggestions (inferred overrides >= 3 records) ---
  QList<GroupReviewDialog::PatchSuggestion> patches;
  for (int i = 0; i < pluginCount; ++i) {
    const auto* plugin = m_PluginList->getPlugin(i);
    if (!plugin || !plugin->enabled()) continue;
    const auto& inferred = plugin->getInferredOverrides();
    auto maxIt = std::max_element(inferred.constBegin(), inferred.constEnd());
    if (maxIt == inferred.constEnd() || maxIt.value() < 3) continue;
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
    ps.preChecked   = (ps.recordCount >= 20);
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
    if (newOnly && reviewed.contains(plugin->name(), Qt::CaseInsensitive)) continue;

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
  QStringList nowReviewed = reviewed;
  for (int i = 0; i < pluginCount; ++i) {
    if (const auto* p = m_PluginList->getPlugin(i)) {
      if (!nowReviewed.contains(p->name(), Qt::CaseInsensitive)) {
        nowReviewed.append(p->name());
      }
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
    ui->sortButton->setVisible(newValue.value<bool>());
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
    html += u"<b>%1</b>: %2 <i>(%3)</i><br>"_s
                .arg(tr("Classified as"), cls.groupName, cls.reason);
  }

  // Current group
  if (!plugin->group().isEmpty()) {
    html += u"<b>%1</b>: %2<br>"_s.arg(tr("Group"), plugin->group());
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
  if (maxIt != inferred.constEnd() && maxIt.value() >= 3) {
    if (const auto* target = m_PluginList->getPlugin(maxIt.key())) {
      html += u"<b>%1</b>: %2 <i>(%3 shared records)</i><br>"_s
                  .arg(tr("Likely patches"), target->name(),
                       QString::number(maxIt.value()));
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

  // ---- Updates ----
  auto* updGroup = new QGroupBox(tr("Updates"), page);
  auto* updForm  = new QFormLayout(updGroup);
  auto* nexusEdit = new QLineEdit(ini.skipVersion().isEmpty()
                                      ? QStringLiteral("https://www.nexusmods.com/")
                                      : ini.skipVersion(),
                                  updGroup);
  nexusEdit->setPlaceholderText(tr("Nexus Mods page URL"));
  updForm->addRow(tr("Nexus URL:"), nexusEdit);

  auto* checkNowBtn = new QPushButton(tr("Check for update now"), updGroup);
  connect(checkNowBtn, &QPushButton::clicked, page, [this]() {
    auto* checker = new UpdateChecker(u"0.2.0"_s, this);
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
      tr("<small>BSPlugins Extended v0.2.0 by MK-HATERS<br>"
         "Based on work by Parapets and Alaxouche<br>"
         "<a href='https://github.com/MK-HATERS/bsplugins-extended'>GitHub</a>"
         "</small>"),
      page);
  aboutLabel->setOpenExternalLinks(true);
  aboutLabel->setAlignment(Qt::AlignCenter);
  vbox->addWidget(aboutLabel);

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
  const auto    verInfo       = MOBase::VersionInfo(0, 2, 0, 0);
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
