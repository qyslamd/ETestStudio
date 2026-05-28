#include "ActivityBarWidget.h"

#include <QVBoxLayout>

#include "AppIconProvider.h"
#include "ThemeManager.h"

namespace etest::app {

ActivityBarWidget::ActivityBarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();

  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &ActivityBarWidget::reloadIcons);
}

void ActivityBarWidget::setupUi() {
  setFixedWidth(48);
  setObjectName(QStringLiteral("sidebarActivityBar"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 4, 0, 4);
  layout->setSpacing(0);

  top_layout_ = new QVBoxLayout();
  top_layout_->setSpacing(4);
  top_layout_->setContentsMargins(0, 0, 0, 0);

  layout->addLayout(top_layout_);
  layout->addStretch();

  // 底部按钮（登录 > 设置）
  auto* bottom_layout = new QVBoxLayout();
  bottom_layout->setSpacing(0);
  bottom_layout->setContentsMargins(0, 0, 0, 0);
  login_btn_ = createButton(QStringLiteral("登录"));
  login_btn_->setIcon(AppIconProvider::instance().icon(QStringLiteral("account")));
  login_btn_->setIconSize(QSize(kNormalIconSize, kNormalIconSize));
  bottom_layout->addWidget(login_btn_);
  connect(login_btn_, &QPushButton::clicked, this,
          &ActivityBarWidget::loginTriggered);
  settings_btn_ = createButton(QStringLiteral("设置"));
  settings_btn_->setIcon(AppIconProvider::instance().icon(QStringLiteral("settings")));
  settings_btn_->setIconSize(QSize(kNormalIconSize, kNormalIconSize));
  bottom_layout->addWidget(settings_btn_);
  connect(settings_btn_, &QPushButton::clicked, this,
          &ActivityBarWidget::settingsTriggered);
  layout->addLayout(bottom_layout);
}

void ActivityBarWidget::addPage(const QString& id, const QString& tooltip,
                                const QString& iconName) {
  // 不重复添加相同 ID 的按钮
  for (const auto& p : pages_) {
    if (p.id == id) return;
  }

  pages_.append({id, iconName, tooltip});

  auto* btn = createButton(tooltip);
  btn->setIcon(AppIconProvider::instance().icon(iconName));
  btn->setIconSize(QSize(kNormalIconSize, kNormalIconSize));
  buttons_.append(btn);
  top_layout_->addWidget(btn);

  connect(btn, &QPushButton::clicked, this, [this, id]() {
    emit pageClicked(id);
  });

  // 默认选中第一个添加的页面
  if (buttons_.size() == 1) {
    setActivePageId(id);
  }
}

void ActivityBarWidget::reloadIcons() {
  for (int i = 0; i < buttons_.size(); ++i) {
    buttons_[i]->setIcon(AppIconProvider::instance().icon(pages_[i].iconName));
  }
  if (login_btn_) {
    login_btn_->setIcon(AppIconProvider::instance().icon(QStringLiteral("account")));
  }
  if (settings_btn_) {
    settings_btn_->setIcon(AppIconProvider::instance().icon(QStringLiteral("settings")));
  }
  updateActiveIconSize();
}

void ActivityBarWidget::setLoginState(bool loggedIn, const QString& userName,
                                      const QString& role) {
  if (loggedIn) {
    login_btn_->setToolTip(QStringLiteral("当前用户：%1 (%2)").arg(userName).arg(role));
  } else {
    login_btn_->setToolTip(QStringLiteral("登录"));
  }
}

void ActivityBarWidget::updateActiveIconSize() {
  for (int i = 0; i < buttons_.size(); ++i) {
    bool active = (pages_[i].id == active_page_id_);
    buttons_[i]->setIconSize(active ? QSize(kActiveIconSize, kActiveIconSize)
                                    : QSize(kNormalIconSize, kNormalIconSize));
  }
}

QPushButton* ActivityBarWidget::createButton(const QString& tooltip) {
  auto* btn = new QPushButton(this);
  btn->setToolTip(tooltip);
  btn->setFixedSize(48, 40);
  btn->setCheckable(true);
  btn->setFlat(true);
  btn->setFocusPolicy(Qt::NoFocus);
  return btn;
}

void ActivityBarWidget::setActivePageId(const QString& id) {
  active_page_id_ = id;
  for (int i = 0; i < pages_.size(); ++i) {
    buttons_[i]->setChecked(pages_[i].id == id);
  }
  updateActiveIconSize();
}

QString ActivityBarWidget::activePageId() const {
  return active_page_id_;
}

}  // namespace etest::app
