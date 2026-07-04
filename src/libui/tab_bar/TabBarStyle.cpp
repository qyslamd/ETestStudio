#include "TabBarStyle.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>

TabBarStyle::TabBarStyle() : QProxyStyle() {}

void TabBarStyle::setDarkTheme(bool dark) { dark_ = dark; }

// ── 主题色（dark_ 派生） ──

QColor TabBarStyle::selectedColor() const {
  // 选中：dark 主题下用更亮的灰，与 tab bar 背景有清晰对比
  return dark_ ? QColor(0x4A, 0x4A, 0x4D) : QColor(0xFF, 0xFF, 0xFF);
}
QColor TabBarStyle::hoveredColor() const {
  return dark_ ? QColor(0x3A, 0x3A, 0x3D) : QColor(0xE6, 0xE6, 0xE6);
}
QColor TabBarStyle::dividerColor() const {
  return dark_ ? QColor(0x3C, 0x3C, 0x3C) : QColor(0xE0, 0xE0, 0xE0);
}
QColor TabBarStyle::textColor(bool selected) const {
  if (selected) return QColor(0xFF, 0xFF, 0xFF);
  return dark_ ? QColor(0xCC, 0xCC, 0xCC) : QColor(0x66, 0x66, 0x66);
}

QSize TabBarStyle::sizeFromContents(QStyle::ContentsType type,
                                    const QStyleOption* option,
                                    const QSize& size,
                                    const QWidget* widget) const {
  if (type == CT_TabBarTab) {
    QSize ret(size);
    ret.rheight() = 28;
    ret.rwidth() = 100;  // 固定 tab 宽度；超出部分由 tab bar elide 处理
    return ret;
  }
  return QProxyStyle::sizeFromContents(type, option, size, widget);
}

void TabBarStyle::drawControl(QStyle::ControlElement element,
                              const QStyleOption* opt,
                              QPainter* p,
                              const QWidget* w) const {
  switch (element) {
    case CE_TabBarTabLabel:
      drawTabBarTabLabel(opt, p, w);
      break;
    case CE_TabBarTabShape:
      drawTabBarTabShape(opt, p, w);
      break;
    default:
      QProxyStyle::drawControl(element, opt, p, w);
      break;
  }
}

void TabBarStyle::drawTabBarTabShape(const QStyleOption* option,
                                     QPainter* painter,
                                     const QWidget* w) const {
  QStyle::State state = option->state;
  if (state.testFlag(QStyle::State_Selected)) {
    QPainterPath path = getSelectedShape(option);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(selectedColor());
    QPolygonF polygon = path.toFillPolygon();
    painter->drawPolygon(polygon);
    painter->restore();

  } else if (state.testFlag(QStyle::State_MouseOver)) {
    auto path = getHoveredShape(option);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(hoveredColor());
    QPolygonF polygon = path.toFillPolygon();
    painter->drawPolygon(polygon);
    painter->restore();

  } else {
    auto line = getDividingLine(option);
    painter->save();
    painter->setPen(QPen(dividerColor(), 1, Qt::SolidLine, Qt::FlatCap,
                         Qt::MiterJoin));
    painter->drawLine(line);
    painter->restore();
  }
}

// ── tab 文字（主题色绘制） ──

void TabBarStyle::drawTabBarTabLabel(const QStyleOption* option,
                                     QPainter* painter,
                                     const QWidget* widget) const {
  auto tabOption = qstyleoption_cast<const QStyleOptionTab*>(option);
  if (!tabOption) return;

  const bool selected = tabOption->state.testFlag(QStyle::State_Selected);

  // 文字区域：tab 矩形去掉左右各 8px 边距，居中显示
  QRect r = tabOption->rect;
  QRect textRect = r.adjusted(8, 0, -8, 0);

  painter->save();
  painter->setPen(textColor(selected));
  QTextOption opt(Qt::AlignCenter | Qt::AlignVCenter);
  opt.setWrapMode(QTextOption::NoWrap);
  painter->drawText(textRect, tabOption->text, opt);
  painter->restore();
}

