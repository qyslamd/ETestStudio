#include "SignalTreePanel.h"

#include <QLineEdit>
#include <QPair>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

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

  // 树
  tree_ = new QTreeWidget(this);
  tree_->setObjectName(QStringLiteral("SignalTree"));
  tree_->setHeaderHidden(true);
  tree_->setFrameShape(QFrame::NoFrame);
  tree_->setRootIsDecorated(true);
  tree_->setAnimated(true);
  layout->addWidget(tree_, 1);

  // checkbox 变化 → 发射 checkStateChanged 信号
  connect(tree_, &QTreeWidget::itemChanged, this,
          [this](QTreeWidgetItem* item, int column) {
            if (column != 0) {
              return;
            }
            if (!item || item->parent() == nullptr) {
              // 只处理叶节点（有 parent 的通道节点）
              return;
            }
            int key = item->data(0, Qt::UserRole).toInt();
            int monitorIndex = key >> 16;
            int channelIndex = key & 0xFFFF;
            bool checked = (item->checkState(0) == Qt::Checked);
            emit checkStateChanged(monitorIndex, channelIndex, checked);
          });
}

// ══════════════════════════════════════════════════════════════════════════════
// setMonitorTree — 设置树数据，构建界面
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::setMonitorTree(
    const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
    const QList<QPair<int, int>>& preCheckedChannels) {
  tree_data_ = tree;
  buildTree(tree, preCheckedChannels);
}

// ══════════════════════════════════════════════════════════════════════════════
// buildTree — 按监听器→通道构建两级 QTreeWidget
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::buildTree(
    const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
    const QList<QPair<int, int>>& preCheckedChannels) {
  tree_->clear();
  node_map_.clear();

  // 阻止重建过程中的 itemChanged 信号（checkState 设勾选不会误触 checkStateChanged）
  QSignalBlocker blocker(tree_);

  // 构建 preChecked 集合用于 O(n) 查找
  QSet<QPair<int, int>> preSet;
  for (const auto& ch : preCheckedChannels) {
    preSet.insert(ch);
  }

  for (const auto& entry : tree) {
    // 顶级：监听器名称
    auto* topItem = new QTreeWidgetItem(tree_);
    QString topText = entry.name;
    if (!entry.deviceType.isEmpty()) {
      topText += QStringLiteral(" [%1]").arg(entry.deviceType);
    }
    topItem->setText(0, topText);
    topItem->setFlags(topItem->flags() & ~Qt::ItemIsUserCheckable);
    topItem->setExpanded(true);

    // 二级：通道
    for (int ci = 0; ci < entry.channelCount; ++ci) {
      auto* childItem = new QTreeWidgetItem(topItem);
      childItem->setText(0, QStringLiteral("ch%1").arg(ci));
      childItem->setFlags(childItem->flags() | Qt::ItemIsUserCheckable);
      childItem->setCheckState(0, Qt::Unchecked);

      // 存储 (monitorIndex, channelIndex) 到 UserRole
      int key = (entry.monitorIndex << 16) | ci;
      childItem->setData(0, Qt::UserRole, key);

      // 如果该通道在 preChecked 集合中，恢复勾选
      if (preSet.contains(qMakePair(entry.monitorIndex, ci))) {
        childItem->setCheckState(0, Qt::Checked);
      }

      node_map_.insert(key, childItem);
    }
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// updateNodeValue — 更新某个通道的实时值缩略文本
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::updateNodeValue(int monitorIndex, int channelIndex,
                                       const QString& valueText) {
  int key = (monitorIndex << 16) | channelIndex;
  auto it = node_map_.constFind(key);
  if (it == node_map_.constEnd()) {
    return;
  }

  // 在名称后追加缩略值（如 "ch0  5.02V"）
  QTreeWidgetItem* item = it.value();
  QString baseText = QStringLiteral("ch%1").arg(channelIndex);
  if (!valueText.isEmpty()) {
    item->setText(0, QStringLiteral("%1  %2").arg(baseText, valueText));
  } else {
    item->setText(0, baseText);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// clearTree — 清空树和所有勾选（项目关闭时使用）
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::clearTree() {
  tree_->clear();
  node_map_.clear();
}

// ══════════════════════════════════════════════════════════════════════════════
// uncheckChannel — 取消勾选某个通道（可视化区右键关闭时同步）
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::uncheckChannel(int monitorIndex, int channelIndex) {
  int key = (monitorIndex << 16) | channelIndex;
  auto it = node_map_.constFind(key);
  if (it == node_map_.constEnd()) {
    return;
  }
  it.value()->setCheckState(0, Qt::Unchecked);
}

// ══════════════════════════════════════════════════════════════════════════════
// onFilterChanged — 搜索框文本变化时过滤树节点
// ══════════════════════════════════════════════════════════════════════════════

void SignalTreePanel::onFilterChanged(const QString& text) {
  if (text.isEmpty()) {
    // 恢复所有
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
      tree_->topLevelItem(i)->setHidden(false);
      for (int j = 0; j < tree_->topLevelItem(i)->childCount(); ++j) {
        tree_->topLevelItem(i)->child(j)->setHidden(false);
      }
    }
    return;
  }

  QString filter = text.trimmed().toLower();
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* topItem = tree_->topLevelItem(i);
    bool topMatch = topItem->text(0).toLower().contains(filter);

    // 先隐藏所有子节点
    bool hasVisibleChild = false;
    for (int j = 0; j < topItem->childCount(); ++j) {
      auto* child = topItem->child(j);
      bool childMatch = child->text(0).toLower().contains(filter);
      child->setHidden(!childMatch);
      if (childMatch) {
        hasVisibleChild = true;
      }
    }

    // 顶级节点：匹配则显示所有子，不匹配且有匹配子则显示
    if (topMatch) {
      topItem->setHidden(false);
      for (int j = 0; j < topItem->childCount(); ++j) {
        topItem->child(j)->setHidden(false);
      }
    } else {
      topItem->setHidden(!hasVisibleChild);
    }
  }
}

}  // namespace etest::app
