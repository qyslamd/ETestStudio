#include "ActivityBarWidget.h"

ActivityBarWidget::ActivityBarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void ActivityBarWidget::setupUi() {
  setFixedWidth(40);

  layout_ = new QVBoxLayout(this);
  layout_->setContentsMargins(0, 4, 0, 4);
  layout_->setSpacing(0);

  // 3个活动按钮：资源管理器、搜索、设置
  buttons_.append(createButton(QStringLiteral("资源管理器"), "E"));
  buttons_.append(createButton(QStringLiteral("全局搜索"), "S"));
  buttons_.append(createButton(QStringLiteral("设置"), "G"));

  for (int i = 0; i < buttons_.size(); ++i) {
    layout_->addWidget(buttons_[i]);
    connect(buttons_[i], &QPushButton::clicked, this, [this, i]() {
      setActiveIndex(i);
      emit activityClicked(i);
    });
  }

  layout_->addStretch();

  setActiveIndex(0);
}

QPushButton* ActivityBarWidget::createButton(const QString& tooltip,
                                             const QString& iconText) {
  QPushButton* btn = new QPushButton(iconText, this);
  btn->setToolTip(tooltip);
  btn->setFixedSize(36, 36);
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
