#ifndef BSPLUGINLIST_FLAGICONDELEGATE_H
#define BSPLUGINLIST_FLAGICONDELEGATE_H

#include "GUI/IconDelegate.h"
#include "PluginListView.h"
#include "TESData/FileInfo.h"

namespace BSPluginList
{

// COL_FLAGS — MO2's standard flag icons (unchanged from original bsplugins).
class FlagIconDelegate final : public GUI::IconDelegate
{
  Q_OBJECT

public:
  explicit FlagIconDelegate(PluginListView* view);

protected:
  [[nodiscard]] QList<QString> getIcons(const QModelIndex& index) const override;
  [[nodiscard]] int getNumIcons(const QModelIndex& index) const override;

private:
  const PluginListView* m_View;
};

// COL_BSINFO — our own Starfield-specific flag icons (ESH, Blueprint, Patch).
class BSFlagIconDelegate final : public GUI::IconDelegate
{
  Q_OBJECT

public:
  explicit BSFlagIconDelegate(PluginListView* view);

protected:
  [[nodiscard]] QList<QString> getIcons(const QModelIndex& index) const override;
  [[nodiscard]] int getNumIcons(const QModelIndex& index) const override;

private:
  const PluginListView* m_View;
};

}  // namespace BSPluginList

#endif  // BSPLUGINLIST_FLAGICONDELEGATE_H
