#include "ActivityBarWidget.h"

#include <QFile>

namespace etest {
namespace app {

ActivityBarWidget::ActivityBarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void ActivityBarWidget::setupUi() {
  setFixedWidth(48);

  // 强制设置背景色，防止被QADS的样式覆盖
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor("#333333"));
  setPalette(pal);

  layout_ = new QVBoxLayout(this);
  layout_->setContentsMargins(0, 4, 0, 4);
  layout_->setSpacing(0);

  // 顶部按钮区域
  top_layout_ = new QVBoxLayout();
  top_layout_->setSpacing(4);
  top_layout_->setContentsMargins(0, 0, 0, 0);

  // 主功能按钮（顶部）
  // 索引0：资源管理器
  buttons_.append(createButton(QStringLiteral("资源管理器"),
                                ":/resources/icons/svg/project_dark.svg",
                                ":/resources/icons/svg/project_light.svg"));
  // 索引1：硬件拓扑（特殊处理，弹中央dock）
  buttons_.append(createButton(QStringLiteral("硬件拓扑"),
                                ":/resources/icons/svg/topology_dark.svg",
                                ":/resources/icons/svg/topology_light.svg"));
  // 索引2：搜索
  buttons_.append(createButton(QStringLiteral("搜索"),
                                ":/resources/icons/svg/search_dark.svg",
                                ":/resources/icons/svg/search_light.svg"));
  // 索引3：源代码管理
  buttons_.append(createButton(QStringLiteral("源代码管理"),
                                ":/resources/icons/svg/git_dark.svg",
                                ":/resources/icons/svg/git_light.svg"));
  // 索引4：调试
  buttons_.append(createButton(QStringLiteral("调试"),
                                ":/resources/icons/svg/debug_dark.svg",
                                ":/resources/icons/svg/debug_dark.svg"));
  // 索引5：扩展
  buttons_.append(createButton(QStringLiteral("扩展"),
                                ":/resources/icons/svg/extensions_dark.svg",
                                ":/resources/icons/svg/extensions_light.svg"));
  // 索引6：硬件
  buttons_.append(createButton(QStringLiteral("硬件"),
                                ":/resources/icons/svg/hardware_dark.svg",
                                ":/resources/icons/svg/hardware_light.svg"));

  for (int i = 0; i < buttons_.size(); ++i) {
    top_layout_->addWidget(buttons_[i]);
    connect(buttons_[i], &QPushButton::clicked, this, [this, i]() {
      if (i == 1) {
        // 硬件拓扑按钮：弹中央dock，不切换sidebar页面
        setActiveIndex(i);
        emit topologyClicked();
      } else if (active_index_ == i) {
        emit sidebarToggleRequested();
      } else {
        setActiveIndex(i);
        emit activityClicked(i);
      }
    });
  }

  layout_->addLayout(top_layout_);
  layout_->addStretch();

  // 底部按钮区域（设置按钮）
  bottom_layout_ = new QVBoxLayout();
  bottom_layout_->setSpacing(0);
  bottom_layout_->setContentsMargins(0, 0, 0, 0);

  buttons_.append(createButton(QStringLiteral("设置"),
                               ":/resources/icons/svg/settings_dark.svg",
                               ":/resources/icons/svg/settings_light.svg"));
  int settingsIndex = buttons_.size() - 1;
  bottom_layout_->addWidget(buttons_[settingsIndex]);
  connect(buttons_[settingsIndex], &QPushButton::clicked, this,
          [this]() { emit settingsTriggered(); });

  layout_->addLayout(bottom_layout_);

  setActiveIndex(0);
}

QPushButton* ActivityBarWidget::createButton(const QString& tooltip,
                                             const QString& darkIconPath,
                                             const QString& lightIconPath) {
  QPushButton* btn = new QPushButton(this);
  btn->setToolTip(tooltip);
  btn->setFixedSize(48, 40);
  btn->setCheckable(true);
  btn->setFlat(true);
  btn->setFocusPolicy(Qt::NoFocus);

  // 加载图标，优先dark版本（当前为暗色主题）
  QIcon icon;
  icon.addFile(darkIconPath, QSize(), QIcon::Normal, QIcon::Off);
  icon.addFile(lightIconPath, QSize(), QIcon::Disabled, QIcon::Off);
  btn->setIcon(icon);
  btn->setIconSize(QSize(24, 24));
  btn->setText(QString());

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

}  // namespace app
}  // namespace etest
