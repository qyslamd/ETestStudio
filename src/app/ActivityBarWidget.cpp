#include "ActivityBarWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "IconProvider.h"
#include "ThemeManager.h"

namespace etest::app {

ActivityBarWidget::ActivityBarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();

  // 主题切换时刷新图标
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &ActivityBarWidget::reloadIcons);
}

void ActivityBarWidget::setupUi() {
  setFixedWidth(48);
  setObjectName(QStringLiteral("sidebarActivityBar"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 4, 0, 4);
  layout->setSpacing(0);

  auto* top_layout = new QVBoxLayout();
  top_layout->setSpacing(4);
  top_layout->setContentsMargins(0, 0, 0, 0);

  struct ButtonDef {
    QString tip;
    QString iconName;
  };

  // clang-format off
  const ButtonDef defs[] = {
    {QStringLiteral("资源管理器"), QStringLiteral("project")},
    {QStringLiteral("搜索"),        QStringLiteral("search")},
    {QStringLiteral("源代码管理"),  QStringLiteral("git")},
    {QStringLiteral("调试"),        QStringLiteral("debug")},
    {QStringLiteral("硬件"),        QStringLiteral("hardware")},
    {QStringLiteral("协议"),        QStringLiteral("protocol")},
    {QStringLiteral("用例"),        QStringLiteral("testprogram")},
  };
  // clang-format on

  for (const auto& d : defs) {
    icon_names_.append(d.iconName);
    auto* btn = createButton(d.tip);
    btn->setIcon(IconProvider::instance().icon(d.iconName));
    btn->setIconSize(QSize(24, 24));
    buttons_.append(btn);
    top_layout->addWidget(btn);
    connect(btn, &QPushButton::clicked, this, [this, i = buttons_.size() - 1]() {
      emit pageClicked(i);
    });
  }

  layout->addLayout(top_layout);
  layout->addStretch();

  // 底部设置按钮
  auto* bottom_layout = new QVBoxLayout();
  bottom_layout->setSpacing(0);
  bottom_layout->setContentsMargins(0, 0, 0, 0);
  icon_names_.append(QStringLiteral("settings"));
  auto* settings_btn = createButton(QStringLiteral("设置"));
  settings_btn->setIcon(IconProvider::instance().icon(QStringLiteral("settings")));
  settings_btn->setIconSize(QSize(24, 24));
  buttons_.append(settings_btn);
  bottom_layout->addWidget(settings_btn);
  connect(settings_btn, &QPushButton::clicked, this,
          &ActivityBarWidget::settingsTriggered);
  layout->addLayout(bottom_layout);

  setActiveIndex(0);
}

void ActivityBarWidget::reloadIcons() {
  // 前 8 个按钮
  for (int i = 0; i < buttons_.size(); ++i) {
    buttons_[i]->setIcon(IconProvider::instance().icon(icon_names_[i]));
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

void ActivityBarWidget::setActiveIndex(int index) {
  if (index < 0 || index >= buttons_.size()) return;
  active_index_ = index;
  for (int i = 0; i < buttons_.size(); ++i) {
    buttons_[i]->setChecked(i == index);
  }
}

int ActivityBarWidget::activeIndex() const {
  return active_index_;
}

}  // namespace etest::app
