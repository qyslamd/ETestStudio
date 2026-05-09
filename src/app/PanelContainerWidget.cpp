#include "PanelContainerWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QTabBar>
#include <QToolButton>

namespace etest {
namespace app {

PanelContainerWidget::PanelContainerWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void PanelContainerWidget::setupUi() {
  // 强制设置背景色，防止被QADS的样式覆盖
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor("#1E1E1E"));
  setPalette(pal);

  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // 标签栏区域：左侧tab + 右侧控制按钮
  auto* header_layout = new QHBoxLayout();
  header_layout->setContentsMargins(0, 0, 0, 0);
  header_layout->setSpacing(0);

  tab_widget_ = new QTabWidget(this);
  tab_widget_->setTabPosition(QTabWidget::North);
  tab_widget_->setDocumentMode(true);
  tab_widget_->tabBar()->setMovable(true);
  tab_widget_->setAutoFillBackground(true);
  QPalette tabPal = tab_widget_->palette();
  tabPal.setColor(QPalette::Window, QColor("#1E1E1E"));
  tab_widget_->setPalette(tabPal);

  // 右侧控制按钮：最大化 + 关闭
  max_button_ = new QToolButton(this);
  max_button_->setIcon(QIcon(":/resources/icons/svg/maximize_dark.svg"));
  max_button_->setToolTip(QStringLiteral("最大化面板"));
  max_button_->setAutoRaise(true);
  max_button_->setFixedSize(20, 20);

  close_button_ = new QToolButton(this);
  close_button_->setIcon(QIcon(":/resources/icons/svg/close_dark.svg"));
  close_button_->setToolTip(QStringLiteral("关闭面板"));
  close_button_->setAutoRaise(true);
  close_button_->setFixedSize(20, 20);

  // 将控制按钮放在tab widget的右上角角落
  auto* corner_widget = new QWidget(this);
  auto* corner_layout = new QHBoxLayout(corner_widget);
  corner_layout->setContentsMargins(4, 0, 4, 0);
  corner_layout->setSpacing(2);
  corner_layout->addStretch();
  corner_layout->addWidget(max_button_);
  corner_layout->addWidget(close_button_);

  tab_widget_->setCornerWidget(corner_widget, Qt::TopRightCorner);

  main_layout->addWidget(tab_widget_);

  // 信号连接
  connect(max_button_, &QToolButton::clicked, this, [this]() {
    maximized_ = !maximized_;
    if (maximized_) {
      max_button_->setIcon(QIcon(":/resources/icons/svg/restore_dark.svg"));
      max_button_->setToolTip(QStringLiteral("还原面板"));
      emit panelMaximized();
    } else {
      max_button_->setIcon(QIcon(":/resources/icons/svg/maximize_dark.svg"));
      max_button_->setToolTip(QStringLiteral("最大化面板"));
      emit panelRestored();
    }
  });

  connect(close_button_, &QToolButton::clicked, this,
          &PanelContainerWidget::panelClosed);
}

void PanelContainerWidget::addPanel(const QString& title, QWidget* panel) {
  tab_widget_->addTab(panel, title);
}

void PanelContainerWidget::setCurrentPanel(int index) {
  tab_widget_->setCurrentIndex(index);
}

int PanelContainerWidget::currentPanelIndex() const {
  return tab_widget_->currentIndex();
}

bool PanelContainerWidget::isMaximized() const {
  return maximized_;
}

void PanelContainerWidget::setMaximized(bool maximized) {
  maximized_ = maximized;
  if (maximized_) {
    max_button_->setIcon(QIcon(":/resources/icons/svg/restore_dark.svg"));
    max_button_->setToolTip(QStringLiteral("还原面板"));
  } else {
    max_button_->setIcon(QIcon(":/resources/icons/svg/maximize_dark.svg"));
    max_button_->setToolTip(QStringLiteral("最大化面板"));
  }
}

}  // namespace app
}  // namespace etest
