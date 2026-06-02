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

  // MO2's standard icons — these match what the rest of MO2 uses.
  if (flags & FLAG_PROBLEMATIC) {
    icons.append(":/MO/gui/warning");
  }
  if (flags & FLAG_INFORMATION) {
    icons.append(":/MO/gui/information");
  }
  if (flags & FLAG_INI) {
    icons.append(":/MO/gui/attachment");
  }
  if (flags & FLAG_BSA) {
    icons.append(":/MO/gui/archive_conflict_neutral");
  }
  if (flags & FLAG_MASTER) {
    icons.append(":/bsplugins/star");
  }
  if (flags & FLAG_LIGHT) {
    icons.append(":/bsplugins/feather");
  }
  if (flags & FLAG_OVERLAY) {
    icons.append(":/MO/gui/instance_switch");
  }
  if (flags & FLAG_CLEAN) {
    icons.append(":/MO/gui/edit_clear");
  }
  if (flags & FLAG_LOCKED) {
    icons.append(":/MO/gui/locked");
  }

  // Starfield-specific flags live in the separate BS Info column
  // (BSFlagIconDelegate / COL_BSINFO) — not shown here.

  return icons;
}

int FlagIconDelegate::getNumIcons(const QModelIndex& index) const
{
  using enum TESData::FileInfo::EFlag;
  // Only count the flags rendered in this column (not our BS Info flags).
  constexpr uint kMOFlags = FLAG_PROBLEMATIC | FLAG_INFORMATION | FLAG_INI |
                            FLAG_BSA | FLAG_MASTER | FLAG_LIGHT | FLAG_OVERLAY |
                            FLAG_CLEAN | FLAG_LOCKED;
  return std::popcount(static_cast<uint>(m_View->fileFlags(index)) & kMOFlags);
}

// ---------------------------------------------------------------------------
// BSFlagIconDelegate — COL_BSINFO: Starfield-specific custom icons only.
// ---------------------------------------------------------------------------

BSFlagIconDelegate::BSFlagIconDelegate(PluginListView* view)
    : GUI::IconDelegate(view), m_View{view}
{}

QList<QString> BSFlagIconDelegate::getIcons(const QModelIndex& index) const
{
  const auto flags = m_View->fileFlags(index);
  QList<QString> icons;

  // ESH medium plugin
  if (flags & FLAG_MEDIUM) {
    icons.append(":/bsplugins/hex-shield");
  }
  // Blueprint auto-loaded alongside paired main plugin
  if (flags & FLAG_BLUEPRINT) {
    icons.append(":/bsplugins/schematic");
  }
  // Inferred patch — may need ordering fix
  if (flags & FLAG_PATCH_SUGGESTION) {
    icons.append(":/bsplugins/query-beacon");
  }

  return icons;
}

int BSFlagIconDelegate::getNumIcons(const QModelIndex& index) const
{
  using enum TESData::FileInfo::EFlag;
  constexpr uint kBSFlags = FLAG_MEDIUM | FLAG_BLUEPRINT | FLAG_PATCH_SUGGESTION;
  return std::popcount(static_cast<uint>(m_View->fileFlags(index)) & kBSFlags);
}

}  // namespace BSPluginList
