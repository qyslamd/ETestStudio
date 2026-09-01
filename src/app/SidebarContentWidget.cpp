#include "SidebarContentWidget.h"
#include "GitWidget.h"
#include "HardwareTreeWidget.h"
#include "ProtocolManagerWidget.h"
#include "SearchWidget.h"
#include "TestProgramManagerWidget.h"
#include "TopologyManagerWidget.h"

#include <QHBoxLayout>
#include <QLabel>

namespace etest::app {

SidebarContentWidget::SidebarContentWidget(QWidget* parent) : QWidget(parent) {
  initUi();
}

void SidebarContentWidget::initUi() {
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
  content_layout->addWidget(stack_);

  outer_layout->addWidget(content_panel_);
}

void SidebarContentWidget::addPage(const QString& id,
                            QWidget* page,
                            const QString& title) {
  if (id_to_index_.contains(id))
    return;

  int index = stack_->count();
  id_to_index_[id] = index;
  id_to_title_[id] = title;
  id_order_.append(id);
  stack_->addWidget(page);

  // 记录类型安全指针
  if (auto* hw = qobject_cast<HardwareTreeWidget*>(page)) {
    hardware_tree_ = hw;
  } else if (auto* pm = qobject_cast<ProtocolManagerWidget*>(page)) {
    protocol_manager_ = pm;
  } else if (auto* sw = qobject_cast<SearchWidget*>(page)) {
    search_widget_ = sw;
  } else if (auto* gw = qobject_cast<GitWidget*>(page)) {
    git_widget_ = gw;
  } else if (auto* tp = qobject_cast<TestProgramManagerWidget*>(page)) {
    test_program_manager_ = tp;
  } else if (auto* tm = qobject_cast<TopologyManagerWidget*>(page)) {
    topology_manager_ = tm;
  }

  // 默认显示第一个页面
  if (id_order_.size() == 1) {
    switchPage(id);
  }
}

void SidebarContentWidget::switchPage(const QString& id) {
  auto it = id_to_index_.constFind(id);
  if (it != id_to_index_.constEnd()) {
    stack_->setCurrentIndex(it.value());
    current_page_id_ = id;
    auto titleIt = id_to_title_.constFind(id);
    if (titleIt != id_to_title_.constEnd()) {
      title_label_->setText(titleIt.value());
    }
  }
}

QString SidebarContentWidget::currentPageId() const {
  return current_page_id_;
}

QWidget* SidebarContentWidget::pageById(const QString& id) const {
  auto it = id_to_index_.constFind(id);
  if (it != id_to_index_.constEnd()) {
    return stack_->widget(it.value());
  }
  return nullptr;
}

int SidebarContentWidget::pageCount() const {
  return stack_->count();
}

void SidebarContentWidget::showContent() {
  show();  // QSplitter 自动恢复布局空间
}

void SidebarContentWidget::hideContent() {
  hide();  // QSplitter 自动缩至 0 宽
}

bool SidebarContentWidget::isContentVisible() const {
  return !isHidden();
}

HardwareTreeWidget* SidebarContentWidget::hardwareTree() const {
  return hardware_tree_;
}

ProtocolManagerWidget* SidebarContentWidget::protocolManager() const {
  return protocol_manager_;
}

SearchWidget* SidebarContentWidget::searchWidget() const {
  return search_widget_;
}

GitWidget* SidebarContentWidget::gitWidget() const {
  return git_widget_;
}

TestProgramManagerWidget* SidebarContentWidget::testProgramManager() const {
  return test_program_manager_;
}

TopologyManagerWidget* SidebarContentWidget::topologyManager() const {
  return topology_manager_;
}

}  // namespace etest::app
