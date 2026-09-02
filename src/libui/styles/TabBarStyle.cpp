#include "TabBarStyle.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QTabBar>

#include "ThemeManager.h"

TabBarStyle::TabBarStyle(int minWidth, int minHeight)
    : QProxyStyle(), min_width_(minWidth), min_height_(minHeight) {}

void TabBarStyle::install(QTabBar* tabBar, QSize minTabSize) {
  auto* existing = qobject_cast<TabBarStyle*>(tabBar->style());
  if (existing) {
    existing->setDarkTheme(
        etest::core_ui::ThemeManager::instance().isDarkTheme());
    tabBar->update();
    return;
  }
  auto* style = new TabBarStyle(minTabSize.width(), minTabSize.height());
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
  auto& tm = etest::core_ui::ThemeManager::instance();
  QColor base = tm.tabSelectedBackground();
  if (!base.isValid()) {  // 兜底：JSON 缺失键时保持旧行为
    base = tm.isDarkTheme() ? tm.panelBackground() : QColor(0xFF, 0xFF, 0xFF);
  }
  if (!tm.isDarkTheme()) {
    return QBrush(base);
  }
  QLinearGradient grad(0, 0, 0, tabRect.height());
  grad.setColorAt(0.0, base.lighter(115));
  grad.setColorAt(0.5, base.lighter(107));
  grad.setColorAt(1.0, base);
  return QBrush(grad);
}
QColor TabBarStyle::hoveredColor() const {
  return etest::core_ui::ThemeManager::instance().hoverBackground();
}
QColor TabBarStyle::dividerColor() const {
  return etest::core_ui::ThemeManager::instance().borderColor();
}
QColor TabBarStyle::textColor(bool selected) const {
  auto& tm = etest::core_ui::ThemeManager::instance();
  if (selected) {
    return tm.textColor();
  }
  return tm.secondaryTextColor();
}

QSize TabBarStyle::sizeFromContents(QStyle::ContentsType type,
                                    const QStyleOption* option,
                                    const QSize& size,
                                    const QWidget* widget) const {
  if (type == CT_TabBarTab) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.12 没有 setTabVisible，用 setTabEnabled(false) + 零尺寸模拟隐藏
    if (!option->state.testFlag(QStyle::State_Enabled)) {
      return QSize(0, 0);
    }
#endif
    QSize ret(size);
    ret.rheight() = qMax(size.height(), min_height_);
    ret.rwidth() = qMax(size.width(), min_width_);
    return ret;
  }
  return QProxyStyle::sizeFromContents(type, option, size, widget);
}

void TabBarStyle::drawControl(QStyle::ControlElement element,
                              const QStyleOption* opt,
                              QPainter* p,
                              const QWidget* w) const {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
  // Qt 5.12：setTabEnabled(false) 的 tab 不绘制
  if ((element == CE_TabBarTabShape || element == CE_TabBarTabLabel) &&
      !opt->state.testFlag(QStyle::State_Enabled)) {
    return;
  }
#endif
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
    QColor accent = etest::core_ui::ThemeManager::instance().accentColor();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(accent, kTabBorderWidth, Qt::SolidLine, Qt::RoundCap,
                         Qt::RoundJoin));
    painter->setBrush(selectedBrush(option->rect));
    // drawPath 而非 drawPolygon：填充仍按闭合区域，但描边不画底部闭合线，
    // 选中 tab 底边与内容区自然衔接
    painter->drawPath(path);
    painter->restore();

    // dark 下选中 tab 追加渐变外框（主色渐变，端点收敛保证均匀）
    if (dark_) {
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing);
      QLinearGradient grad(option->rect.left(), 0, option->rect.right(), 0);
      grad.setColorAt(0.0, accent.darker(140));
      grad.setColorAt(0.15, accent.darker(115));
      grad.setColorAt(0.5, accent);
      grad.setColorAt(0.85, accent.darker(115));
      grad.setColorAt(1.0, accent.darker(140));
      painter->setPen(QPen(QBrush(grad), kTabBorderWidth, Qt::SolidLine,
                           Qt::RoundCap, Qt::RoundJoin));
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

  qreal per = r.height() * kTabHRatio;

  QPointF p1, p2, p3, p4, p5, p6, p7, p8;
  QPainterPath path;
  switch (tabOption->position) {
    case QStyleOptionTab::Beginning:
      path.moveTo(r.bottomLeft());

      p1 = QPointF(r.bottomLeft());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
      p5 = QPointF(r.right() - per, p4.y());
      p6 = QPointF(r.right(), p3.y());
      p7 = QPointF(r.right(), p2.y());
      p8 = QPointF(r.right() + per, p1.y());

      break;
    case QStyleOptionTab::Middle:
      path.moveTo(r.bottomLeft() - QPointF(per, 0));

      p1 = QPointF(r.left() - per, r.bottom());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
      p5 = QPointF(r.right() - per, p4.y());
      p6 = QPointF(r.right(), p3.y());
      p7 = QPointF(r.right(), p2.y());
      p8 = QPointF(r.right() + per, p1.y());

      break;
    case QStyleOptionTab::End:
      path.moveTo(r.bottomLeft() - QPointF(per, 0));

      p1 = QPointF(r.left() - per, r.bottom());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
      p5 = QPointF(r.right() - 2 * per, p4.y());
      p6 = QPointF(r.right() - per, p3.y());
      p7 = QPointF(r.right() - per, p2.y());
      p8 = QPointF(r.right(), p1.y());
      break;
    case QStyleOptionTab::OnlyOneTab:
      path.moveTo(r.bottomLeft());

      p1 = QPointF(r.bottomLeft());
      p2 = QPointF(p1.x() + per, r.bottom() - per);
      p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
      p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
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

  qreal per = r.height() * kTabHRatio;

  QPointF p1, p2, p3, p4, p5, p6, p7, p8;
  QPainterPath path;
  switch (tabOption->selectedPosition) {
    case QStyleOptionTab::NotAdjacent:
      if (tabOption->position == QStyleOptionTab::Beginning) {
        path.moveTo(r.bottomLeft());

        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() + per, p1.y());

      } else if (tabOption->position == QStyleOptionTab::End) {
        path.moveTo(r.bottomLeft() - QPointF(per, 0));

        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
        p5 = QPointF(r.right() - 2 * per, p4.y());
        p6 = QPointF(r.right() - per, p3.y());
        p7 = QPointF(r.right() - per, p2.y());
        p8 = QPointF(r.right(), p1.y());
      } else {
        path.moveTo(r.bottomLeft() - QPointF(per, 0));

        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
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
        p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
        p5 = QPointF(r.right() - per, p4.y());
        p6 = QPointF(r.right(), p3.y());
        p7 = QPointF(r.right(), p2.y());
        p8 = QPointF(r.right() - per, p1.y());
      } else {
        path.moveTo(r.bottomLeft() - QPointF(per, 0));

        p1 = QPointF(r.left() - per, r.bottom());
        p2 = QPointF(p1.x() + per, r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
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
        p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
        p5 = QPointF(r.right() - 2 * per, p4.y());
        p6 = QPointF(r.right() - per, p3.y());
        p7 = QPointF(r.right() - per, p2.y());
        p8 = QPointF(r.right(), p1.y());
      } else {
        p1 = QPointF(r.bottomLeft());
        p2 = QPointF(p1.x(), r.bottom() - per);
        p3 = QPointF(p2.x(), r.top() + kTabTopMargin + per);
        p4 = QPointF(p3.x() + per, r.top() + kTabTopMargin);
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
