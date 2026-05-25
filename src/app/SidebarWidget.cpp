#include "SidebarWidget.h"
#include "FileExplorerWidget.h"
#include "GitWidget.h"
#include "HardwareTreeWidget.h"
#include "ProtocolManagerWidget.h"
#include "SearchWidget.h"
#include "TestProgramManagerWidget.h"

#include <QHBoxLayout>
#include <QLabel>

namespace etest::app {

SidebarWidget::SidebarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void SidebarWidget::setupUi() {
  auto* outer_layout = new QHBoxLayout(this);
  outer_layout->setContentsMargins(0, 0, 0, 0);
  outer_layout->setSpacing(0);

  // 内容面板
  content_panel_ = new QWidget(this);
  auto* content_layout = new QVBoxLayout(content_panel_);
  content_layout->setContentsMargins(0, 0, 0, 0);
  content_layout->setSpacing(0);

  // 视图标题栏
  auto* title_bar = new QWidget(this);
  title_bar->setObjectName(QStringLiteral("sidebarTitleBar"));
  title_bar->setFixedHeight(35);
  title_bar->setAutoFillBackground(true);
  auto* title_layout = new QHBoxLayout(title_bar);
  title_layout->setContentsMargins(12, 0, 8, 0);
  title_layout->setSpacing(4);

  title_label_ = new QLabel(this);
  title_layout->addWidget(title_label_);
  title_layout->addStretch();

  content_layout->addWidget(title_bar);

  // 内容区域
  stack_ = new QStackedWidget(this);

  // 页0：资源管理器
  file_explorer_ = new FileExplorerWidget(this);
  stack_->addWidget(file_explorer_);
  view_titles_ << QStringLiteral("资源管理器");

  // 页1：全局搜索
  search_widget_ = new SearchWidget(this);
  stack_->addWidget(search_widget_);
  view_titles_ << QStringLiteral("搜索");

  // 页2：源代码管理
  git_widget_ = new GitWidget(this);
  stack_->addWidget(git_widget_);
  view_titles_ << QStringLiteral("源代码管理");

  // 页3：调试占位
  auto* debugPage = new QWidget(this);
  auto* debugLayout = new QVBoxLayout(debugPage);
  debugLayout->setContentsMargins(0, 0, 0, 0);
  auto* debugLabel = new QLabel(QStringLiteral("调试\n（待实现）"), this);
  debugLabel->setAlignment(Qt::AlignCenter);
  debugLayout->addWidget(debugLabel);
  stack_->addWidget(debugPage);
  view_titles_ << QStringLiteral("调试");

  // 页5：硬件树
  hardware_tree_ = new HardwareTreeWidget(this);
  stack_->addWidget(hardware_tree_);
  view_titles_ << QStringLiteral("硬件");

  // 页6：协议管理器
  protocol_manager_ = new ProtocolManagerWidget(this);
  stack_->addWidget(protocol_manager_);
  view_titles_ << QStringLiteral("协议");

  // 页7：用例管理器
  test_program_manager_ = new TestProgramManagerWidget(this);
  stack_->addWidget(test_program_manager_);
  view_titles_ << QStringLiteral("用例");

  content_layout->addWidget(stack_);

  outer_layout->addWidget(content_panel_);

  switchPage(0);
}

int SidebarWidget::pageCount() const {
  return stack_->count();
}

void SidebarWidget::switchPage(int index) {
  if (index >= 0 && index < stack_->count()) {
    stack_->setCurrentIndex(index);
    if (index < view_titles_.size()) {
      title_label_->setText(view_titles_[index]);
    }
  }
}

void SidebarWidget::showContent() {
  content_panel_->show();
}

void SidebarWidget::hideContent() {
  content_panel_->hide();
}

bool SidebarWidget::isContentVisible() const {
  return content_panel_->isVisible();
}

FileExplorerWidget* SidebarWidget::fileExplorer() const {
  return file_explorer_;
}

HardwareTreeWidget* SidebarWidget::hardwareTree() const {
  return hardware_tree_;
}

ProtocolManagerWidget* SidebarWidget::protocolManager() const {
  return protocol_manager_;
}

SearchWidget* SidebarWidget::searchWidget() const {
  return search_widget_;
}

GitWidget* SidebarWidget::gitWidget() const {
  return git_widget_;
}

TestProgramManagerWidget* SidebarWidget::testProgramManager() const {
  return test_program_manager_;
}

}  // namespace etest::app
