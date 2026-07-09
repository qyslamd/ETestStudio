#include "TabBarStyle.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QTabBar>

#include "ThemeManager.h"

TabBarStyle::TabBarStyle() : QProxyStyle() {}

void TabBarStyle::install(QTabBar* tabBar) {
  auto* style = new TabBarStyle();
  style->dark_ = etest::core_ui::ThemeManager::instance().isDarkTheme();
  tabBar->setStyle(style);
  QObject::connect(&etest::core_ui::ThemeManager::instance(),
                   &etest::core_ui::ThemeManager::themeChanged, tabBar,
                   [tabBar, style](bool isDark) {
                     style->setDarkTheme(isDark);
                     tabBar->update();
                   });
}

void TabBarStyle::setDarkTheme(bool dark) {
  dark_ = dark;
}

// ── 主题色（dark_ 派生） ──

QBrush TabBarStyle::selectedBrush(const QRect& tabRect) const {
  if (!dark_)
    return QBrush(QColor(0xFF, 0xFF, 0xFF));
  QLinearGradient grad(0, 0, 0, tabRect.height());
  grad.setColorAt(0.0, QColor(0x5E, 0x5E, 0x60));
  grad.setColorAt(0.5, QColor(0x46, 0x46, 0x48));
  grad.setColorAt(1.0, QColor(0x2D, 0x2D, 0x2D));
  return QBrush(grad);
}
QColor TabBarStyle::hoveredColor() const {
  return dark_ ? QColor(0x4C, 0x4C, 0x4E) : QColor(0xE8, 0xE8, 0xE8);
}
QColor TabBarStyle::dividerColor() const {
  return dark_ ? QColor(0x3C, 0x3C, 0x3C) : QColor(0xD8, 0xD8, 0xD8);
}
QColor TabBarStyle::textColor(bool selected) const {
  if (selected)
    return dark_ ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x33, 0x33, 0x33);
  return dark_ ? QColor(0xCC, 0xCC, 0xCC) : QColor(0x88, 0x88, 0x88);
}
QColor TabBarStyle::borderColor() const {
  return dark_ ? QColor(0x00, 0x00, 0x00, 0x00) : QColor(0xD0, 0xD0, 0xD0);
}

QSize TabBarStyle::sizeFromContents(QStyle::ContentsType type,
                                    const QStyleOption* option,
                                    const QSize& size,
                                    const QWidget* widget) const {
  if (type == CT_TabBarTab) {
    QSize ret(size);
    ret.rheight() = 28;
    ret.rwidth() = 110;
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
    painter->setPen(QPen(borderColor(), 1));
    painter->setBrush(selectedBrush(option->rect));
    QPolygonF polygon = path.toFillPolygon();
    painter->drawPolygon(polygon);
    painter->restore();

    // dark 下选中 tab 追加红色外框
    if (dark_) {
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing);
      QLinearGradient grad(option->rect.left(), 0, option->rect.right(), 0);
      grad.setColorAt(0.0,  QColor(0x0A, 0x3A, 0x5C));
      grad.setColorAt(0.15, QColor(0x40, 0xB0, 0xEE));
      grad.setColorAt(0.5,  QColor(0x90, 0xDD, 0xFF));
      grad.setColorAt(0.85, QColor(0x40, 0xB0, 0xEE));
      grad.setColorAt(1.0,  QColor(0x0A, 0x3A, 0x5C));
      painter->setPen(QPen(QBrush(grad), 1));
      painter->setBrush(Qt::NoBrush);
      painter->drawPath(path);
      painter->restore();
    }

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
    painter->setPen(
        QPen(dividerColor(), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter->drawLine(line);
    painter->restore();
  }
}

void TabBarStyle::drawTabBarTabLabel(const QStyleOption* option,
                                     QPainter* painter,
                                     const QWidget* widget) const {
  auto tabOption = qstyleoption_cast<const QStyleOptionTab*>(option);
  if (!tabOption)
    return;

  const bool selected = tabOption->state.testFlag(QStyle::State_Selected);

  QRect r = tabOption->rect;
  QRect textRect = r.adjusted(8, 0, -8, 0);

  painter->save();
  painter->setPen(textColor(selected));

  if (!tabOption->icon.isNull()) {
    int iconSize = pixelMetric(QStyle::PM_TabBarIconSize, option, widget);
    if (iconSize <= 0) {
      iconSize = 16;
    }
    QRect iconRect(r.left() + 8, r.top() + (r.height() - iconSize) / 2,
                   iconSize, iconSize);
    tabOption->icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal);
    textRect.setLeft(iconRect.right() + 4);
  }

  Qt::Alignment align = tabOption->icon.isNull()
                            ? (Qt::AlignCenter | Qt::AlignVCenter)
                            : (Qt::AlignLeft | Qt::AlignVCenter);
  QTextOption opt(align);
  opt.setWrapMode(QTextOption::NoWrap);
  QString text = painter->fontMetrics().elidedText(
      tabOption->text, Qt::ElideRight, textRect.width());
  painter->drawText(textRect, text, opt);
  painter->restore();
}

QPainterPath TabBarStyle::getSelectedShape(const QStyleOption* option) const {
  auto tabOption = qstyleoption_cast<const QStyleOptionTab*>(option);
  QRectF r = tabOption->rect;

  qreal per = r.height() * HRatio;

  QPointF p1, p2, p3, p4, p5, p6, p7, p8;
  QPainterPath path;
  switch (tabOption->position) {
    case QStyleOptionTab::Beginning:
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
    case QStyleOptionTab::Middle:
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
    case QStyleOptionTab::End:
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
    case QStyleOptionTab::OnlyOneTab:
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
      if (tabOption->position == QStyleOptionTab::Beginning) {
        path.moveTo(r.bottomLeft());

        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() + per, p1.y());

      } else if (tabOption->position == QStyleOptionTab::End) {
        path.moveTo(r.bottomLeft() - QPointF(per, 0));

        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
        p5 = QPointF(r.right() - 2 * per, p4.y());
        p6 = QPointF(r.right() - per, p3.y());
        p7 = QPointF(r.right() - per, p2.y());
        p8 = QPointF(r.right(), p1.y());
      } else {
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
      if (tabOption->position == QStyleOptionTab::Beginning) {
        path.moveTo(r.bottomLeft());

        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() - per, p1.y());
      } else {
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
        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x(), r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + topMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + topMargin);
        p5 = QPointF(r.right() - 2 * per, p4.y());
        p6 = QPointF(r.right() - per, p3.y());
        p7 = QPointF(r.right() - per, p2.y());
        p8 = QPointF(r.right(), p1.y());
      } else {
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
