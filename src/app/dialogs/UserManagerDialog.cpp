#include "UserManagerDialog.h"
#include "auth/AuthService.h"
#include "auth/UserManager.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace etest::app {

using namespace etest::core::auth;

UserManagerDialog::UserManagerDialog(QWidget* parent)
    : AnimationDialog(parent) {
  round_radius_ = 12;
  initUi();
  initSignals();
  refreshUserList();
}

void UserManagerDialog::initUi() {
  auto* content = new QWidget;
  content->setObjectName(QStringLiteral("userMgrContent"));
  content->setFixedSize(520, 400);

  auto* mainLayout = new QVBoxLayout(content);
  mainLayout->setContentsMargins(24, 20, 24, 20);
  mainLayout->setSpacing(12);

  // Header
  auto* headerRow = new QHBoxLayout;
  auto* title = new QLabel(QStringLiteral("用户管理"), content);
  title->setObjectName(QStringLiteral("userMgrTitle"));
  countLabel_ = new QLabel(content);
  countLabel_->setObjectName(QStringLiteral("userMgrCount"));
  auto* addBtn = new QToolButton(content);
  addBtn->setText(QStringLiteral("+ 添加用户"));
  addBtn->setObjectName(QStringLiteral("userMgrAddBtn"));
  addBtn->setCursor(Qt::PointingHandCursor);
  connect(addBtn, &QToolButton::clicked, this, &UserManagerDialog::onAddUser);
  auto* closeBtn = new QToolButton(content);
  closeBtn->setText(QStringLiteral("×"));
  closeBtn->setObjectName(QStringLiteral("userMgrCloseBtn"));
  closeBtn->setFixedSize(28, 28);
  closeBtn->setCursor(Qt::PointingHandCursor);
  connect(closeBtn, &QToolButton::clicked, this, [this]() {
    actHideAnimation();
  });
  headerRow->addWidget(title);
  headerRow->addWidget(countLabel_);
  headerRow->addStretch();
  headerRow->addWidget(addBtn);
  headerRow->addWidget(closeBtn);
  mainLayout->addLayout(headerRow);

  // Table
  table_ = new QTableWidget(content);
  table_->setObjectName(QStringLiteral("userMgrTable"));
  table_->setColumnCount(3);
  table_->setHorizontalHeaderLabels(
      {QStringLiteral("用户名"), QStringLiteral("角色"), QStringLiteral("操作")});
  table_->horizontalHeader()->setStretchLastSection(false);
  table_->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Stretch);
  table_->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  table_->verticalHeader()->hide();
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  mainLayout->addWidget(table_);

  // Hint
  auto* hint = new QLabel(
      QStringLiteral("admin 用户不可删除"), content);
  hint->setObjectName(QStringLiteral("userMgrHint"));
  mainLayout->addWidget(hint);

  setWidget(content);
  setWindowTitle(QStringLiteral("用户管理"));
}

void UserManagerDialog::initSignals() {
  // 表内按钮的连接在 refreshUserList 中动态建立
}

void UserManagerDialog::refreshUserList() {
  auto users = UserManager::instance().allUsers();
  countLabel_->setText(
      QStringLiteral("共 %1 个用户").arg(users.size()));
  table_->setRowCount(users.size());

  for (int i = 0; i < users.size(); ++i) {
    const auto& u = users[i];

    auto* nameItem = new QTableWidgetItem(u.userName);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    table_->setItem(i, 0, nameItem);

    auto* roleItem = new QTableWidgetItem(
        u.role == UserRole::Admin ? QStringLiteral("Admin")
                                  : QStringLiteral("User"));
    roleItem->setFlags(roleItem->flags() & ~Qt::ItemIsEditable);
    table_->setItem(i, 1, roleItem);

    // 操作按钮容器
    auto* actionsWidget = new QWidget;
    auto* actionsLayout = new QHBoxLayout(actionsWidget);
    actionsLayout->setContentsMargins(4, 2, 4, 2);
    actionsLayout->setSpacing(8);

    auto* editBtn = new QToolButton(actionsWidget);
    editBtn->setText(QStringLiteral("编辑"));
    editBtn->setObjectName(QStringLiteral("userMgrEditBtn"));
    editBtn->setCursor(Qt::PointingHandCursor);
    connect(editBtn, &QToolButton::clicked, this, [this, i]() {
      onEditUser(i);
    });
    actionsLayout->addWidget(editBtn);

    if (u.userName != "admin") {
      auto* deleteBtn = new QToolButton(actionsWidget);
      deleteBtn->setText(QStringLiteral("删除"));
      deleteBtn->setObjectName(QStringLiteral("userMgrDeleteBtn"));
      deleteBtn->setCursor(Qt::PointingHandCursor);
      connect(deleteBtn, &QToolButton::clicked, this, [this, i]() {
        onDeleteUser(i);
      });
      actionsLayout->addWidget(deleteBtn);
    }

    table_->setCellWidget(i, 2, actionsWidget);
  }
}

