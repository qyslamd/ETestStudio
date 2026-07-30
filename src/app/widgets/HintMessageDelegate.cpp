#include "HintMessageDelegate.h"

#include <QAbstractItemModel>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>

#include "MessageService.h"
#include "ThemeManager.h"

namespace etest::app {

HintMessageDelegate::HintMessageDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QRect HintMessageDelegate::actionRect(const QRect& itemRect) {
  return QRect(itemRect.right() - kCloseBtnWidth - kMargin - kActionBtnWidth,
               itemRect.top(), kActionBtnWidth, itemRect.height());
}

QRect HintMessageDelegate::closeRect(const QRect& itemRect) {
  return QRect(itemRect.right() - kCloseBtnWidth, itemRect.top(),
               kCloseBtnWidth, itemRect.height());
}

HintMessageDelegate::ClickRegion HintMessageDelegate::hitTest(
    const QRect& itemRect, const QPoint& pos, bool hasAction) {
  if (closeRect(itemRect).contains(pos)) {
    return ClickRegion::Close;
  }
  if (hasAction && actionRect(itemRect).contains(pos)) {
    return ClickRegion::Action;
  }
  return ClickRegion::None;
}

void HintMessageDelegate::setHoveredRegion(int row, ClickRegion region) {
  if (hovered_row_ == row && hovered_region_ == region) {
    return;
  }
  hovered_row_ = row;
  hovered_region_ = region;
}

void HintMessageDelegate::paint(QPainter* painter,
                                 const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
  painter->save();

  auto& tm = etest::core_ui::ThemeManager::instance();
  bool isDark = tm.isDarkTheme();

  // 背景：交替行色 + hover 高亮
  if (option.state & QStyle::State_MouseOver) {
    painter->fillRect(option.rect, tm.hoverBackground());
  } else if (index.row() % 2 == 1) {
    painter->fillRect(option.rect, isDark ? QColor("#2A2A2B")
                                          : QColor("#F8F9FA"));
  }

  bool read = index.data(MessageService::ReadRole).toBool();
  bool hasAction = index.data(MessageService::HasActionRole).toBool();
  QString text = index.data(MessageService::TextRole).toString();
  QString actionLabel = index.data(MessageService::ActionLabelRole).toString();

  // 左侧色块（未读时显示）
  if (!read) {
    QColor indicatorColor = tm.accentColor();
    painter->fillRect(
        QRect(option.rect.left(), option.rect.top(), kIndicatorWidth,
              option.rect.height()),
        indicatorColor);
  }

  // 文本区域（色块右侧到操作按钮左侧）
  int textLeft = option.rect.left() + kIndicatorWidth + kMargin;
  int textRight = option.rect.right() - kCloseBtnWidth - kMargin;
  if (hasAction) {
    textRight -= kActionBtnWidth + kMargin;
  }
  QRect textRect(textLeft, option.rect.top(), textRight - textLeft,
                 option.rect.height());

  QFont font = option.font;
  if (!read) {
    font.setBold(true);
  }
  painter->setFont(font);
  painter->setPen(tm.textColor());

  QFontMetrics fm(font);
  QString elidedText =
      fm.elidedText(text, Qt::ElideRight, textRect.width());
  painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedText);

  // 操作按钮
  if (hasAction) {
    QRect aRect = actionRect(option.rect);
    QString label =
        actionLabel.isEmpty() ? QStringLiteral("操作") : actionLabel;
    bool actionHovered = (index.row() == hovered_row_ &&
                          hovered_region_ == ClickRegion::Action);
    QColor actionColor = tm.accentColor();
    if (actionHovered) {
      painter->fillRect(aRect, tm.hoverBackground());
    }
    painter->setPen(actionColor);
    QFont btnFont = option.font;
    btnFont.setPointSize(btnFont.pointSize() - 1);
    btnFont.setUnderline(true);
    painter->setFont(btnFont);
    painter->drawText(aRect, Qt::AlignCenter, label);
  }

  // 关闭按钮
  QRect cRect = closeRect(option.rect);
  bool closeHovered = (index.row() == hovered_row_ &&
                       hovered_region_ == ClickRegion::Close);
  QColor closeColor = tm.secondaryTextColor();
  if (closeHovered) {
    painter->fillRect(cRect, tm.hoverBackground());
    closeColor = tm.textColor();
  }
  painter->setPen(QPen(closeColor, 1.5));
  int cx = cRect.center().x();
  int cy = cRect.center().y();
  int r = 4;
  painter->drawLine(cx - r, cy - r, cx + r, cy + r);
  painter->drawLine(cx + r, cy - r, cx - r, cy + r);

  painter->restore();
}

QSize HintMessageDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                     const QModelIndex& /*index*/) const {
  return QSize(0, kItemHeight);
}

bool HintMessageDelegate::editorEvent(QEvent* event,
                                       QAbstractItemModel* /*model*/,
                                       const QStyleOptionViewItem& option,
                                       const QModelIndex& index) {
  if (event->type() == QEvent::MouseButtonRelease) {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    bool hasAction = index.data(MessageService::HasActionRole).toBool();
    ClickRegion region =
        hitTest(option.rect, mouseEvent->pos(), hasAction);
    int row = index.row();
    switch (region) {
      case ClickRegion::Action:
        emit actionTriggered(row);
        return true;
      case ClickRegion::Close:
        emit closeRequested(row);
        return true;
      default:
        break;
    }
  }
  return false;
}

}  // namespace etest::app
