#include "DockAreaTabBarStyle.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QWidget>

#include "DockWidgetTab.h"
#include "ThemeManager.h"

// ── 构造 ──────────────────────────────────────────────────────

DockAreaTabBarStyle::DockAreaTabBarStyle(ads::CDockAreaWidget* parent,
                                         int tab_height)
    : ads::CDockAreaTabBar(parent) {
  dark_ = etest::core_ui::ThemeManager::instance().isDarkTheme();
  setFixedHeight(tab_height);

  // 获取 tabsContainerWidget 并安装 eventFilter
  // 1. 拦截 Paint 事件 -> 统一绘制所有 tab 形状
  // 2. 拦截 ChildAdded 事件 -> 对新插入的子 tab 安装 eventFilter
  tabs_container_ = findChild<QWidget*>("tabsContainerWidget");
  if (tabs_container_) {
    tabs_container_->installEventFilter(this);
  }

  // 对 viewport 也安装 eventFilter，填充背景色
  if (auto* vp = viewport()) {
    vp->installEventFilter(this);
    applyViewportBackground();
  }

  // 主题切换
  connect(&etest::core_ui::ThemeManager::instance(),
          &etest::core_ui::ThemeManager::themeChanged, this,
          [this](bool) { onThemeChanged(); });
}

// ── eventFilter ───────────────────────────────────────────────

bool DockAreaTabBarStyle::eventFilter(QObject* watched, QEvent* event) {
  // 拦截 viewport Paint 事件，填充背景色
  if (event->type() == QEvent::Paint) {
    auto* vp = viewport();
    if (watched == vp) {
      QPainter painter(vp);
      painter.fillRect(vp->rect(),
                       dark_ ? QColor("#252526") : QColor("#F0F0F0"));
      return true;
    }
  }

  // 拦截 tabsContainerWidget 的 Paint 事件，统一绘制所有 tab 形状
  if (watched == tabs_container_ && event->type() == QEvent::Paint) {
    QPainter painter(tabs_container_);
    painter.setRenderHint(QPainter::Antialiasing);
    // 先填充背景色，再画 tab 形状
    painter.fillRect(tabs_container_->rect(),
                     dark_ ? QColor("#252526") : QColor("#F0F0F0"));
    paintAllTabs(&painter);
    return true;  // 吃掉事件，阻止默认 paintEvent
  }

  // tabsContainerWidget 有新子 widget 加入时，对 CDockWidgetTab 安装 eventFilter
  if (watched == tabs_container_ && event->type() == QEvent::ChildAdded) {
    auto* child_event = static_cast<QChildEvent*>(event);
    auto* tab = qobject_cast<ads::CDockWidgetTab*>(child_event->child());
    if (tab) {
      tab->installEventFilter(this);
    }
  }

  // 拦截子 tab 的 Enter/Leave/StyleChange 事件，触发容器重绘
  if (event->type() == QEvent::Enter || event->type() == QEvent::Leave ||
      event->type() == QEvent::StyleChange) {
    auto* tab = qobject_cast<ads::CDockWidgetTab*>(watched);
    if (tab && tabs_container_) {
      tabs_container_->update();
    }
  }

  return ads::CDockAreaTabBar::eventFilter(watched, event);
}

// ── paintEvent -- 填充 scroll area 自身背景 ─────────────────

void DockAreaTabBarStyle::paintEvent(QPaintEvent* event) {
  QPainter painter(this);
  painter.fillRect(rect(), dark_ ? QColor("#252526") : QColor("#F0F0F0"));
  ads::CDockAreaTabBar::paintEvent(event);
}

// ── paintAllTabs ──────────────────────────────────────────────

