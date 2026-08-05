#include "ExecutionDashboard.h"

#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include "ExecutionDebugWidget.h"
#include "VisualizationArea.h"
#include "libui/styles/TabBarStyle.h"
#include "widgets/ExecutionOutputPanel.h"
#include "widgets/ProblemsPanel.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造
// ══════════════════════════════════════════════════════════════════════════════

ExecutionDashboard::ExecutionDashboard(QWidget* parent)
    : QWidget(parent) {
  initUi();
}

void ExecutionDashboard::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // ── 垂直 splitter：上方两列 + 下方输出/问题 tab ──
  vert_splitter_ = new QSplitter(Qt::Vertical, this);
  vert_splitter_->setObjectName(QStringLiteral("ExecVertSplitter"));

  // ── 水平三列（主 splitter） ──
  main_splitter_ = new QSplitter(Qt::Horizontal, vert_splitter_);
  main_splitter_->setObjectName(QStringLiteral("ExecDashboardSplitter"));

  // objectName 由各组件构造函数自行设置，用于匹配 vscode.qss / default.qss 中的
  // #VisualizationArea / #ExecutionDebugWidget 等选择器；
  // 此处不可覆盖，否则 QSS 选择器失配（曾导致可视化区在暗色主题下不显示 #252526 背景）。
  // 通道选择树（SignalTreePanel）已迁至 ribbon「通道选择」Modal Dialog。
  vis_area_ = new VisualizationArea(main_splitter_);
  // 运行态「手动 + 只读」：按 .erun.layout 忠实还原编辑器布局，禁拖拽/resize（4.5）
  vis_area_->setManualLayout(true);
  vis_area_->setInteractive(false);

  debug_widget_ = new ExecutionDebugWidget(main_splitter_);

  main_splitter_->addWidget(vis_area_);
  main_splitter_->addWidget(debug_widget_);
  main_splitter_->setSizes({600, 200});

  vert_splitter_->addWidget(main_splitter_);

  // ── 底部输出/问题 tab ──
  bottom_tabs_ = new QTabWidget(vert_splitter_);
  bottom_tabs_->setObjectName(QStringLiteral("ExecBottomTabs"));
  bottom_tabs_->setTabPosition(QTabWidget::North);
  bottom_tabs_->setDocumentMode(true);
  bottom_tabs_->tabBar()->setElideMode(Qt::ElideRight);
  bottom_tabs_->tabBar()->setUsesScrollButtons(true);
  // 与 page0 BottomContainerWidget 统一 tab 外观（Chrome 风格圆角 tab）
  TabBarStyle::install(bottom_tabs_->tabBar());
  // 输出 tab：占位，setOutputPanel 注入后填入
  // 问题 tab：内部创建 ProblemsPanel
  problems_panel_ = new ProblemsPanel(bottom_tabs_);
  bottom_tabs_->addTab(problems_panel_, QStringLiteral("问题"));

  vert_splitter_->setStretchFactor(0, 1);
  vert_splitter_->setSizes({700, 200});

  layout->addWidget(vert_splitter_);
}

// ══════════════════════════════════════════════════════════════════════════════
// setOutputPanel - 由 MainWindow 注入 ExecutionOutputPanel 实例
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionDashboard::setOutputPanel(ExecutionOutputPanel* panel) {
  if (output_panel_ == panel) {
    return;
  }

  // 移除旧的 output panel（从 tab widget 中移除并删除）
  if (output_panel_) {
    int idx = bottom_tabs_->indexOf(output_panel_);
    if (idx >= 0) {
      bottom_tabs_->removeTab(idx);
    }
    output_panel_->deleteLater();
  }

  output_panel_ = panel;
  if (output_panel_) {
    // 输出 tab 插在 index 0，问题 tab 顺延到 index 1
    bottom_tabs_->insertTab(0, output_panel_, QStringLiteral("输出"));
    bottom_tabs_->setCurrentIndex(0);
    output_panel_->show();
  }
}

void ExecutionDashboard::setCurrentBottomTab(int index) {
  if (index >= 0 && index < bottom_tabs_->count()) {
    bottom_tabs_->setCurrentIndex(index);
  }
}

void ExecutionDashboard::showProblemsTab() {
  if (!problems_panel_) {
    return;
  }
  int idx = bottom_tabs_->indexOf(problems_panel_);
  if (idx >= 0) {
    bottom_tabs_->setCurrentIndex(idx);
  }
}

}  // namespace etest::app
