#ifndef GUI_MESSAGEDIALOG_H
#define GUI_MESSAGEDIALOG_H

#include <QDialog>

namespace Ui
{
class MessageDialog;
}

namespace GUI
{





class MessageDialog : public QDialog
{
  Q_OBJECT

public:








  explicit MessageDialog(const QString& text, QWidget* reference);

  ~MessageDialog();













  static void showMessage(const QString& text, QWidget* reference,
                          bool bringToFront = true);

protected:
  virtual void resizeEvent(QResizeEvent* event);

private:
  Ui::MessageDialog* ui;
};

}  // namespace GUI

#endif  // GUI_MESSAGEDIALOG_H
