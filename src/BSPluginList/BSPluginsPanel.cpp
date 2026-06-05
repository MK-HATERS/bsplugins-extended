#include "BSPluginsPanel.h"
#include "PluginsWidget.h"

#include <QVBoxLayout>

using namespace Qt::Literals::StringLiterals;

namespace BSPluginList
{

BSPluginsPanel::BSPluginsPanel(PluginsWidget* pluginsWidget, QWidget* parent)
    : QWidget(parent)
{
  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);

  // Transplant the Info/Log/Settings tabs from PluginsWidget into this panel.
  // PluginsWidget built and wired all the signals before releasing ownership;
  // those connections (selection model → refreshInfoTab, etc.) remain active.
  auto* infoPanel = pluginsWidget->releaseInfoPanel();
  if (infoPanel) {
    infoPanel->setParent(this);
    lay->addWidget(infoPanel);
  }
}

}  // namespace BSPluginList
