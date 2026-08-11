#include "RecentProjOrFileDelegate.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolTip>

#include "AppIconProvider.h"
#include "ThemeManager.h"

namespace etest::app {

using etest::core_ui::AppIconProvider;

RecentProjOrFileDelegate::RecentProjOrFileDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void RecentProjOrFileDelegate::setCloseButtonVisible(bool visible) {
  close_button_visible_ = visible;
}

void RecentProjOrFileDelegate::setShowTime(bool show) { show_time_ = show; }

void RecentProjOrFileDelegate::paint(QPainter* painter,
                                     const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const {
  // Draw selection / hover background via the style system（QSS）
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  opt.text.clear();
  QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

  const QString fileName = index.data(Qt::DisplayRole).toString();
  const QString dirPath = index.data(DirPathRole).toString();
  const QString timeStr =
      show_time_ ? index.data(TimeStrRole).toString() : QString();

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  const QRect itemRect = option.rect;
  const bool itemHovered = (option.state & QStyle::State_MouseOver);

  // 左侧内边距：带图标（model setIcon）时避开 decoration 区。
  // 读 initStyleOption 后的 opt：option.features 恒为 0（HasDecoration 由
  // initStyleOption 填充）。
  int leftMargin = 8;
  if (opt.features & QStyleOptionViewItem::HasDecoration) {
    leftMargin += opt.decorationSize.width() + 6;
  }

  // 右侧：时间占位 + 关闭按钮占位
  int timeWidth = 0;
  if (show_time_ && !timeStr.isEmpty()) {
    timeWidth = option.fontMetrics.horizontalAdvance(timeStr);
  }
  int textRight = itemRect.right() - 4 - timeWidth - (timeWidth > 0 ? 8 : 0);
  if (itemHovered && close_button_visible_) {
    textRight -= 24;  // room for close button
  }
  const int textLeft = itemRect.left() + leftMargin;
  const int textWidth = textRight - textLeft;

  // 两行：第 1 行名称（粗体），第 2 行路径（小一号）
  const int half = itemRect.height() / 2;
  QFont nameFont = option.font;
  nameFont.setBold(true);
  const QFontMetrics nameFm(nameFont);
  const QString elidedName =
      nameFm.elidedText(fileName, Qt::ElideRight, textWidth);
  painter->setFont(nameFont);
  painter->setPen(option.palette.color(QPalette::WindowText));
  painter->drawText(QRect(textLeft, itemRect.top(), textWidth, half),
                    Qt::AlignLeft | Qt::AlignVCenter, elidedName);

  if (!dirPath.isEmpty()) {
    QFont pathFont = option.font;
    pathFont.setPointSizeF(qMax(option.font.pointSizeF() - 1.0, 6.0));
    const QFontMetrics smallFm(pathFont);
    const QString elidedPath =
        smallFm.elidedText(dirPath, Qt::ElideLeft, textWidth);
    painter->setFont(pathFont);
    painter->setPen(
        option.palette.color(QPalette::Disabled, QPalette::WindowText));
    painter->drawText(QRect(textLeft, itemRect.top() + half, textWidth, half),
                      Qt::AlignLeft | Qt::AlignVCenter, elidedPath);
  }

  // 时间（右侧，垂直居中）
  if (show_time_ && !timeStr.isEmpty()) {
    painter->setFont(option.font);
    painter->setPen(
        option.palette.color(QPalette::Disabled, QPalette::WindowText));
    painter->drawText(
        QRect(textRight + 8, itemRect.top(), timeWidth, itemRect.height()),
        Qt::AlignRight | Qt::AlignVCenter, timeStr);
  }

  // 关闭按钮（hover 显示，已打开列表）
  if (itemHovered && close_button_visible_) {
    const QRect btnRect = closeButtonRect(option);
    const bool btnHovered = index.data(CloseBtnHoverRole).toBool();

    painter->setPen(Qt::NoPen);
    if (btnHovered) {
      painter->setBrush(etest::core_ui::ThemeManager::instance().accentColor());
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

QSize RecentProjOrFileDelegate::sizeHint(
    const QStyleOptionViewItem& option, const QModelIndex& index) const {
  Q_UNUSED(index)
  // 两行条目：行高约两行字高 + 上下内边距
  const int h = qMax(option.fontMetrics.height() * 2 + 10, 36);
  return QSize(0, h);
}

bool RecentProjOrFileDelegate::editorEvent(QEvent* event,
                                           QAbstractItemModel* model,
                                           const QStyleOptionViewItem& option,
                                           const QModelIndex& index) {
  auto* me = dynamic_cast<QMouseEvent*>(event);
  if (!me) {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }

  switch (me->type()) {
    case QEvent::MouseMove: {
      if (close_button_visible_) {
        const bool overClose = closeButtonRect(option).contains(me->pos());
        model->setData(index, overClose, CloseBtnHoverRole);
      }
      break;
    }
    case QEvent::MouseButtonRelease: {
      if (close_button_visible_ && me->button() == Qt::LeftButton &&
          closeButtonRect(option).contains(me->pos())) {
        const QString filePath = index.data(FilePathRole).toString();
        emit closeRequested(filePath);
        return true;
      }
      break;
    }
    case QEvent::ToolTip: {
      if (close_button_visible_ && closeButtonRect(option).contains(me->pos())) {
        QToolTip::showText(me->globalPos(), QStringLiteral("关闭文件"), nullptr);
        return true;
      }
      break;
    }
    default:
      break;
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QRect RecentProjOrFileDelegate::closeButtonRect(
    const QStyleOptionViewItem& option) {
  const int btnSize = 16;
  const int margin = 8;
  const int x = option.rect.right() - btnSize - margin;
  const int y = option.rect.center().y() - btnSize / 2;
  return QRect(x, y, btnSize, btnSize);
}

}  // namespace etest::app
