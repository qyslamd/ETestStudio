#include "BottomContainerWidget.h"

#include <QApplication>
#include <QEvent>
#include <QTabBar>

#include "AppIconProvider.h"
#include "ThemeManager.h"
#include "libui/styles/TabBarStyle.h"

namespace etest::app {

using etest::core_ui::AppIconProvider;
using etest::core_ui::ThemeManager;

BottomContainerWidget::BottomContainerWidget(QWidget* parent)
    : QWidget(parent) {
  initUi();
}

void BottomContainerWidget::initUi() {
  setAutoFillBackground(true);

  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  tab_widget_ = new QTabWidget(this);
  tab_widget_->setTabPosition(QTabWidget::North);
  tab_widget_->setDocumentMode(true);
  tab_widget_->setTabsClosable(true);
  tab_widget_->tabBar()->setMovable(true);
  tab_widget_->tabBar()->setElideMode(Qt::ElideRight);
  tab_widget_->tabBar()->setUsesScrollButtons(true);
  tab_widget_->setAutoFillBackground(true);
  tab_widget_->setObjectName(QStringLiteral("bottomTabWidget"));
  TabBarStyle::install(tab_widget_->tabBar());

  // test
  // tab_widget_->setTabPosition(QTabWidget::South);

  main_layout->addWidget(tab_widget_);

  // 关闭 tab → 隐藏面板
  connect(tab_widget_, &QTabWidget::tabCloseRequested, this, [this](int index) {
    setPanelVisible(index, false);
    emit panelVisibilityChanged();
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

void BottomContainerWidget::setPanelVisible(int index, bool visible) {
  if (index < 0 || index >= tab_widget_->count())
    return;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  tab_widget_->tabBar()->setTabVisible(index, visible);
#else
  tab_widget_->setTabEnabled(index, visible);
  // setTabEnabled 不触发 layoutTabs()，手动发 StyleChange 事件强制重排
  QEvent styleEvent(QEvent::StyleChange);
  QApplication::sendEvent(tab_widget_->tabBar(), &styleEvent);
#endif
  if (visible)
    tab_widget_->setCurrentIndex(index);
}

bool BottomContainerWidget::isPanelVisible(int index) const {
  if (index < 0 || index >= tab_widget_->count())
    return false;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  return tab_widget_->tabBar()->isTabVisible(index);
#else
  return tab_widget_->isTabEnabled(index);
#endif
}

int BottomContainerWidget::indexOf(QWidget* panel) const {
  return tab_widget_->indexOf(panel);
}

int BottomContainerWidget::count() const {
  return tab_widget_->count();
}

void BottomContainerWidget::setCurrentPanel(int index) {
  tab_widget_->setCurrentIndex(index);
}

int BottomContainerWidget::currentPanelIndex() const {
  return tab_widget_->currentIndex();
}

}  // namespace etest::app
