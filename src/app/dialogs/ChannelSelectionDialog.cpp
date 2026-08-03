#include "ChannelSelectionDialog.h"

#include <QVBoxLayout>

#include "SignalTreePanel.h"

namespace etest::app {

ChannelSelectionDialog::ChannelSelectionDialog(QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(QStringLiteral("通道选择"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  resize(380, 480);
  initUi();
}

void ChannelSelectionDialog::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  tree_panel_ = new SignalTreePanel(this);
  connect(tree_panel_, &SignalTreePanel::checkStateChanged, this,
          &ChannelSelectionDialog::checkStateChanged);
  layout->addWidget(tree_panel_);
}

void ChannelSelectionDialog::setMonitorTree(
    const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
    const QList<int>& preCheckedMonitors) {
  tree_panel_->setMonitorTree(tree, preCheckedMonitors);
}

void ChannelSelectionDialog::uncheckMonitor(int monitorIndex) {
  tree_panel_->uncheckMonitor(monitorIndex);
}

void ChannelSelectionDialog::clearTree() { tree_panel_->clearTree(); }

}  // namespace etest::app