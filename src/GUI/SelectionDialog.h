#ifndef GUI_SELECTIONDIALOG_H
#define GUI_SELECTIONDIALOG_H

#include <QAbstractButton>
#include <QDialog>

namespace Ui
{
class SelectionDialog;
}

namespace GUI
{

class SelectionDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SelectionDialog(const QString& description, QWidget* parent = 0,
                           const QSize& iconSize = QSize());

  SelectionDialog(const SelectionDialog&) = delete;
  SelectionDialog(SelectionDialog&&)      = delete;

  ~SelectionDialog() noexcept;

  SelectionDialog& operator=(const SelectionDialog&) = delete;
  SelectionDialog& operator=(SelectionDialog&&)      = delete;










  void addChoice(const QString& buttonText, const QString& description,
                 const QVariant& data);

  void addChoice(const QIcon& icon, const QString& buttonText,
                 const QString& description, const QVariant& data);

  int numChoices() const;

  QVariant getChoiceData();
  QString getChoiceString();
  QString getChoiceDescription();

  void disableCancel();

private slots:

  void on_buttonBox_clicked(QAbstractButton* button);

  void on_cancelButton_clicked();

private:
  Ui::SelectionDialog* ui;
  QAbstractButton* m_Choice;
  bool m_ValidateByData;
  QSize m_IconSize;
};

}  // namespace GUI

#endif  // GUI_SELECTIONDIALOG_H
