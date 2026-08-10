#pragma once

#include "dialogs/OverlayDialog.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QKeyEvent;

namespace etest::app {

class LoginDialog : public OverlayDialog {
  Q_OBJECT

 public:
  explicit LoginDialog(QWidget* parent = nullptr);

 protected:
  void keyReleaseEvent(QKeyEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void onLoginClicked();

  QLineEdit* usernameEdit_ = nullptr;
  QLineEdit* passwordEdit_ = nullptr;
  QCheckBox* rememberCheckBox_ = nullptr;
  QPushButton* loginButton_ = nullptr;
  QLabel* hintLabel_ = nullptr;
};

}  // namespace etest::app
