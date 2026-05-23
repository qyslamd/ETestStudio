#include "ActivityBarWidget.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>

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

  // 索引0：资源管理器
  buttons_.append(createButton(QStringLiteral("资源管理器"),
                                ":/resources/icons/svg/project_dark.svg",
                                ":/resources/icons/svg/project_light.svg"));
  // 索引1：搜索
  buttons_.append(createButton(QStringLiteral("搜索"),
                                ":/resources/icons/svg/search_dark.svg",
                                ":/resources/icons/svg/search_light.svg"));
  // 索引2：源代码管理
  buttons_.append(createButton(QStringLiteral("源代码管理"),
                                ":/resources/icons/svg/git_dark.svg",
                                ":/resources/icons/svg/git_light.svg"));
  // 索引3：调试
  buttons_.append(createButton(QStringLiteral("调试"),
                                ":/resources/icons/svg/debug_dark.svg",
                                ":/resources/icons/svg/debug_dark.svg"));
  // 索引4：扩展
  buttons_.append(createButton(QStringLiteral("扩展"),
                                ":/resources/icons/svg/extensions_dark.svg",
                                ":/resources/icons/svg/extensions_light.svg"));
  // 索引5：硬件
  buttons_.append(createButton(QStringLiteral("硬件"),
                                ":/resources/icons/svg/hardware_dark.svg",
                                ":/resources/icons/svg/hardware_light.svg"));
  // 索引6：协议
  buttons_.append(createButton(QStringLiteral("协议"),
                                ":/resources/icons/svg/protocol_dark.svg",
                                ":/resources/icons/svg/protocol_light.svg"));
  // 索引7：用例
  buttons_.append(createButton(QStringLiteral("用例"),
                                ":/resources/icons/svg/testprogram_dark.svg",
                                ":/resources/icons/svg/testprogram_light.svg"));

  for (int i = 0; i < buttons_.size(); ++i) {
    top_layout->addWidget(buttons_[i]);
    connect(buttons_[i], &QPushButton::clicked, this, [this, i]() {
      emit pageClicked(i);
    });
  }

  layout->addLayout(top_layout);
  layout->addStretch();

  // 底部设置按钮
  auto* bottom_layout = new QVBoxLayout();
  bottom_layout->setSpacing(0);
  bottom_layout->setContentsMargins(0, 0, 0, 0);
  auto* settings_btn = createButton(QStringLiteral("设置"),
                                     ":/resources/icons/svg/settings_dark.svg",
                                     ":/resources/icons/svg/settings_light.svg");
  bottom_layout->addWidget(settings_btn);
  connect(settings_btn, &QPushButton::clicked, this, &ActivityBarWidget::settingsTriggered);
  layout->addLayout(bottom_layout);

  setActiveIndex(0);
}

QPushButton* ActivityBarWidget::createButton(const QString& tooltip,
                                              const QString& darkIconPath,
                                              const QString& lightIconPath) {
  auto* btn = new QPushButton(this);
  btn->setToolTip(tooltip);
  btn->setFixedSize(48, 40);
  btn->setCheckable(true);
  btn->setFlat(true);
  btn->setFocusPolicy(Qt::NoFocus);

  QIcon icon;
  icon.addFile(darkIconPath, QSize(), QIcon::Normal, QIcon::Off);
  icon.addFile(lightIconPath, QSize(), QIcon::Disabled, QIcon::Off);
  btn->setIcon(icon);
  btn->setIconSize(QSize(24, 24));

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
