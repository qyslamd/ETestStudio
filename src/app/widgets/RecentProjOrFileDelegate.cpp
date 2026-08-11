#include "RecentProjOrFileDelegate.h"

#include <QIcon>
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
  // 完全自绘（参照 HintMessageDelegate）：QSS 的 QListView::item:hover/selected
  // 对自定义 delegate 不可靠，背景用 ThemeManager 主题色，明暗自适应。
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  const QRect itemRect = option.rect;
  const bool itemHovered = (option.state & QStyle::State_MouseOver);

  auto& tm = etest::core_ui::ThemeManager::instance();

  // 背景：选中 → selectionBackground，hover → hoverBackground
  if (option.state & QStyle::State_Selected) {
    painter->fillRect(itemRect, tm.selectionBackground());
  } else if (itemHovered) {
    painter->fillRect(itemRect, tm.hoverBackground());
  }

  const QString fileName = index.data(Qt::DisplayRole).toString();
  const QString dirPath = index.data(DirPathRole).toString();
  const QString timeStr =
      show_time_ ? index.data(TimeStrRole).toString() : QString();

  // 图标（model setIcon，经 DecorationRole 自绘）
  const QVariant deco = index.data(Qt::DecorationRole);
  const bool hasIcon =
      deco.type() == QVariant::Icon && !deco.value<QIcon>().isNull();
  const int iconSize = 16;
  if (hasIcon) {
    const QRect iconRect(itemRect.left() + 4,
                         itemRect.center().y() - iconSize / 2, iconSize,
                         iconSize);
    deco.value<QIcon>().paint(painter, iconRect, Qt::AlignCenter,
                              QIcon::Normal);
  }
  const int leftMargin = 8 + (hasIcon ? iconSize + 6 : 0);

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
  painter->setPen(tm.textColor());
  painter->drawText(QRect(textLeft, itemRect.top(), textWidth, half),
                    Qt::AlignLeft | Qt::AlignVCenter, elidedName);

  if (!dirPath.isEmpty()) {
    QFont pathFont = option.font;
    pathFont.setPointSizeF(qMax(option.font.pointSizeF() - 1.0, 6.0));
    const QFontMetrics smallFm(pathFont);
    const QString elidedPath =
        smallFm.elidedText(dirPath, Qt::ElideLeft, textWidth);
    painter->setFont(pathFont);
    painter->setPen(tm.disabledTextColor());
    painter->drawText(QRect(textLeft, itemRect.top() + half, textWidth, half),
                      Qt::AlignLeft | Qt::AlignVCenter, elidedPath);
  }

  // 时间（右侧，垂直居中）
  if (show_time_ && !timeStr.isEmpty()) {
    painter->setFont(option.font);
    painter->setPen(tm.disabledTextColor());
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
      painter->setBrush(tm.accentColor());
    } else {
      QColor btnBg = tm.textColor();
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
