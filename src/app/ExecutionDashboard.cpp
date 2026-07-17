#include "ExecutionDashboard.h"

#include <QSplitter>
#include <QVBoxLayout>

#include "RunStatusPanel.h"
#include "SignalTreePanel.h"
#include "VisualizationArea.h"
#include "widgets/ExecutionOutputPanel.h"

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
  layout->setSpacing(1);

  // ── 水平三列（主 splitter） ──
  main_splitter_ = new QSplitter(Qt::Horizontal, this);
  main_splitter_->setObjectName(QStringLiteral("ExecDashboardSplitter"));

  run_status_ = new RunStatusPanel(main_splitter_);
  run_status_->setObjectName(QStringLiteral("ExecRunStatus"));

  signal_tree_ = new SignalTreePanel(main_splitter_);
  signal_tree_->setObjectName(QStringLiteral("ExecSignalTree"));

  vis_area_ = new VisualizationArea(main_splitter_);
  vis_area_->setObjectName(QStringLiteral("ExecVisArea"));

  main_splitter_->addWidget(run_status_);
  main_splitter_->addWidget(signal_tree_);
  main_splitter_->addWidget(vis_area_);
  main_splitter_->setSizes({200, 250, 600});

  layout->addWidget(main_splitter_, 1);

  // ── 底部输出面板占位（Task 11 由 MainWindow 通过 setOutputPanel 注入） ──
  // output_panel_ 初始为 nullptr，由外部注入
}

// ══════════════════════════════════════════════════════════════════════════════
// setOutputPanel — 由 MainWindow 注入 ExecutionOutputPanel 实例
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionDashboard::setOutputPanel(ExecutionOutputPanel* panel) {
  if (output_panel_ == panel) {
    return;
  }

  // 移除旧的 output panel
  if (output_panel_) {
    layout()->removeWidget(output_panel_);
    // 不 delete — setParent 会转移所有权
  }

  output_panel_ = panel;
  if (output_panel_) {
    output_panel_->setParent(this);
    layout()->addWidget(output_panel_);
    output_panel_->show();
  }
}

}  // namespace etest::app
