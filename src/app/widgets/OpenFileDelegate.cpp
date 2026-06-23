#include "OpenFileDelegate.h"

#include <QApplication>

#include "AppIconProvider.h"
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolTip>

namespace etest::app {

OpenFileDelegate::OpenFileDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void OpenFileDelegate::setCloseButtonVisible(bool visible) {
  close_button_visible_ = visible;
}

void OpenFileDelegate::paint(QPainter* painter,
                             const QStyleOptionViewItem& option,
                             const QModelIndex& index) const {
  // Draw selection / hover background via the style system
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  opt.text.clear();
  QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

  QString fileName = index.data(Qt::DisplayRole).toString();
  QString dirPath = index.data(DirPathRole).toString();

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  QRect itemRect = option.rect;
  bool itemHovered = (option.state & QStyle::State_MouseOver);
  bool itemSelected = (option.state & QStyle::State_Selected);

  // ── Item hover background ──
  if (itemHovered && !itemSelected) {
    QColor bgColor = option.palette.color(QPalette::Base);
    bool isDark = bgColor.lightness() < 128;
    QColor hoverColor = isDark ? QColor(255, 255, 255, 26)
                               : QColor(0, 0, 0, 16);
    painter->fillRect(itemRect, hoverColor);
  }

  // ── Text area ──
  int leftMargin = 8;
  int rightMargin = 4;
  int textRight = itemRect.right() - rightMargin;
  if (itemHovered && close_button_visible_) {
    textRight -= 24;  // room for close button
  }
  int textLeft = itemRect.left() + leftMargin;
  int textWidth = textRight - textLeft;

  // Measure file name width in bold font
  QFont boldFont = option.font;
  boldFont.setBold(true);
  QFontMetrics boldFm(boldFont);

  QFont normalFont = option.font;
  normalFont.setBold(false);
  QFontMetrics normalFm(normalFont);

  // Prefer to keep the full file name visible; elide the path
  int fileNameWidth = boldFm.horizontalAdvance(fileName + QStringLiteral("  "));
  int pathMaxWidth = textWidth - fileNameWidth;
  if (pathMaxWidth < 0) pathMaxWidth = 0;

  QString elidedPath;
  bool showPath = !dirPath.isEmpty() && pathMaxWidth > 8;
  if (showPath) {
    elidedPath = normalFm.elidedText(dirPath, Qt::ElideLeft, pathMaxWidth);
  }

  // If even the file name itself doesn't fit, elide it too
  int availableForName = textWidth;
  if (!showPath && boldFm.horizontalAdvance(fileName) > availableForName) {
    fileName = boldFm.elidedText(fileName, Qt::ElideRight, availableForName);
  }

  int yCenter = itemRect.center().y();

  // ── Draw file name (bold) ──
  painter->setFont(boldFont);
  painter->setPen(option.palette.color(QPalette::WindowText));
  QRect nameRect(textLeft, itemRect.top(), textWidth, itemRect.height());
  painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, fileName);

  // ── Draw path (normal, after filename) ──
  if (showPath) {
    painter->setFont(normalFont);
    painter->setPen(option.palette.color(QPalette::Disabled, QPalette::WindowText));
    int pathX = textLeft + fileNameWidth;
    int pathW = textRight - pathX;
    if (pathW > 0) {
      painter->drawText(QRect(pathX, itemRect.top(), pathW, itemRect.height()),
                        Qt::AlignLeft | Qt::AlignVCenter, elidedPath);
    }
  }

  // ── Close button ──
  if (itemHovered && close_button_visible_) {
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

    AppIconProvider::instance().icon(QStringLiteral("close"))
        .paint(painter, btnRect, Qt::AlignCenter, QIcon::Normal);
  }

  painter->restore();
}

QSize OpenFileDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
  Q_UNUSED(index)
  int h = qMax(option.fontMetrics.height() + 8, 24);
  return QSize(0, h);
}

bool OpenFileDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex& index) {
  auto* me = dynamic_cast<QMouseEvent*>(event);
  if (!me) {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }

  switch (me->type()) {
    case QEvent::MouseMove: {
      if (close_button_visible_) {
        bool overClose = closeButtonRect(option).contains(me->pos());
        model->setData(index, overClose, CloseBtnHoverRole);
      }
      break;
    }
    case QEvent::MouseButtonRelease: {
      if (close_button_visible_ && me->button() == Qt::LeftButton &&
          closeButtonRect(option).contains(me->pos())) {
        QString filePath = index.data(FilePathRole).toString();
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

QRect OpenFileDelegate::closeButtonRect(const QStyleOptionViewItem& option) {
  const int btnSize = 16;
  const int margin = 8;
  int x = option.rect.right() - btnSize - margin;
  int y = option.rect.center().y() - btnSize / 2;
  return QRect(x, y, btnSize, btnSize);
}

}  // namespace etest::app
