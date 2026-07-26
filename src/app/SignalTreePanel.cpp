#include "SignalTreePanel.h"

#include <QLineEdit>
#include <QSet>
#include <QListWidget>
#include <QVBoxLayout>

#include "logger/Logger.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造 / UI 初始化
// ══════════════════════════════════════════════════════════════════════════════

SignalTreePanel::SignalTreePanel(QWidget* parent)
    : QWidget(parent) {
  initUi();
}

void SignalTreePanel::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  // 搜索框
  search_box_ = new QLineEdit(this);
  search_box_->setObjectName(QStringLiteral("SignalTreeSearch"));
  search_box_->setPlaceholderText(QStringLiteral("搜索通道..."));
  search_box_->setClearButtonEnabled(true);
  layout->addWidget(search_box_);

  connect(search_box_, &QLineEdit::textChanged,
          this, &SignalTreePanel::onFilterChanged);

  // 扁平列表
  list_ = new QListWidget(this);
  list_->setObjectName(QStringLiteral("SignalTree"));
  list_->setFrameShape(QFrame::NoFrame);
  layout->addWidget(list_, 1);

  // checkbox 变化 → 发射 checkStateChanged 信号
  connect(list_, &QListWidget::itemChanged, this,
          [this](QListWidgetItem* item) {
    if (!item) return;
    int monitorIndex = item->data(Qt::UserRole).toInt();
    bool checked = (item->checkState() == Qt::Checked);
    LOG_DEBUG("VISUAL", "itemChanged -> monitorIndex={} checked={}",
              monitorIndex, checked);
    emit checkStateChanged(monitorIndex, checked);
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// setMonitorTree — 设置数据，构建界面
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::setMonitorTree(
    const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
    const QList<int>& preCheckedMonitors) {
  tree_data_ = tree;
  buildTree(tree, preCheckedMonitors);
}

// ══════════════════════════════════════════════════════════════════════════════
// buildTree — 按监听器列表构建扁平列表
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::buildTree(
    const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
    const QList<int>& preCheckedMonitors) {
  list_->clear();
  node_map_.clear();

  QSignalBlocker blocker(list_);

  QSet<int> preSet;
  for (int mi : preCheckedMonitors) {
    preSet.insert(mi);
  }

  for (const auto& entry : tree) {
    auto* item = new QListWidgetItem(list_);
    item->setText(entry.name);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(preSet.contains(entry.monitorIndex)
                             ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, entry.monitorIndex);
    node_map_.insert(entry.monitorIndex, item);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// updateNodeValue — 更新某个监听器的实时值缩略文本
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::updateNodeValue(int monitorIndex,
                                       const QString& valueText) {
  auto it = node_map_.constFind(monitorIndex);
  if (it == node_map_.constEnd()) {
    return;
  }

  QListWidgetItem* item = it.value();
  if (!valueText.isEmpty()) {
    QString prefix = tree_data_.value(monitorIndex).name;
    item->setText(QStringLiteral("%1  %2").arg(prefix, valueText));
  } else {
    item->setText(tree_data_.value(monitorIndex).name);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// uncheckMonitor — 取消勾选某个监听器（可视化区右键关闭时同步）
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::uncheckMonitor(int monitorIndex) {
  auto it = node_map_.constFind(monitorIndex);
  if (it == node_map_.constEnd()) {
    return;
  }
  it.value()->setCheckState(Qt::Unchecked);
}

// ══════════════════════════════════════════════════════════════════════════════
// clearTree — 清空（项目关闭时使用）
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::clearTree() {
  list_->clear();
  node_map_.clear();
}

// ══════════════════════════════════════════════════════════════════════════════
// onFilterChanged — 搜索框文本变化时过滤列表
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::onFilterChanged(const QString& text) {
  if (text.isEmpty()) {
    for (int i = 0; i < list_->count(); ++i) {
      list_->item(i)->setHidden(false);
    }
    return;
  }

  QString filter = text.trimmed().toLower();
  for (int i = 0; i < list_->count(); ++i) {
    auto* item = list_->item(i);
    item->setHidden(!item->text().toLower().contains(filter));
  }
}

}  // namespace etest::app
