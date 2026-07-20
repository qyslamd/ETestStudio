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

void VisualizationArea::addVisualizer(int monitorIndex, int channelIndex,
                                       SignalVisualizer* visualizer) {
  if (!visualizer) {
    return;
  }

  int key = (monitorIndex << 16) | channelIndex;

  // 已存在则不重复添加
  if (items_.contains(key)) {
    return;
  }

  // 创建 proxy 包裹 widget
  auto* proxy = scene_->addWidget(visualizer);
  proxy->setCacheMode(QGraphicsItem::NoCache);

  Item item;
  item.proxy = proxy;
  item.widget = visualizer;
  items_.insert(key, item);

  relayout();
}

// ══════════════════════════════════════════════════════════════════════════════
// removeVisualizer — 移除某个可视化组件
// ══════════════════════════════════════════════════════════════════════════════

void VisualizationArea::removeVisualizer(int monitorIndex, int channelIndex) {
  int key = (monitorIndex << 16) | channelIndex;
  auto it = items_.find(key);
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

SignalVisualizer* VisualizationArea::visualizer(int monitorIndex,
                                                 int channelIndex) const {
  int key = (monitorIndex << 16) | channelIndex;
  auto it = items_.constFind(key);
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
  auto keys = items_.keys();
  for (int key : keys) {
    auto it = items_.find(key);
    if (it == items_.end()) {
      continue;
    }
    // 发射关闭信号
    int monitorIndex = key >> 16;
    int channelIndex = key & 0xFFFF;
    emit visualizerClosed(monitorIndex, channelIndex);

    scene_->removeItem(it->proxy);
    delete it->proxy;
  }
  items_.clear();
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
  int foundKey = -1;
  for (auto it = items_.constBegin(); it != items_.constEnd(); ++it) {
    if (it->proxy == proxy) {
      foundKey = it.key();
      break;
    }
  }
  if (foundKey < 0) {
    return;
  }

  int monitorIndex = foundKey >> 16;
  int channelIndex = foundKey & 0xFFFF;

  QMenu menu(this);
  QAction* closeAction = menu.addAction(QStringLiteral("关闭可视化"));
  if (menu.exec(event->globalPos()) == closeAction) {
    emit visualizerClosed(monitorIndex, channelIndex);
    removeVisualizer(monitorIndex, channelIndex);
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
