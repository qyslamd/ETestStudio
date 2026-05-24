#include "ActivityBarWidget.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>

#include "core/common/ThemeState.h"

namespace etest::app {

ActivityBarWidget::ActivityBarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
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
    QString dark;
    QString light;
  };

  // clang-format off
  const ButtonDef defs[] = {
    {QStringLiteral("资源管理器"),
     QStringLiteral(":/resources/icons/svg/project_dark.svg"),
     QStringLiteral(":/resources/icons/svg/project_light.svg")},
    {QStringLiteral("搜索"),
     QStringLiteral(":/resources/icons/svg/search_dark.svg"),
     QStringLiteral(":/resources/icons/svg/search_light.svg")},
    {QStringLiteral("源代码管理"),
     QStringLiteral(":/resources/icons/svg/git_dark.svg"),
     QStringLiteral(":/resources/icons/svg/git_light.svg")},
    {QStringLiteral("调试"),
     QStringLiteral(":/resources/icons/svg/debug_dark.svg"),
     QStringLiteral(":/resources/icons/svg/debug_light.svg")},
    {QStringLiteral("扩展"),
     QStringLiteral(":/resources/icons/svg/extensions_dark.svg"),
     QStringLiteral(":/resources/icons/svg/extensions_light.svg")},
    {QStringLiteral("硬件"),
     QStringLiteral(":/resources/icons/svg/hardware_dark.svg"),
     QStringLiteral(":/resources/icons/svg/hardware_light.svg")},
    {QStringLiteral("协议"),
     QStringLiteral(":/resources/icons/svg/protocol_dark.svg"),
     QStringLiteral(":/resources/icons/svg/protocol_light.svg")},
    {QStringLiteral("用例"),
     QStringLiteral(":/resources/icons/svg/testprogram_dark.svg"),
     QStringLiteral(":/resources/icons/svg/testprogram_light.svg")},
  };
  // clang-format on

  bool dark = core::common::isDarkTheme();
  for (const auto& d : defs) {
    icon_pairs_.append({d.dark, d.light});
    auto* btn = createButton(d.tip);
    btn->setIcon(QIcon(dark ? d.light : d.dark));
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
  icon_pairs_.append({
      QStringLiteral(":/resources/icons/svg/settings_dark.svg"),
      QStringLiteral(":/resources/icons/svg/settings_light.svg")});
  auto* settings_btn = createButton(QStringLiteral("设置"));
  settings_btn->setIcon(QIcon(dark ? icon_pairs_.last().light
                                   : icon_pairs_.last().dark));
  settings_btn->setIconSize(QSize(24, 24));
  bottom_layout->addWidget(settings_btn);
  connect(settings_btn, &QPushButton::clicked, this,
          &ActivityBarWidget::settingsTriggered);
  layout->addLayout(bottom_layout);

  setActiveIndex(0);
}

void ActivityBarWidget::reloadIcons() {
  bool dark = core::common::isDarkTheme();
  // 前 8 个按钮
  for (int i = 0; i < buttons_.size(); ++i) {
    buttons_[i]->setIcon(QIcon(dark ? icon_pairs_[i].light
                                    : icon_pairs_[i].dark));
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
