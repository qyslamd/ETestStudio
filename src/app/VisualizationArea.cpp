#include "VisualizationArea.h"

#include <cmath>

#include <QContextMenuEvent>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QMenu>

#include "visualizers/SignalVisualizer.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造 / 析构
// ══════════════════════════════════════════════════════════════════════════════

VisualizationArea::VisualizationArea(QWidget* parent)
    : QGraphicsView(parent) {
  scene_ = new QGraphicsScene(this);
  setScene(scene_);

  setObjectName(QStringLiteral("VisualizationArea"));
  setFrameShape(QFrame::NoFrame);
  setRenderHint(QPainter::Antialiasing, true);
  setDragMode(QGraphicsView::NoDrag);
  setInteractive(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  // 背景透明，由 QSS 统一控制
  setBackgroundBrush(Qt::NoBrush);
}

VisualizationArea::~VisualizationArea() {
  clearAll();
}

// ══════════════════════════════════════════════════════════════════════════════
// addVisualizer — 添加可视化组件到网格
// ══════════════════════════════════════════════════════════════════════════════

void VisualizationArea::addVisualizer(const QString& connectionId,
                                       SignalVisualizer* visualizer) {
  if (!visualizer) {
    return;
  }

  // 已存在则不重复添加
  if (items_.contains(connectionId)) {
    return;
  }

  // 创建 proxy 包裹 widget
  auto* proxy = scene_->addWidget(visualizer);
  proxy->setCacheMode(QGraphicsItem::NoCache);

  Item item;
  item.proxy = proxy;
  item.widget = visualizer;
  items_.insert(connectionId, item);

  relayout();
}

// ══════════════════════════════════════════════════════════════════════════════
// removeVisualizer — 移除某个可视化组件
// ══════════════════════════════════════════════════════════════════════════════

void VisualizationArea::removeVisualizer(const QString& connectionId) {
  auto it = items_.find(connectionId);
  if (it == items_.end()) {
    return;
  }

  // 从 scene 移除 proxy
  scene_->removeItem(it->proxy);
  delete it->proxy;   // QGraphicsProxyWidget 会连带删除其 widget
  items_.erase(it);

  relayout();
}

// ══════════════════════════════════════════════════════════════════════════════
// visualizer — 按索引查找
// ══════════════════════════════════════════════════════════════════════════════

SignalVisualizer* VisualizationArea::visualizer(const QString& connectionId) const {
  auto it = items_.constFind(connectionId);
  if (it != items_.constEnd()) {
    return it->widget;
  }
  return nullptr;
}

// ══════════════════════════════════════════════════════════════════════════════
// clearAll — 清空所有
// ══════════════════════════════════════════════════════════════════════════════

void VisualizationArea::clearAll() {
  // 先复制一份 key 列表，避免迭代中修改容器
  // 注意：emit visualizerClosed 会同步触发 removeVisualizer（通过 uncheckChannel 回调链），
  // 该调用会 delete proxy 并 erase items_。emit 后 it/iter 可能已失效，必须重新查找。
  auto keys = items_.keys();
  for (const QString& key : keys) {
    emit visualizerClosed(key);

    // emit 后该条目可能已被 removeVisualizer 删除，重新查找确认
    auto it = items_.find(key);
    if (it != items_.end()) {
      scene_->removeItem(it->proxy);
      delete it->proxy;
    }
  }
  items_.clear();
}

// ══════════════════════════════════════════════════════════════════════════════
// activeChannels — 当前活跃通道列表（供 syncProjectTopologies 恢复订阅使用）
// ══════════════════════════════════════════════════════════════════════════════
QList<QString> VisualizationArea::activeChannels() const {
  return items_.keys();
}

// ══════════════════════════════════════════════════════════════════════════════
// resizeEvent — 视图大小变化时重排
// ══════════════════════════════════════════════════════════════════════════════

void VisualizationArea::resizeEvent(QResizeEvent* event) {
  QGraphicsView::resizeEvent(event);
  relayout();
}

// ══════════════════════════════════════════════════════════════════════════════
// contextMenuEvent — 右键菜单（关闭等）
// ══════════════════════════════════════════════════════════════════════════════

void VisualizationArea::contextMenuEvent(QContextMenuEvent* event) {
  // 获取点击位置下的 item
  QGraphicsItem* item = itemAt(event->pos());
  if (!item) {
    return;
  }

  // 向上找 QGraphicsProxyWidget
  while (item && item->type() != QGraphicsProxyWidget::Type) {
    item = item->parentItem();
  }
  if (!item) {
    return;
  }

  auto* proxy = static_cast<QGraphicsProxyWidget*>(item);
  auto* visWidget = qobject_cast<SignalVisualizer*>(proxy->widget());
  if (!visWidget) {
    return;
  }

  // 在 items_ 中查找对应的 key
  QString foundKey;
  for (auto it = items_.constBegin(); it != items_.constEnd(); ++it) {
    if (it->proxy == proxy) {
      foundKey = it.key();
      break;
    }
  }
  if (foundKey.isEmpty()) {
    return;
  }

  QMenu menu(this);
  QAction* closeAction = menu.addAction(QStringLiteral("关闭可视化"));
  if (menu.exec(event->globalPos()) == closeAction) {
    // 先移除再发信号：visualizerClosed 槽会按 activeChannels() 同步对话框勾选态，
    // 若先发信号该通道还在列表里，勾选不会取消（审查 🟡1）。
    removeVisualizer(foundKey);
    emit visualizerClosed(foundKey);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// relayout — 自动网格重排
// ══════════════════════════════════════════════════════════════════════════════

void VisualizationArea::relayout() {
  if (items_.isEmpty()) {
    return;
  }

  int n = items_.size();
  int cols;

  // ── 网格列数计算 ──
  if (n == 1) {
    cols = 1;
  } else if (n <= 4) {
    cols = 2;
  } else {
    // ceil(sqrt(n))，上限 5 列
    cols = qMin(static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n)))), 5);
  }

  int rows = (n + cols - 1) / cols;

  qreal totalW = viewport()->width();
  qreal totalH = viewport()->height();

  // 避免除零
  if (totalW < 1 || totalH < 1) {
    return;
  }

  qreal cellW = totalW / cols;
  qreal cellH = totalH / rows;
  qreal marginX = cellW * 0.02;
  qreal marginY = cellH * 0.02;
  qreal widgetW = cellW - 2.0 * marginX;
  qreal widgetH = cellH - 2.0 * marginY;

  // 确保最小尺寸
  if (widgetW < 40) {
    widgetW = 40;
  }
  if (widgetH < 30) {
    widgetH = 30;
  }

  int idx = 0;
  // 按插入顺序排列（order_ 列表）
  for (auto it = items_.constBegin(); it != items_.constEnd(); ++it, ++idx) {
    int row = idx / cols;
    int col = idx % cols;

    qreal x = col * cellW + marginX;
    qreal y = row * cellH + marginY;

    it->proxy->setPos(x, y);
    it->proxy->resize(widgetW, widgetH);
    it->proxy->setMinimumSize(40, 30);
  }
}

}  // namespace etest::app
