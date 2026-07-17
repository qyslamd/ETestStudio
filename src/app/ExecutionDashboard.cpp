#include "ExecutionDashboard.h"

#include <QSplitter>
#include <QVBoxLayout>

#include "ExecutionDebugWidget.h"
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
  layout->setSpacing(0);

  // ── 垂直 splitter：上方三列 + 下方输出面板 ──
  vert_splitter_ = new QSplitter(Qt::Vertical, this);
  vert_splitter_->setObjectName(QStringLiteral("ExecVertSplitter"));

  // ── 水平三列（主 splitter） ──
  main_splitter_ = new QSplitter(Qt::Horizontal, vert_splitter_);
  main_splitter_->setObjectName(QStringLiteral("ExecDashboardSplitter"));

  debug_widget_ = new ExecutionDebugWidget(main_splitter_);
  debug_widget_->setObjectName(QStringLiteral("ExecRunStatus"));

  signal_tree_ = new SignalTreePanel(main_splitter_);
  signal_tree_->setObjectName(QStringLiteral("ExecSignalTree"));

  vis_area_ = new VisualizationArea(main_splitter_);
  vis_area_->setObjectName(QStringLiteral("ExecVisArea"));

  main_splitter_->addWidget(debug_widget_);
  main_splitter_->addWidget(signal_tree_);
  main_splitter_->addWidget(vis_area_);
  main_splitter_->setSizes({200, 250, 600});

  vert_splitter_->addWidget(main_splitter_);

  // ── 底部输出面板占位（由 MainWindow 通过 setOutputPanel 注入） ──
  // output_panel_ 初始为 nullptr，注入后会被加到 vert_splitter_

  vert_splitter_->setStretchFactor(0, 1);
  vert_splitter_->setSizes({700, 200});

  layout->addWidget(vert_splitter_);
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
    output_panel_->setParent(nullptr);
    output_panel_->deleteLater();
  }

  output_panel_ = panel;
  if (output_panel_) {
    vert_splitter_->addWidget(output_panel_);
    vert_splitter_->setSizes({700, 200});
    output_panel_->show();
  }
}

}  // namespace etest::app
