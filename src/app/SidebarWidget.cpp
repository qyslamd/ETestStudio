#include "SidebarWidget.h"
#include "FileExplorerWidget.h"
#include "GitWidget.h"
#include "HardwareTreeWidget.h"
#include "SearchWidget.h"

#include <QHBoxLayout>
#include <QIcon>

namespace etest::app {

SidebarWidget::SidebarWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void SidebarWidget::setupUi() {
  auto* outer_layout = new QHBoxLayout(this);
  outer_layout->setContentsMargins(0, 0, 0, 0);
  outer_layout->setSpacing(0);

  // ===== 左侧：活动按钮栏 =====
  auto* activity_panel = new QWidget(this);
  activity_panel->setFixedWidth(48);
  activity_panel->setObjectName(QStringLiteral("sidebarActivityBar"));
  auto* activity_layout = new QVBoxLayout(activity_panel);
  activity_layout->setContentsMargins(0, 4, 0, 4);
  activity_layout->setSpacing(0);

  auto* top_layout = new QVBoxLayout();
  top_layout->setSpacing(4);
  top_layout->setContentsMargins(0, 0, 0, 0);

  // 索引0：资源管理器
  buttons_.append(createButton(QStringLiteral("资源管理器"),
                                ":/resources/icons/svg/project_dark.svg",
                                ":/resources/icons/svg/project_light.svg"));
  // 索引1：搜索
  buttons_.append(createButton(QStringLiteral("搜索"),
                                ":/resources/icons/svg/search_dark.svg",
                                ":/resources/icons/svg/search_light.svg"));
  // 索引2：源代码管理
  buttons_.append(createButton(QStringLiteral("源代码管理"),
                                ":/resources/icons/svg/git_dark.svg",
                                ":/resources/icons/svg/git_light.svg"));
  // 索引3：调试
  buttons_.append(createButton(QStringLiteral("调试"),
                                ":/resources/icons/svg/debug_dark.svg",
                                ":/resources/icons/svg/debug_dark.svg"));
  // 索引4：扩展
  buttons_.append(createButton(QStringLiteral("扩展"),
                                ":/resources/icons/svg/extensions_dark.svg",
                                ":/resources/icons/svg/extensions_light.svg"));
  // 索引5：硬件
  buttons_.append(createButton(QStringLiteral("硬件"),
                                ":/resources/icons/svg/hardware_dark.svg",
                                ":/resources/icons/svg/hardware_light.svg"));

  for (int i = 0; i < buttons_.size(); ++i) {
    top_layout->addWidget(buttons_[i]);
    connect(buttons_[i], &QPushButton::clicked, this, [this, i]() {
      if (active_index_ == i) {
        emit sidebarToggleRequested();
      } else {
        setActiveIndex(i);
        switchPage(i);
      }
    });
  }

  activity_layout->addLayout(top_layout);
  activity_layout->addStretch();

  // 底部设置按钮
  auto* bottom_layout = new QVBoxLayout();
  bottom_layout->setSpacing(0);
  bottom_layout->setContentsMargins(0, 0, 0, 0);
  auto* settings_btn = createButton(QStringLiteral("设置"),
                                     ":/resources/icons/svg/settings_dark.svg",
                                     ":/resources/icons/svg/settings_light.svg");
  bottom_layout->addWidget(settings_btn);
  connect(settings_btn, &QPushButton::clicked, this, &SidebarWidget::settingsTriggered);
  activity_layout->addLayout(bottom_layout);

  outer_layout->addWidget(activity_panel);

  // ===== 右侧：内容面板 =====
  auto* content_panel = new QWidget(this);
  auto* content_layout = new QVBoxLayout(content_panel);
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

  content_layout->addWidget(stack_);

  outer_layout->addWidget(content_panel);

  setMinimumWidth(200);
  setActiveIndex(0);
  switchPage(0);
}

QPushButton* SidebarWidget::createButton(const QString& tooltip,
                                          const QString& darkIconPath,
                                          const QString& lightIconPath) {
  auto* btn = new QPushButton(this);
  btn->setToolTip(tooltip);
  btn->setFixedSize(48, 40);
  btn->setCheckable(true);
  btn->setFlat(true);
  btn->setFocusPolicy(Qt::NoFocus);

  QIcon icon;
  icon.addFile(darkIconPath, QSize(), QIcon::Normal, QIcon::Off);
  icon.addFile(lightIconPath, QSize(), QIcon::Disabled, QIcon::Off);
  btn->setIcon(icon);
  btn->setIconSize(QSize(24, 24));
  btn->setText(QString());

  return btn;
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

void SidebarWidget::setActiveIndex(int index) {
  if (index < 0 || index >= buttons_.size()) return;
  active_index_ = index;
  for (int i = 0; i < buttons_.size(); ++i) {
    buttons_[i]->setChecked(i == index);
  }
}

int SidebarWidget::activeIndex() const {
  return active_index_;
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

GitWidget* SidebarWidget::gitWidget() const {
  return git_widget_;
}

}  // namespace etest::app
