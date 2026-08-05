#include "VisualizationArea.h"

#include <algorithm>
#include <cmath>

#include <QContextMenuEvent>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include "visualizers/SignalVisualizer.h"
#include "visualizers/VisualizerProxy.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造 / 析构
// ══════════════════════════════════════════════════════════════════════════════

VisualizationArea::VisualizationArea(QWidget* parent) : QGraphicsView(parent) {
  scene_ = new QGraphicsScene(this);
  setScene(scene_);

  setObjectName(QStringLiteral("VisualizationArea"));
  setFrameShape(QFrame::NoFrame);
  setRenderHint(QPainter::Antialiasing, true);
  setDragMode(QGraphicsView::NoDrag);
  setInteractive(true);
  // 大画布浏览：固定 sceneRect + 滚动条按需，支持缩放/平移
  scene_->setSceneRect(0, 0, 4000, 3000);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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

  // 创建 VisualizerProxy 包裹 widget（编辑态可拖拽/resize）
  auto* proxy = new VisualizerProxy;
  proxy->setWidget(visualizer);
  proxy->setCacheMode(QGraphicsItem::NoCache);
  proxy->setEditMode(edit_mode_);
  // 用户拖拽/resize 结束 → 汇总为 layoutChanged，供宿主置脏
  connect(proxy, &VisualizerProxy::geometryEdited, this,
          &VisualizationArea::layoutChanged);
  scene_->addItem(proxy);

  Item item;
  item.proxy = proxy;
  item.widget = visualizer;
  items_.insert(connectionId, item);

  if (!edit_mode_) {
    relayout();
  }
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
  delete it->proxy;  // QGraphicsProxyWidget 会连带删除其 widget
  items_.erase(it);

  // 编辑态手动布局，删除单卡不重排其余
  if (!edit_mode_) {
    relayout();
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// visualizer — 按索引查找
// ══════════════════════════════════════════════════════════════════════════════

SignalVisualizer* VisualizationArea::visualizer(
    const QString& connectionId) const {
  auto it = items_.constFind(connectionId);
  if (it != items_.constEnd()) {
    return it->widget;
  }
  return nullptr;
}

// 编辑/展示两态切换：编辑态可拖拽/resize/排列，展示态只读并恢复自动网格
void VisualizationArea::setEditMode(bool edit) {
  if (edit_mode_ == edit) {
    return;
  }
  edit_mode_ = edit;
  for (auto it = items_.constBegin(); it != items_.constEnd(); ++it) {
    if (auto* vp = qgraphicsitem_cast<VisualizerProxy*>(it->proxy)) {
      vp->setEditMode(edit);
    }
  }
  if (!edit_mode_) {
    relayout();  // 切回展示态恢复自动网格
  }
}

// 布局收集：返回各卡片 scene 坐标（位置 = proxy pos，尺寸 = widget size，不含手柄边距）
QVector<VisualizationArea::VisualizerGeometry>
VisualizationArea::visualizerGeometries() const {
  QVector<VisualizerGeometry> result;
  for (auto it = items_.constBegin(); it != items_.constEnd(); ++it) {
    VisualizerGeometry g;
    g.connectionId = it.key();
    g.rect = QRectF(it->proxy->pos(), it->proxy->widget()->size());
    result.append(g);
  }
  return result;
}

// 布局应用：按 scene 坐标设置卡片位置/大小
void VisualizationArea::setVisualizerGeometry(const QString& connectionId,
                                               const QRectF& rect) {
  auto it = items_.find(connectionId);
  if (it == items_.end()) {
    return;
  }
  it->proxy->setPos(rect.topLeft());
  if (auto* w = it->proxy->widget()) {
    w->resize(rect.size().toSize());
  }
}

// 选中 VisualizerProxy 数量（排列/分布门控）
int VisualizationArea::selectedVisualizerCount() const {
  int count = 0;
  for (auto* item : scene_->selectedItems()) {
    if (qgraphicsitem_cast<VisualizerProxy*>(item)) {
      ++count;
    }
  }
  return count;
}

// 排列：按类型对选中卡片对齐（移植拓扑 doAlign）
// 几何用 visualRect()（widget 实际矩形），不含 resize 手柄外扩边距
void VisualizationArea::alignVisualizers(AlignType type) {
  QVector<VisualizerProxy*> items;
  for (auto* item : scene_->selectedItems()) {
    if (auto* vp = qgraphicsitem_cast<VisualizerProxy*>(item)) {
      items.append(vp);
    }
  }
  if (items.size() < 2) {
    return;
  }
  QRectF total = items[0]->visualRect();
  for (int i = 1; i < items.size(); ++i) {
    total = total.united(items[i]->visualRect());
  }
  for (auto* item : items) {
    const QRectF r = item->visualRect();
    qreal dx = 0, dy = 0;
    switch (type) {
      case AlignType::Left:
        dx = total.left() - r.left();
        break;
      case AlignType::HCenter:
        dx = total.center().x() - r.center().x();
        break;
      case AlignType::Right:
        dx = total.right() - r.right();
        break;
      case AlignType::Top:
        dy = total.top() - r.top();
        break;
      case AlignType::VCenter:
        dy = total.center().y() - r.center().y();
        break;
      case AlignType::Bottom:
        dy = total.bottom() - r.bottom();
        break;
    }
    if (dx != 0 || dy != 0) {
      item->moveBy(dx, dy);
    }
  }
  emit layoutChanged();  // 程序性排列也置脏
}

// 分布：按左/上边排序等距分布（移植拓扑 doDistribute）
// 几何用 visualRect()（widget 实际矩形），不含 resize 手柄外扩边距
void VisualizationArea::distributeVisualizers(DistributeType type) {
  QVector<VisualizerProxy*> items;
  for (auto* item : scene_->selectedItems()) {
    if (auto* vp = qgraphicsitem_cast<VisualizerProxy*>(item)) {
      items.append(vp);
    }
  }
  if (items.size() < 2) {
    return;
  }

  if (type == DistributeType::Horizontal) {
    std::sort(items.begin(), items.end(),
              [](VisualizerProxy* a, VisualizerProxy* b) {
                return a->visualRect().left() < b->visualRect().left();
              });
    const qreal first = items[0]->visualRect().left();
    const qreal last = items.last()->visualRect().right();
    qreal used = 0;
    for (const auto* it : items) {
      used += it->visualRect().width();
    }
    const qreal gap = (last - first - used) / (items.size() - 1);
    qreal x = first;
    for (auto* item : items) {
      item->setPos(x, item->pos().y());
      x += item->visualRect().width() + gap;
    }
  } else {
    std::sort(items.begin(), items.end(),
              [](VisualizerProxy* a, VisualizerProxy* b) {
                return a->visualRect().top() < b->visualRect().top();
              });
    const qreal first = items[0]->visualRect().top();
    const qreal last = items.last()->visualRect().bottom();
    qreal used = 0;
    for (const auto* it : items) {
      used += it->visualRect().height();
    }
    const qreal gap = (last - first - used) / (items.size() - 1);
    qreal y = first;
    for (auto* item : items) {
      item->setPos(item->pos().x(), y);
      y += item->visualRect().height() + gap;
    }
  }
  emit layoutChanged();  // 程序性分布也置脏
}

// ══════════════════════════════════════════════════════════════════════════════
// clearAll — 清空所有
// ══════════════════════════════════════════════════════════════════════════════

void VisualizationArea::clearAll() {
  // 先复制一份 key 列表，避免迭代中修改容器
  // 注意：emit visualizerClosed 会同步触发 removeVisualizer（通过
  // uncheckChannel 回调链）， 该调用会 delete proxy 并 erase items_。emit 后
  // it/iter 可能已失效，必须重新查找。
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
  // 编辑态手动布局，不随窗口尺寸重排
  if (!edit_mode_) {
    relayout();
  }
}

// 视图浏览：Ctrl+滚轮缩放，否则默认滚动（参考 TopologyView）
void VisualizationArea::wheelEvent(QWheelEvent* event) {
  if (event->modifiers() & Qt::ControlModifier) {
    qreal factor = event->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    // 缩放钳制 [0.25, 4.0]，避免缩到不可恢复
    const qreal current = transform().m11();
    if (current * factor > 4.0) {
      factor = 4.0 / current;
    } else if (current * factor < 0.25) {
      factor = 0.25 / current;
    }
    scale(factor, factor);
    event->accept();
    return;
  }
  QGraphicsView::wheelEvent(event);
}

// 中键平移（参考 TopologyView panning_）
void VisualizationArea::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton) {
    panning_ = true;
    last_pan_point_ = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  QGraphicsView::mousePressEvent(event);
}

void VisualizationArea::mouseMoveEvent(QMouseEvent* event) {
  if (panning_) {
    const QPoint delta = event->pos() - last_pan_point_;
    last_pan_point_ = event->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    event->accept();
    return;
  }
  QGraphicsView::mouseMoveEvent(event);
}

void VisualizationArea::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton && panning_) {
    panning_ = false;
    setCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }
  QGraphicsView::mouseReleaseEvent(event);
}

// ══════════════════════════════════════════════════════════════════════════════
// contextMenuEvent — 右键菜单（关闭等）
// ══════════════════════════════════════════════════════════════════════════════

void VisualizationArea::contextMenuEvent(QContextMenuEvent* event) {
  // 命中即卡片：VisualizerProxy 覆写了 type()，必须用 qgraphicsitem_cast 匹配
  //（其内部比较 item->type() == VisualizerProxy::Type，基类 QGraphicsProxyWidget::Type 对不上）
  auto* proxy = qgraphicsitem_cast<VisualizerProxy*>(itemAt(event->pos()));
  if (!proxy) {
    return;
  }

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
    // 先移除再发信号：visualizerClosed 槽会按 activeChannels()
    // 同步对话框勾选态，若先发信号该通道还在列表里，勾选不会取消。
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
    cols =
        qMin(static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n)))), 5);
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
