#ifndef ETEST_APP_DIALOGS_ABOUTDIALOG_H_
#define ETEST_APP_DIALOGS_ABOUTDIALOG_H_

#include "AnimationDialog.h"

class QLabel;
class QPushButton;

namespace etest::app {

class AboutDialog : public AnimationDialog {
  Q_OBJECT

 public:
  explicit AboutDialog(QWidget* parent = nullptr);

 private:
  void initUi();

  QLabel* logo_label_;
  QLabel* name_label_;
  QLabel* version_label_;
  QLabel* desc_label_;
  QLabel* copyright_label_;
  QWidget* tech_chips_container_;
  QLabel* build_info_label_;
  QLabel* license_label_;
  QPushButton* ok_button_;
};

}  // namespace etest::app

#endif  // ETEST_APP_DIALOGS_ABOUTDIALOG_H_
