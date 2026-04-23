#ifndef ETEST_APP_DIALOGS_NEWPROJECTDIALOG_H_
#define ETEST_APP_DIALOGS_NEWPROJECTDIALOG_H_

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace etest {
namespace app {

class NewProjectDialog : public QDialog {
  Q_OBJECT

 public:
  explicit NewProjectDialog(QWidget* parent = nullptr);
  ~NewProjectDialog() override;

  QString projectName() const;
  QString projectLocation() const;
  QString fullProjectPath() const;

 private Q_SLOTS:
  void onBrowseClicked();
  void onTextChanged();

 private:
  void initUi();
  void initSignals();
  void validateInputs();
  void updatePreview();

  QLineEdit* name_edit_;
  QLineEdit* location_edit_;
  QPushButton* browse_button_;
  QLabel* preview_label_;
  QLabel* error_label_;
  QPushButton* create_button_;
  QPushButton* cancel_button_;
};

}  // namespace app
}  // namespace etest

#endif  // ETEST_APP_DIALOGS_NEWPROJECTDIALOG_H_
