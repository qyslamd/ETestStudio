#include "VerticalTabListDelegate.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolTip>

#include "common/AppIconProvider.h"

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
  bool closable = index.data(ClosableRole).toBool();

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

  // 文字（elide），关闭按钮出现时让出右侧空间
  int textLeft = iconRect.right() + 6;
  int textRight = itemRect.right() - 4;
  if (itemHovered && closable) {
    textRight -= 24;
  }
  int textWidth = textRight - textLeft;
  if (textWidth > 0) {
    QString text = index.data(Qt::DisplayRole).toString();
    text = option.fontMetrics.elidedText(text, Qt::ElideRight, textWidth);
    painter->setPen(option.palette.color(QPalette::WindowText));
    QRect textRect(textLeft, itemRect.top(), textWidth, itemRect.height());
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
  }

  // 关闭按钮（仅可关闭项悬停时）
  if (itemHovered && closable) {
    QRect btnRect = closeButtonRect(option);
    bool btnHovered = index.data(CloseBtnHoverRole).toBool();

    painter->setPen(Qt::NoPen);
    if (btnHovered) {
      painter->setBrush(QColor(200, 60, 60, 200));
    } else {
      QColor btnBg = option.palette.color(QPalette::WindowText);
      btnBg.setAlpha(60);
      painter->setBrush(btnBg);
    }
    painter->drawEllipse(btnRect);

    AppIconProvider::instance()
        .icon(QStringLiteral("close"))
        .paint(painter, btnRect, Qt::AlignCenter, QIcon::Normal);
  }

  painter->restore();
}

QSize VerticalTabListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const {
  Q_UNUSED(index)
  int h = qMax(option.fontMetrics.height() + 8, 28);
  return QSize(0, h);
}

bool VerticalTabListDelegate::editorEvent(QEvent* event,
                                          QAbstractItemModel* model,
                                          const QStyleOptionViewItem& option,
                                          const QModelIndex& index) {
  if (event->type() != QEvent::MouseButtonPress &&
      event->type() != QEvent::MouseButtonRelease &&
      event->type() != QEvent::MouseMove) {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }
  auto* me = static_cast<QMouseEvent*>(event);

  bool closable = index.data(ClosableRole).toBool();
  switch (me->type()) {
    case QEvent::MouseMove: {
      if (closable) {
        bool overClose = closeButtonRect(option).contains(me->pos());
        // 仅 hover 状态变化时才 setData，避免高频鼠标移动触发无效重绘
        if (overClose != index.data(CloseBtnHoverRole).toBool()) {
          model->setData(index, overClose, CloseBtnHoverRole);
        }
      }
      break;
    }
    case QEvent::MouseButtonRelease: {
      if (closable && me->button() == Qt::LeftButton &&
          closeButtonRect(option).contains(me->pos())) {
        int tabIndex = index.data(TabIndexRole).toInt();
        emit closeRequested(tabIndex);
        return true;
      }
      break;
    }
    case QEvent::ToolTip: {
      if (closable && closeButtonRect(option).contains(me->pos())) {
        QToolTip::showText(me->globalPos(), QStringLiteral("关闭用例"), nullptr);
        return true;
      }
      break;
    }
    default:
      break;
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QRect VerticalTabListDelegate::closeButtonRect(
    const QStyleOptionViewItem& option) {
  const int btnSize = 16;
  const int margin = 8;
  int x = option.rect.right() - btnSize - margin;
  int y = option.rect.center().y() - btnSize / 2;
  return QRect(x, y, btnSize, btnSize);
}

}  // namespace etest::app
