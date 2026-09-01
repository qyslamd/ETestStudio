#include "LoginDialog.h"
#include "auth/AuthService.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace etest::app {

using etest::core::auth::AuthService;
using etest::core::auth::User;
using namespace etest::core::config;

LoginDialog::LoginDialog(QWidget* parent) : OverlayDialog(parent) {
  round_radius_ = 12;
  initUi();
  initSignals();

  // 加载记住的凭据
  auto& cfg = etest::core::config::ConfigManager::instance();
  QString savedUser = cfg.get<QString>(CONFIG_AUTH_REMEMBER_USERNAME);
  QString savedPass = cfg.get<QString>(CONFIG_AUTH_REMEMBER_PASSWORD);
  if (!savedUser.isEmpty()) {
    usernameEdit_->setText(savedUser);
    rememberCheckBox_->setChecked(true);
  }
  if (!savedPass.isEmpty()) {
    passwordEdit_->setText(savedPass);
  }
}

void LoginDialog::initUi() {
  auto* content = new QWidget;
  content->setObjectName(QStringLiteral("loginContent"));
  content->setFixedSize(440, 350);

  auto* hLayout = new QHBoxLayout(content);
  hLayout->setContentsMargins(0, 0, 0, 0);
  hLayout->setSpacing(0);

  // 左侧品牌区
  auto* brand = new QWidget(content);
  brand->setObjectName(QStringLiteral("loginBrand"));
  brand->setFixedWidth(160);
  auto* brandLayout = new QVBoxLayout(brand);
  brandLayout->setContentsMargins(24, 24, 24, 24);
  brandLayout->setSpacing(4);
  auto* brandTitle = new QLabel(QStringLiteral("ETest"), brand);
  brandTitle->setObjectName(QStringLiteral("loginBrandTitle"));
  auto* brandSub = new QLabel(QStringLiteral("测试系统"), brand);
  brandSub->setObjectName(QStringLiteral("loginBrandSub"));
  auto* brandHint = new QLabel(QStringLiteral("请登录以使用全部功能"), brand);
  brandHint->setObjectName(QStringLiteral("loginBrandHint"));
  brandLayout->addWidget(brandTitle);
  brandLayout->addWidget(brandSub);
  brandLayout->addStretch();
  brandLayout->addWidget(brandHint);
  hLayout->addWidget(brand);

  // 右侧表单区
  auto* form = new QWidget(content);
  form->setObjectName(QStringLiteral("loginForm"));
  auto* formLayout = new QVBoxLayout(form);
  formLayout->setContentsMargins(24, 24, 24, 24);
  formLayout->setSpacing(12);

  auto* titleRow = new QHBoxLayout;
  auto* titleLabel = new QLabel(QStringLiteral("登录"), form);
  titleLabel->setObjectName(QStringLiteral("loginFormTitle"));
  closeBtn_ = new QPushButton(QStringLiteral("×"), form);
  closeBtn_->setObjectName(QStringLiteral("loginCloseBtn"));
  closeBtn_->setFixedSize(28, 28);
  closeBtn_->setCursor(Qt::PointingHandCursor);
  titleRow->addWidget(titleLabel);
  titleRow->addStretch();
  titleRow->addWidget(closeBtn_);

  usernameEdit_ = new QLineEdit(form);
  usernameEdit_->setObjectName(QStringLiteral("loginUsername"));
  usernameEdit_->setPlaceholderText(QStringLiteral("admin"));

  passwordEdit_ = new QLineEdit(form);
  passwordEdit_->setObjectName(QStringLiteral("loginPassword"));
  passwordEdit_->setPlaceholderText(QStringLiteral("请输入密码"));
  passwordEdit_->setEchoMode(QLineEdit::Password);

  rememberCheckBox_ = new QCheckBox(QStringLiteral("记住密码"), form);
  rememberCheckBox_->setObjectName(QStringLiteral("loginRemember"));

  loginButton_ = new QPushButton(QStringLiteral("登录"), form);
  loginButton_->setObjectName(QStringLiteral("loginButton"));

  hintLabel_ = new QLabel(form);
  hintLabel_->setObjectName(QStringLiteral("loginHint"));
  hintLabel_->hide();

  formLayout->addLayout(titleRow);
  formLayout->addWidget(usernameEdit_);
  formLayout->addWidget(passwordEdit_);
  formLayout->addWidget(rememberCheckBox_);
  formLayout->addWidget(loginButton_);
  formLayout->addWidget(hintLabel_);
  formLayout->addStretch();
  hLayout->addWidget(form);

  setWidget(content);
  setWindowTitle(QStringLiteral("登录"));
}

void LoginDialog::initSignals() {
  // 关闭就意味着不登录
  connect(closeBtn_, &QPushButton::clicked, this, &QDialog::reject);
  connect(loginButton_, &QPushButton::clicked, this,
          &LoginDialog::onLoginClicked);
  connect(&AuthService::instance(), &AuthService::loginSucceeded, this,
          [this]() {
            // 保存或清除记住的密码
            auto& cfg = etest::core::config::ConfigManager::instance();
            if (rememberCheckBox_->isChecked()) {
              cfg.set(CONFIG_AUTH_REMEMBER_USERNAME, usernameEdit_->text());
              cfg.set(CONFIG_AUTH_REMEMBER_PASSWORD, passwordEdit_->text());
            } else {
              cfg.set(CONFIG_AUTH_REMEMBER_USERNAME, QString());
              cfg.set(CONFIG_AUTH_REMEMBER_PASSWORD, QString());
            }
            accept();
          });
  connect(&AuthService::instance(), &AuthService::loginFailed, this,
          [this](const QString& reason) {
            hintLabel_->setText(reason);
            hintLabel_->show();
          });
}

void LoginDialog::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    onLoginClicked();
  }
  OverlayDialog::keyReleaseEvent(event);
}

void LoginDialog::onLoginClicked() {
  hintLabel_->hide();
  AuthService::instance().login(usernameEdit_->text(), passwordEdit_->text());
}

}  // namespace etest::app
