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

  // ── 底部运行输出面板 ──
  output_panel_ = new ExecutionOutputPanel(this);
  output_panel_->setObjectName(QStringLiteral("ExecOutputPanel"));
  layout->addWidget(output_panel_);
}

}  // namespace etest::app
