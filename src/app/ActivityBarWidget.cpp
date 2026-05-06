#include "ActivityBarWidget.h"

namespace etest {
namespace app {

ActivityBarWidget::ActivityBarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void ActivityBarWidget::setupUi() {
  setFixedWidth(48);


  layout_ = new QVBoxLayout(this);
  layout_->setContentsMargins(0, 4, 0, 4);
  layout_->setSpacing(0);

  // 顶部按钮区域
  top_layout_ = new QVBoxLayout();
  top_layout_->setSpacing(0);
  top_layout_->setContentsMargins(0, 0, 0, 0);

  // 主功能按钮（顶部）
  buttons_.append(createButton(QStringLiteral("资源管理器"), "E"));
  buttons_.append(createButton(QStringLiteral("搜索"), "S"));
  buttons_.append(createButton(QStringLiteral("源代码管理"), "G"));
  buttons_.append(createButton(QStringLiteral("调试"), "D"));
  buttons_.append(createButton(QStringLiteral("扩展"), "X"));
  buttons_.append(createButton(QStringLiteral("硬件"), "H"));

  for (int i = 0; i < buttons_.size(); ++i) {
    top_layout_->addWidget(buttons_[i]);
    connect(buttons_[i], &QPushButton::clicked, this, [this, i]() {
      if (active_index_ == i) {
        // 再次点击已选中的按钮，切换侧边栏显隐
        emit sidebarToggleRequested();
      } else {
        setActiveIndex(i);
        emit activityClicked(i);
      }
    });
  }

  layout_->addLayout(top_layout_);
  layout_->addStretch();

  // 底部按钮区域
  bottom_layout_ = new QVBoxLayout();
  bottom_layout_->setSpacing(0);
  bottom_layout_->setContentsMargins(0, 0, 0, 0);

  auto* settingsBtn = createButton(QStringLiteral("设置"), "G");
  bottom_layout_->addWidget(settingsBtn);

  layout_->addLayout(bottom_layout_);

  setActiveIndex(0);
}

QPushButton* ActivityBarWidget::createButton(const QString& tooltip,
                                             const QString& iconText) {
  QPushButton* btn = new QPushButton(iconText, this);
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

}  // namespace app
}  // namespace etest
