#pragma once

#include "auth/AuthTypes.h"
#include "dialogs/OverlayDialog.h"

class QTableWidget;
class QLabel;
class QToolButton;

namespace etest::app {

class UserManagerDialog : public OverlayDialog {
  Q_OBJECT

 public:
  explicit UserManagerDialog(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();
  void refreshUserList();
  void onAddUser();
  void onEditUser(int row);
  void onDeleteUser(int row);
  bool showUserForm(
      bool isAdd,
      const QString& userName = {},
      const QString& password = {},
      etest::core::auth::UserRole role = etest::core::auth::UserRole::User);

 private:
  QToolButton* closeBtn_ = nullptr;
  QTableWidget* table_ = nullptr;
  QLabel* countLabel_ = nullptr;
};

}  // namespace etest::app
