#ifndef GUI_GENERICICONDELEGATE_H
#define GUI_GENERICICONDELEGATE_H

#include "IconDelegate.h"

namespace GUI
{




class GenericIconDelegate : public IconDelegate
{
  Q_OBJECT
public:













  GenericIconDelegate(QTreeView* parent, int role, int logicalIndex = -1,
                      int compactSize = 150);

private:
  [[nodiscard]] QList<QString> getIcons(const QModelIndex& index) const override;
  [[nodiscard]] int getNumIcons(const QModelIndex& index) const override;

private:
  int m_Role;
  int m_LogicalIndex;
  int m_CompactSize;
  bool m_Compact;
};

}  // namespace GUI

#endif  // GUI_GENERICICONDELEGATE_H
