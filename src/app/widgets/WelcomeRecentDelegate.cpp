#include "WelcomeRecentDelegate.h"

#include <QAbstractItemModel>
#include <QEvent>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolTip>

#include "AppIconProvider.h"
#include "RecentProjOrFileDelegate.h"  // RecentItemRole
#include "ThemeManager.h"

namespace etest::app {

using etest::core_ui::AppIconProvider;

WelcomeRecentDelegate::WelcomeRecentDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void WelcomeRecentDelegate::paint(QPainter* painter,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const {
  // 完全自绘（同 RecentProjOrFileDelegate）：QSS ::item 对自定义 delegate 不可靠
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

  const QString name = index.data(Qt::DisplayRole).toString();
  const QString dirPath = index.data(DirPathRole).toString();
  const QString timeStr = index.data(TimeStrRole).toString();

  const int leftMargin = 12;
  const int rightMargin = itemHovered ? 28 : 8;  // hover 时给移除按钮让位
  const int textWidth = itemRect.width() - leftMargin - rightMargin;

  // 三行：名称（粗体）/ 路径 / 时间（空时间不占行）
  const int lineCount = timeStr.isEmpty() ? 2 : 3;
  const int lineHeight = itemRect.height() / lineCount;
  const int textLeft = itemRect.left() + leftMargin;

  QFont nameFont = option.font;
  nameFont.setBold(true);
  const QFontMetrics nameFm(nameFont);
  painter->setFont(nameFont);
  painter->setPen(tm.textColor());
  painter->drawText(
      QRect(textLeft, itemRect.top(), textWidth, lineHeight),
      Qt::AlignLeft | Qt::AlignVCenter,
      nameFm.elidedText(name, Qt::ElideRight, textWidth));

  QFont smallFont = option.font;
  smallFont.setPointSizeF(qMax(option.font.pointSizeF() - 1.0, 6.0));
  const QFontMetrics smallFm(smallFont);
  painter->setFont(smallFont);
  painter->setPen(tm.disabledTextColor());
  painter->drawText(
      QRect(textLeft, itemRect.top() + lineHeight, textWidth, lineHeight),
      Qt::AlignLeft | Qt::AlignVCenter,
      smallFm.elidedText(dirPath, Qt::ElideRight, textWidth));

  if (!timeStr.isEmpty()) {
    painter->setFont(smallFont);
    painter->setPen(tm.disabledTextColor());
    painter->drawText(
        QRect(textLeft, itemRect.top() + 2 * lineHeight, textWidth, lineHeight),
        Qt::AlignLeft | Qt::AlignVCenter, timeStr);
  }

  // 移除按钮（hover 显示）
  if (itemHovered) {
    const QRect btnRect = removeButtonRect(option);
    const bool btnHovered = index.data(CloseBtnHoverRole).toBool();

    painter->setPen(Qt::NoPen);
    if (btnHovered) {
      painter->setBrush(tm.accentColor());
    } else {
      QColor bg = tm.textColor();
      bg.setAlpha(60);
      painter->setBrush(bg);
    }
    painter->drawEllipse(btnRect);
    AppIconProvider::instance()
        .icon(QStringLiteral("close"))
        .paint(painter, btnRect, Qt::AlignCenter, QIcon::Normal);
  }

  painter->restore();
}

QSize WelcomeRecentDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const {
  Q_UNUSED(index)
  // 三行卡片：约 3 行字高 + 上下内边距
  const int h = qMax(option.fontMetrics.height() * 3 + 12, 56);
  return QSize(0, h);
}

bool WelcomeRecentDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                        const QStyleOptionViewItem& option,
                                        const QModelIndex& index) {
  auto* me = dynamic_cast<QMouseEvent*>(event);
  if (!me) {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }
  switch (me->type()) {
    case QEvent::MouseMove: {
      model->setData(index, removeButtonRect(option).contains(me->pos()),
                     CloseBtnHoverRole);
      break;
    }
    case QEvent::MouseButtonRelease: {
      if (me->button() == Qt::LeftButton &&
          removeButtonRect(option).contains(me->pos())) {
        const QString path = index.data(FilePathRole).toString();
        emit removeRequested(path);
        return true;
      }
      break;
    }
    case QEvent::ToolTip: {
      if (removeButtonRect(option).contains(me->pos())) {
        QToolTip::showText(me->globalPos(),
                           QStringLiteral("从列表中移除"), nullptr);
        return true;
      }
      break;
    }
    default:
      break;
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QRect WelcomeRecentDelegate::removeButtonRect(
    const QStyleOptionViewItem& option) {
  const int btnSize = 16;
  const int margin = 8;
  const int x = option.rect.right() - btnSize - margin;
  const int y = option.rect.center().y() - btnSize / 2;
  return QRect(x, y, btnSize, btnSize);
}

}  // namespace etest::app
