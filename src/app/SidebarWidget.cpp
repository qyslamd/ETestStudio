#include "SidebarWidget.h"
#include "FileExplorerWidget.h"
#include "HardwareTreeWidget.h"
#include "SearchWidget.h"

#include <QHBoxLayout>
#include <QPalette>

namespace etest::app {

SidebarWidget::SidebarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void SidebarWidget::setupUi() {

  // 强制设置背景色，防止被QADS的样式覆盖
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor("#252526"));
  setPalette(pal);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // 视图标题栏
  auto* title_bar = new QWidget(this);
  title_bar->setFixedHeight(35);
  title_bar->setAutoFillBackground(true);
  QPalette titlePal = title_bar->palette();
  titlePal.setColor(QPalette::Window, QColor("#252526"));
  title_bar->setPalette(titlePal);
  auto* title_layout = new QHBoxLayout(title_bar);
  title_layout->setContentsMargins(12, 0, 8, 0);
  title_layout->setSpacing(4);

  title_label_ = new QLabel(this);
  title_layout->addWidget(title_label_);
  title_layout->addStretch();

  layout->addWidget(title_bar);

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

  // 页2：源代码管理占位
  auto* gitPage = new QWidget(this);
  auto* gitLayout = new QVBoxLayout(gitPage);
  gitLayout->setContentsMargins(0, 0, 0, 0);
  auto* gitLabel = new QLabel(QStringLiteral("源代码管理\n（待实现）"), this);
  gitLabel->setAlignment(Qt::AlignCenter);
  gitLayout->addWidget(gitLabel);
  stack_->addWidget(gitPage);
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

  // 页4：扩展占位
  auto* extPage = new QWidget(this);
  auto* extLayout = new QVBoxLayout(extPage);
  extLayout->setContentsMargins(0, 0, 0, 0);
  auto* extLabel = new QLabel(QStringLiteral("扩展\n（待实现）"), this);
  extLabel->setAlignment(Qt::AlignCenter);
  extLayout->addWidget(extLabel);
  stack_->addWidget(extPage);
  view_titles_ << QStringLiteral("扩展");

  // 页5：硬件树
  hardware_tree_ = new HardwareTreeWidget(this);
  stack_->addWidget(hardware_tree_);
  view_titles_ << QStringLiteral("硬件");

  layout->addWidget(stack_);
  setMinimumWidth(200);

  // 初始化标题
  switchPage(0);
}

void SidebarWidget::switchPage(int index) {
  if (index >= 0 && index < stack_->count()) {
    stack_->setCurrentIndex(index);
    if (index < view_titles_.size()) {
      title_label_->setText(view_titles_[index]);
    }
  }
}

FileExplorerWidget* SidebarWidget::fileExplorer() const {
  return file_explorer_;
}

HardwareTreeWidget* SidebarWidget::hardwareTree() const {
  return hardware_tree_;
}

SearchWidget* SidebarWidget::searchWidget() const {
  return search_widget_;
}

}  // namespace etest::app