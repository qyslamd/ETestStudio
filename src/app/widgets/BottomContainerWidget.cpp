#include "BottomContainerWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTabBar>
#include <QToolButton>

#include "AppIconProvider.h"
#include "ThemeManager.h"
#include "libui/styles/TabBarStyle.h"

namespace etest::app {

BottomContainerWidget::BottomContainerWidget(QWidget* parent)
    : QWidget(parent) {
  initUi();
}

void BottomContainerWidget::initUi() {
  setAutoFillBackground(true);

  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // 标签栏区域：左侧tab + 右侧关闭按钮
  auto* header_layout = new QHBoxLayout();
  header_layout->setContentsMargins(0, 0, 0, 0);
  header_layout->setSpacing(0);

  tab_widget_ = new QTabWidget(this);
  tab_widget_->setTabPosition(QTabWidget::North);
  tab_widget_->setDocumentMode(true);
  tab_widget_->tabBar()->setMovable(true);
  tab_widget_->tabBar()->setElideMode(Qt::ElideRight);
  tab_widget_->tabBar()->setUsesScrollButtons(true);
  tab_widget_->setAutoFillBackground(true);
  TabBarStyle::install(tab_widget_->tabBar());

  // 关闭按钮
  close_button_ = new QToolButton(this);
  close_button_->setIcon(AppIconProvider::instance().icon("close"));
  close_button_->setToolTip(QStringLiteral("关闭面板"));
  close_button_->setAutoRaise(true);
  close_button_->setFixedSize(20, 20);

  // 将关闭按钮放在tab widget的右上角角落
  auto* corner_widget = new QWidget(this);
  auto* corner_layout = new QHBoxLayout(corner_widget);
  corner_layout->setContentsMargins(4, 0, 4, 0);
  corner_layout->setSpacing(2);
  corner_layout->addStretch();
  corner_layout->addWidget(close_button_);

  tab_widget_->setCornerWidget(corner_widget, Qt::TopRightCorner);

  main_layout->addWidget(tab_widget_);

  connect(close_button_, &QToolButton::clicked, this,
          &BottomContainerWidget::panelClosed);

  // Theme change: refresh close icon
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this](bool) {
            close_button_->setIcon(AppIconProvider::instance().icon("close"));
          });

  // Theme change: refresh tab icons
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this]() {
            for (int i = 0; i < tab_icon_names_.size(); ++i) {
              if (!tab_icon_names_.at(i).isEmpty()) {
                tab_widget_->setTabIcon(
                    i, AppIconProvider::instance().icon(tab_icon_names_.at(i)));
              }
            }
          });
}

void BottomContainerWidget::addPanel(const QString& title,
                                     QWidget* panel,
                                     const QString& iconName) {
  int index = tab_widget_->addTab(panel, title);
  tab_icon_names_.append(iconName);
  if (!iconName.isEmpty()) {
    tab_widget_->setTabIcon(index, AppIconProvider::instance().icon(iconName));
  }
}

void BottomContainerWidget::setCurrentPanel(int index) {
  tab_widget_->setCurrentIndex(index);
}

int BottomContainerWidget::currentPanelIndex() const {
  return tab_widget_->currentIndex();
}

}  // namespace etest::app
