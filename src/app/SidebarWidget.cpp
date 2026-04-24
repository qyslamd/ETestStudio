#include "SidebarWidget.h"
#include "FileExplorerWidget.h"

namespace etest::app {

SidebarWidget::SidebarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void SidebarWidget::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  stack_ = new QStackedWidget(this);

  // 页0：资源管理器
  file_explorer_ = new FileExplorerWidget(this);
  stack_->addWidget(file_explorer_);

  // 页1：搜索占位
  auto* searchPage = new QWidget(this);
  auto* searchLayout = new QVBoxLayout(searchPage);
  auto* searchLabel = new QLabel(QStringLiteral("全局搜索"), this);
  searchLabel->setAlignment(Qt::AlignCenter);
  searchLayout->addWidget(searchLabel);
  stack_->addWidget(searchPage);

  // 页2：设置占位
  auto* settingsPage = new QWidget(this);
  auto* settingsLayout = new QVBoxLayout(settingsPage);
  auto* settingsLabel = new QLabel(QStringLiteral("设置"), this);
  settingsLabel->setAlignment(Qt::AlignCenter);
  settingsLayout->addWidget(settingsLabel);
  stack_->addWidget(settingsPage);

  layout->addWidget(stack_);
  setMinimumWidth(200);
}

void SidebarWidget::switchPage(int index) {
  if (index >= 0 && index < stack_->count()) {
    stack_->setCurrentIndex(index);
  }
}

FileExplorerWidget* SidebarWidget::fileExplorer() const {
  return file_explorer_;
}

}  // namespace etest::app
