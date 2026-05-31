#include "FlagIconDelegate.h"

#include <bit>

namespace BSPluginList
{

using enum TESData::FileInfo::EFlag;

FlagIconDelegate::FlagIconDelegate(PluginListView* view)
    : GUI::IconDelegate(view), m_View{view}
{}

QList<QString> FlagIconDelegate::getIcons(const QModelIndex& index) const
{
  const auto flags = m_View->fileFlags(index);

  QList<QString> icons;

  // Critical problems (missing masters, invalid ObjectIDs, broken blueprints)
  if (flags & FLAG_PROBLEMATIC) {
    icons.append(":/bsplugins/beacon-warning");
  }

  // LOOT messages / informational notices
  if (flags & FLAG_INFORMATION) {
    icons.append(":/bsplugins/comms");
  }

  // Has an associated INI file
  if (flags & FLAG_INI) {
    icons.append(":/bsplugins/datapad");
  }

  // Has associated BSA/BA2 archives
  if (flags & FLAG_BSA) {
    icons.append(":/bsplugins/cargo");
  }

  // ESM — master plugin (ringed planet)
  if (flags & FLAG_MASTER) {
    icons.append(":/bsplugins/planet");
  }

  // ESL — light plugin (comet)
  if (flags & FLAG_LIGHT) {
    icons.append(":/bsplugins/comet");
  }

  // Overlay — no record space consumed (hologram)
  if (flags & FLAG_OVERLAY) {
    icons.append(":/bsplugins/hologram");
  }

  // ESH — medium plugin (hex shield)
  if (flags & FLAG_MEDIUM) {
    icons.append(":/bsplugins/hex-shield");
  }

  // Blueprint — auto-loaded alongside paired main plugin (schematic)
  if (flags & FLAG_BLUEPRINT) {
    icons.append(":/bsplugins/schematic");
  }

  // LOOT verified clean
  if (flags & FLAG_CLEAN) {
    icons.append(":/bsplugins/scanner-ok");
  }

  // Locked load order position
  if (flags & FLAG_LOCKED) {
    icons.append(":/bsplugins/mag-lock");
  }

  return icons;
}

int FlagIconDelegate::getNumIcons(const QModelIndex& index) const
{
  return std::popcount(static_cast<uint>(m_View->fileFlags(index)));
}

}  // namespace BSPluginList
