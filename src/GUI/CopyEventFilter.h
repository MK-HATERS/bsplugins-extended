#ifndef GUI_COPY_EVENT_FILTER_H
#define GUI_COPY_EVENT_FILTER_H

#include <QAbstractItemView>
#include <QModelIndex>
#include <QObject>

#include <functional>

namespace GUI
{











class CopyEventFilter : public QObject
{
  Q_OBJECT

public:
  explicit CopyEventFilter(QAbstractItemView* view, int column = 0,
                           int role = Qt::DisplayRole);
  CopyEventFilter(QAbstractItemView* view,
                  std::function<QString(const QModelIndex&)> format);




  void copySelection() const;

  bool eventFilter(QObject* sender, QEvent* event) override;

private:
  QAbstractItemView* m_View;
  std::function<QString(const QModelIndex&)> m_Format;
};

}  // namespace GUI

#endif  // GUI_COPY_EVENT_FILTER_H
