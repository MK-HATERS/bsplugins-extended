#ifndef BSPLUGINLIST_BSPLUGINSPANEL_H
#define BSPLUGINLIST_BSPLUGINSPANEL_H

#include <QWidget>

namespace MOBase { class IOrganizer; }

namespace BSPluginList
{

class PluginsWidget;

// The BSPlugins panel tab — hosts the Info / Log / Settings QTabWidget
// created by PluginsWidget.  BSPlugins::initPlugin registers a second
// onUserInterfaceInitialized callback that creates this widget and inserts
// it into MO2's main tabWidget after the Plugins tab.
class BSPluginsPanel final : public QWidget
{
  Q_OBJECT

public:
  BSPluginsPanel(PluginsWidget* pluginsWidget, QWidget* parent = nullptr);
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_BSPLUGINSPANEL_H
