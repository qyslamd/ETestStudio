#include "VerticalTabListDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>

namespace etest::app {

VerticalTabListDelegate::VerticalTabListDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void VerticalTabListDelegate::paint(QPainter* painter,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const {
  // 选中/悬停背景交由 style 绘制，文字和 icon 自己画以控制布局
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  opt.text.clear();
  opt.icon = QIcon();
  QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

  QRect itemRect = option.rect;
  bool itemHovered = (option.state & QStyle::State_MouseOver);
  bool itemSelected = (option.state & QStyle::State_Selected);

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  // 悬停背景（选中已由 style 绘制）
  if (itemHovered && !itemSelected) {
    QColor bgColor = option.palette.color(QPalette::Base);
    bool isDark = bgColor.lightness() < 128;
    QColor hoverColor =
        isDark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 16);
    painter->fillRect(itemRect, hoverColor);
  }

  // icon
  const int iconSize = 16;
  const int leftMargin = 8;
  QRect iconRect(itemRect.left() + leftMargin,
                 itemRect.top() + (itemRect.height() - iconSize) / 2, iconSize,
                 iconSize);
  QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
  if (!icon.isNull()) {
    icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal);
  }

  // 文字（elide）
  int textLeft = iconRect.right() + 6;
  int textRight = itemRect.right() - 4;
  int textWidth = textRight - textLeft;
  if (textWidth > 0) {
    QString text = index.data(Qt::DisplayRole).toString();
    text = option.fontMetrics.elidedText(text, Qt::ElideRight, textWidth);
    painter->setPen(option.palette.color(QPalette::WindowText));
    QRect textRect(textLeft, itemRect.top(), textWidth, itemRect.height());
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
  }

  painter->restore();
}

QSize VerticalTabListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const {
  Q_UNUSED(index)
  int h = qMax(option.fontMetrics.height() + 8, 28);
  return QSize(0, h);
}



}  // namespace etest::app