void DockAreaTabBarStyle::paintAllTabs(QPainter* painter) {
  int cnt = count();
  for (int i = 0; i < cnt; ++i) {
    auto* tab = this->tab(i);
    if (!tab) {
      continue;
    }

    // tab->geometry() 返回相对于 tabsContainerWidget 的坐标
    QRectF r = tab->geometry();

    bool active = tab->isActiveTab();
    bool hovered = tab->underMouse();

    auto pos = mapPosition(i, cnt);
    auto sel = mapSelectedPosition(i, cnt);

    if (active) {
      auto path = selectedTabPath(r, pos);
      painter->save();
      painter->setPen(QPen(borderColor(), 1));
      painter->setBrush(selectedBrush(r.toRect()));
      painter->drawPolygon(path.toFillPolygon());
      painter->restore();

      // dark 下选中 tab 追加蓝色渐变描边
      if (dark_) {
        painter->save();
        QLinearGradient grad(r.left(), 0, r.right(), 0);
        grad.setColorAt(0.0, QColor(0x0A, 0x3A, 0x5C));
        grad.setColorAt(0.15, QColor(0x40, 0xB0, 0xEE));
        grad.setColorAt(0.5, QColor(0x90, 0xDD, 0xFF));
        grad.setColorAt(0.85, QColor(0x40, 0xB0, 0xEE));
        grad.setColorAt(1.0, QColor(0x0A, 0x3A, 0x5C));
        painter->setPen(QPen(QBrush(grad), 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
        painter->restore();
      }
    } else if (hovered) {
      auto path = hoveredTabPath(r, pos, sel);
      painter->save();
      painter->setPen(Qt::NoPen);
      painter->setBrush(hoveredColor());
      painter->drawPolygon(path.toFillPolygon());
      painter->restore();
    } else if (i > 0) {
      auto line = dividingLine(r, pos);
      painter->save();
      painter->setPen(
          QPen(dividerColor(), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
      painter->drawLine(line);
      painter->restore();
    }
  }
}

// ── 位置映射 ──────────────────────────────────────────────────

DockAreaTabBarStyle::TabPosition DockAreaTabBarStyle::mapPosition(
    int index, int cnt) const {
  if (cnt <= 1) {
    return TabPosition::OnlyOne;
  }
  if (index == 0) {
    return TabPosition::Beginning;
  }
  if (index == cnt - 1) {
    return TabPosition::End;
  }
  return TabPosition::Middle;
}

DockAreaTabBarStyle::SelectedPosition DockAreaTabBarStyle::mapSelectedPosition(
    int index, int cnt) const {
  bool next_selected = (index < cnt - 1) && tab(index + 1) &&
                       tab(index + 1)->isActiveTab();
  bool prev_selected = (index > 0) && tab(index - 1) &&
                       tab(index - 1)->isActiveTab();
  if (next_selected) {
    return SelectedPosition::NextIsSelected;
  }
  if (prev_selected) {
    return SelectedPosition::PreviousIsSelected;
  }
  return SelectedPosition::NotAdjacent;
}

// ── 色值（从 TabBarStyle 移植） ──────────────────────────────

QBrush DockAreaTabBarStyle::selectedBrush(const QRect& tabRect) const {
  if (!dark_) {
    return QBrush(QColor(0xFF, 0xFF, 0xFF));
  }
  QLinearGradient grad(0, 0, 0, tabRect.height());
  grad.setColorAt(0.0, QColor(0x5E, 0x5E, 0x60));
  grad.setColorAt(0.5, QColor(0x46, 0x46, 0x48));
  grad.setColorAt(1.0, QColor(0x2D, 0x2D, 0x2D));
  return QBrush(grad);
}

QColor DockAreaTabBarStyle::hoveredColor() const {
  return dark_ ? QColor(0x4C, 0x4C, 0x4E) : QColor(0xE8, 0xE8, 0xE8);
}

QColor DockAreaTabBarStyle::dividerColor() const {
  return dark_ ? QColor(0x3C, 0x3C, 0x3C) : QColor(0xD8, 0xD8, 0xD8);
}

QColor DockAreaTabBarStyle::borderColor() const {
  return dark_ ? QColor(0x00, 0x00, 0x00, 0x00) : QColor(0xD0, 0xD0, 0xD0);
}

void DockAreaTabBarStyle::onThemeChanged() {
  dark_ = etest::core_ui::ThemeManager::instance().isDarkTheme();
  applyViewportBackground();
  if (tabs_container_) {
    tabs_container_->update();
  }
  if (auto* vp = viewport()) {
    vp->update();
  }
  update();
}

void DockAreaTabBarStyle::applyViewportBackground() {
  if (auto* vp = viewport()) {
    // 阻止 Qt/QSS 绘制 viewport 背景，让 scroll area 的 paintEvent fillRect 透出
    vp->setAttribute(Qt::WA_StyledBackground, false);
    vp->setAttribute(Qt::WA_NoSystemBackground, true);
    vp->setAutoFillBackground(false);
  }
}

// ── 路径计算（从 TabBarStyle 移植） ──────────────────────────
// 将 QStyleOptionTab::position 替换为 TabPosition，
// 将 QStyleOptionTab::selectedPosition 替换为 SelectedPosition，
// 将 QStyleOptionTab::rect 替换为参数 QRectF r。

QPainterPath DockAreaTabBarStyle::selectedTabPath(const QRectF& r,
                                                  TabPosition pos) const {
  qreal per = r.height() * kHRatio;

  QPointF p1, p2, p3, p4, p5, p6, p7, p8;
  QPainterPath path;

  switch (pos) {
    case TabPosition::Beginning:
      path.moveTo(r.bottomLeft());
      p1 = QPointF(r.bottomLeft());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
      p5 = QPointF(r.right() - per, p4.y());
      p6 = QPointF(r.right(), p3.y());
      p7 = QPointF(r.right(), p2.y());
      p8 = QPointF(r.right() + per, p1.y());
      break;
    case TabPosition::Middle:
      path.moveTo(r.bottomLeft() - QPointF(per, 0));
      p1 = QPointF(r.left() - per, r.bottom());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
      p5 = QPointF(r.right() - per, p4.y());
      p6 = QPointF(r.right(), p3.y());
      p7 = QPointF(r.right(), p2.y());
      p8 = QPointF(r.right() + per, p1.y());
      break;
    case TabPosition::End:
      path.moveTo(r.bottomLeft() - QPointF(per, 0));
      p1 = QPointF(r.left() - per, r.bottom());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
      p5 = QPointF(r.right() - 2 * per, p4.y());
      p6 = QPointF(r.right() - per, p3.y());
      p7 = QPointF(r.right() - per, p2.y());
      p8 = QPointF(r.right(), p1.y());
      break;
    case TabPosition::OnlyOne:
      path.moveTo(r.bottomLeft());
      p1 = QPointF(r.bottomLeft());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
      p5 = QPointF(r.right() - 2 * per, p4.y());
      p6 = QPointF(r.right() - per, p3.y());
      p7 = QPointF(r.right() - per, p2.y());
      p8 = QPointF(r.right(), p1.y());
      break;
  }

  path.quadTo(QPointF(p2.x(), p1.y()), p2);
  path.lineTo(p3);
  path.quadTo(QPointF(p3.x(), p4.y()), p4);
  path.lineTo(p5);
  path.quadTo(QPointF(p6.x(), p5.y()), p6);
  path.lineTo(p7);
  path.quadTo(QPointF(p7.x(), p8.y()), p8);
  return path;
}

QPainterPath DockAreaTabBarStyle::hoveredTabPath(
    const QRectF& r, TabPosition pos, SelectedPosition sel) const {
  qreal per = r.height() * kHRatio;

  QPointF p1, p2, p3, p4, p5, p6, p7, p8;
  QPainterPath path;

  switch (sel) {
    case SelectedPosition::NotAdjacent:
      if (pos == TabPosition::Beginning) {
        path.moveTo(r.bottomLeft());
        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() + per, p1.y());
      } else if (pos == TabPosition::End) {
        path.moveTo(r.bottomLeft() - QPointF(per, 0));
        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
        p5 = QPointF(r.right() - 2 * per, p4.y());
        p6 = QPointF(r.right() - per, p3.y());
        p7 = QPointF(r.right() - per, p2.y());
        p8 = QPointF(r.right(), p1.y());
      } else {
        path.moveTo(r.bottomLeft() - QPointF(per, 0));
        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() + per, p1.y());
      }
      path.quadTo(QPointF(p2.x(), p1.y()), p2);
      path.lineTo(p3);
      path.quadTo(QPointF(p3.x(), p4.y()), p4);
      path.lineTo(p5);
      path.quadTo(QPointF(p6.x(), p5.y()), p6);
      path.lineTo(p7);
      path.quadTo(QPointF(p7.x(), p8.y()), p8);
      break;
    case SelectedPosition::NextIsSelected:
      if (pos == TabPosition::Beginning) {
        path.moveTo(r.bottomLeft());
        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() - per, p1.y());
      } else {
        path.moveTo(r.bottomLeft() - QPointF(per, 0));
        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() - per, p1.y());
      }
      path.quadTo(QPointF(p2.x(), p1.y()), p2);
      path.lineTo(p3);
      path.quadTo(QPointF(p3.x(), p4.y()), p4);
      path.lineTo(p5);
      path.quadTo(QPointF(p6.x(), p5.y()), p6);
      path.lineTo(p7);
      path.quadTo(QPointF(p7.x(), p8.y()), p8);
      break;
    case SelectedPosition::PreviousIsSelected:
      path.moveTo(r.bottomLeft() + QPointF(per, 0));
      if (pos == TabPosition::End) {
        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x(), r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
        p5 = QPointF(r.right() - 2 * per, p4.y());
        p6 = QPointF(r.right() - per, p3.y());
        p7 = QPointF(r.right() - per, p2.y());
        p8 = QPointF(r.right(), p1.y());
      } else {
        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x(), r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTopMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() + per, p1.y());
      }
      path.quadTo(QPointF(p2.x(), p1.y()), p2);
      path.lineTo(p3);
      path.quadTo(QPointF(p3.x(), p4.y()), p4);
      path.lineTo(p5);
      path.quadTo(QPointF(p6.x(), p5.y()), p6);
      path.lineTo(p7);
      path.quadTo(QPointF(p7.x(), p8.y()), p8);
      break;
  }
  return path;
}

QLineF DockAreaTabBarStyle::dividingLine(const QRectF& r,
                                         TabPosition pos) const {
  qreal mar = r.height() / 4.0;
  switch (pos) {
    case TabPosition::Beginning:
    case TabPosition::Middle:
      return QLineF(r.right(), r.top() + mar, r.right(), r.bottom() - mar);
    default:
      break;
  }
  return QLineF();
}