QPainterPath TabBarStyle::getSelectedShape(const QStyleOption* option) const {
  auto tabOption = qstyleoption_cast<const QStyleOptionTab*>(option);
  QRectF r = tabOption->rect;

  qreal per = r.height() * HRatio;

  /*
   *  _______________________________
   * |       p4------------p5       |
   * |   p3                   p6    |
   * |   |                    |     |
   * |   |                    |     |
   * |   |                    |     |
   * |   p2                   p7    |
   * |p1                         p8 |
   * --------------------------------
   *
   * */
  QPointF p1, p2, p3, p4, p5, p6, p7, p8;
  QPainterPath path;
  switch (tabOption->position) {
    case QStyleOptionTab::Beginning:  // 左下用自己，右下借用
      path.moveTo(r.bottomLeft());

      p1 = QPointF(r.bottomLeft());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + topMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + topMargin);
      p5 = QPointF(r.right() - per, p4.y());
      p6 = QPointF(r.right(), p3.y());
      p7 = QPointF(r.right(), p2.y());
      p8 = QPointF(r.right() + per, p1.y());

      break;
    case QStyleOptionTab::Middle:  // 左下借用，右下借用
      path.moveTo(r.bottomLeft() - QPointF(per, 0));

      p1 = QPointF(r.left() - per, r.bottom());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + topMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + topMargin);
      p5 = QPointF(r.right() - per, p4.y());
      p6 = QPointF(r.right(), p3.y());
      p7 = QPointF(r.right(), p2.y());
      p8 = QPointF(r.right() + per, p1.y());

      break;
    case QStyleOptionTab::End:  // 左下借用，右下用自己
      path.moveTo(r.bottomLeft() - QPointF(per, 0));

      p1 = QPointF(r.left() - per, r.bottom());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + topMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + topMargin);
      p5 = QPointF(r.right() - 2 * per, p4.y());
      p6 = QPointF(r.right() - per, p3.y());
      p7 = QPointF(r.right() - per, p2.y());
      p8 = QPointF(r.right(), p1.y());
      break;
    case QStyleOptionTab::OnlyOneTab:  // 左右都用自己
      path.moveTo(r.bottomLeft());

      p1 = QPointF(r.bottomLeft());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + topMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + topMargin);
      p5 = QPointF(r.right() - 2 * per, p4.y());
      p6 = QPointF(r.right() - per, p3.y());
      p7 = QPointF(r.right() - per, p2.y());
      p8 = QPointF(r.right(), p1.y());
      break;
    default:
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

QPainterPath TabBarStyle::getHoveredShape(const QStyleOption* option) const {
  auto tabOption = qstyleoption_cast<const QStyleOptionTab*>(option);
  QRectF r = tabOption->rect;

  qreal per = r.height() * HRatio;

  QPointF p1, p2, p3, p4, p5, p6, p7, p8;
  QPainterPath path;
  switch (tabOption->selectedPosition) {
    case QStyleOptionTab::NotAdjacent:
      // 旁边没有选中的，是第一个
      if (tabOption->position == QStyleOptionTab::Beginning) {
        // 左边用自己的区域，右边借用
        path.moveTo(r.bottomLeft());

        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() + per, p1.y());

      }  // 旁边没有选中，是最后一个
      else if (tabOption->position == QStyleOptionTab::End) {
        // 左下借用，右下用自己
        path.moveTo(r.bottomLeft() - QPointF(per, 0));

        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
        p5 = QPointF(r.right() - 2 * per, p4.y());
        p6 = QPointF(r.right() - per, p3.y());
        p7 = QPointF(r.right() - per, p2.y());
        p8 = QPointF(r.right(), p1.y());
      }  // // 旁边没有选中，在中间任何一个位置
      else {
        // 左右都借用
        path.moveTo(r.bottomLeft() - QPointF(per, 0));

        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
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
    case QStyleOptionTab::NextIsSelected:
      // hover的是开始，下一个被选中了
      if (tabOption->position == QStyleOptionTab::Beginning) {
        // 左边用自己的区域，右边收回
        path.moveTo(r.bottomLeft());

        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() - per, p1.y());
      }  // 这种情况下只有Middle了
      else {
        // 左边借用，右边回收
        path.moveTo(r.bottomLeft() - QPointF(per, 0));

        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
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
    case QStyleOptionTab::PreviousIsSelected:
      path.moveTo(r.bottomLeft() + QPointF(per, 0));

      if (tabOption->position == QStyleOptionTab::End) {
        // 左边收，右边用自己的

        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x(), r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
        p5 = QPointF(r.right() - 2 * per, p4.y());
        p6 = QPointF(r.right() - per, p3.y());
        p7 = QPointF(r.right() - per, p2.y());
        p8 = QPointF(r.right(), p1.y());
      } else {
        // 左边收，右边借用

        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x(), r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
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
    default:
      break;
  }
  return path;
}

QLineF TabBarStyle::getDividingLine(const QStyleOption* option) const {
  auto tabOption = qstyleoption_cast<const QStyleOptionTab*>(option);
  QRectF r = tabOption->rect;

  qreal mar = r.height() / 4.0;
  switch (tabOption->position) {
    case QStyleOptionTab::Beginning:
    case QStyleOptionTab::Middle:
      return QLineF(r.right(), r.top() + mar, r.right(), r.bottom() - mar);
    default:
      break;
  }
  return QLineF();
}