bool UserManagerDialog::showUserForm(
    bool isAdd, const QString& userName, const QString& password,
    UserRole role) {
  QDialog dlg(this);
  dlg.setWindowTitle(isAdd ? QStringLiteral("添加用户")
                           : QStringLiteral("编辑用户"));
  dlg.setFixedSize(320, 280);

  auto* layout = new QVBoxLayout(&dlg);
  layout->setSpacing(12);

  auto* nameEdit = new QLineEdit(&dlg);
  nameEdit->setPlaceholderText(QStringLiteral("用户名"));
  nameEdit->setText(userName);
  if (!isAdd) {
    nameEdit->setReadOnly(true);
  }

  auto* passEdit = new QLineEdit(&dlg);
  passEdit->setPlaceholderText(QStringLiteral("密码"));
  passEdit->setEchoMode(QLineEdit::Password);
  if (!isAdd && password.isEmpty())
    passEdit->setPlaceholderText(QStringLiteral("留空则不修改"));

  auto* confirmEdit = new QLineEdit(&dlg);
  confirmEdit->setPlaceholderText(QStringLiteral("确认密码"));
  confirmEdit->setEchoMode(QLineEdit::Password);

  auto* roleCombo = new QComboBox(&dlg);
  roleCombo->addItem(QStringLiteral("Admin"));
  roleCombo->addItem(QStringLiteral("User"));
  roleCombo->setCurrentIndex(static_cast<int>(role));

  auto* hintLabel = new QLabel(&dlg);
  hintLabel->setObjectName(QStringLiteral("userMgrErrorHint"));
  hintLabel->hide();

  auto* btnRow = new QHBoxLayout;
  auto* cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
  auto* okBtn = new QPushButton(
      isAdd ? QStringLiteral("添加") : QStringLiteral("保存"), &dlg);
  btnRow->addStretch();
  btnRow->addWidget(cancelBtn);
  btnRow->addWidget(okBtn);

  layout->addWidget(new QLabel(isAdd ? QStringLiteral("添加用户")
                                     : QStringLiteral("编辑用户"), &dlg));
  layout->addWidget(nameEdit);
  layout->addWidget(passEdit);
  layout->addWidget(confirmEdit);
  layout->addWidget(roleCombo);
  layout->addWidget(hintLabel);
  layout->addLayout(btnRow);

  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  connect(okBtn, &QPushButton::clicked, &dlg, [&]() {
    if (passEdit->text() != confirmEdit->text()) {
      hintLabel->setText(QStringLiteral("两次输入的密码不一致"));
      hintLabel->show();
      return;
    }
    if (isAdd && nameEdit->text().isEmpty()) {
      hintLabel->setText(QStringLiteral("用户名不能为空"));
      hintLabel->show();
      return;
    }
    if (isAdd && passEdit->text().isEmpty()) {
      hintLabel->setText(QStringLiteral("密码不能为空"));
      hintLabel->show();
      return;
    }
    dlg.accept();
  });

  if (dlg.exec() != QDialog::Accepted)
    return false;

  // 获取并执行操作
  auto& um = UserManager::instance();
  bool ok = false;
  if (isAdd) {
    ok = um.addUser(nameEdit->text(), passEdit->text(),
                    static_cast<UserRole>(roleCombo->currentIndex()));
  } else {
    ok = um.updateUser(userName, passEdit->text(),
                       static_cast<UserRole>(roleCombo->currentIndex()));
  }
  if (!ok) {
    QMessageBox::warning(this, QStringLiteral("错误"), um.lastError());
  }
  return ok;
}

void UserManagerDialog::onAddUser() {
  if (showUserForm(true))
    refreshUserList();
}

void UserManagerDialog::onEditUser(int row) {
  auto users = UserManager::instance().allUsers();
  if (row < 0 || row >= users.size()) return;
  const auto& u = users[row];
  if (showUserForm(false, u.userName, {}, u.role))
    refreshUserList();
}

void UserManagerDialog::onDeleteUser(int row) {
  auto users = UserManager::instance().allUsers();
  if (row < 0 || row >= users.size()) return;
  const auto& u = users[row];
  auto ret = QMessageBox::question(
      this, QStringLiteral("确认删除"),
      QStringLiteral("确定要删除用户 \"%1\" 吗？").arg(u.userName));
  if (ret == QMessageBox::Yes) {
    if (UserManager::instance().deleteUser(u.userName)) {
      refreshUserList();
    } else {
      QMessageBox::warning(this, QStringLiteral("错误"),
                           UserManager::instance().lastError());
    }
  }
}

}  // namespace etest::app
