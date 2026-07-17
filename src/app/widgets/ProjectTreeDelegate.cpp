#include "ProjectTreeDelegate.h"

#include <QPainter>
#include <QStyleOptionViewItem>

namespace etest::app {

// kIsLatestRole 定义在 ProjectNodeRole 枚举中（ProjectStructureWidget.h）
// 值 = Qt::UserRole + 4
static constexpr int kIsLatestRole = Qt::UserRole + 4;

ProjectTreeDelegate::ProjectTreeDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void ProjectTreeDelegate::paint(QPainter* painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
  if (!index.data(kIsLatestRole).toBool()) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  // Reserve space on the right for the badge
  static const int kBadgeReserve = 48;
  QStyleOptionViewItem opt = option;
  opt.rect.setWidth(opt.rect.width() - kBadgeReserve);

  QStyledItemDelegate::paint(painter, opt, index);

  // Draw badge
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  static const int kPadX = 6;
  static const int kPadY = 4;
  static const int kRadius = 4;
  QString badge_text = tr("最新");

  QFont badge_font = option.font;
  badge_font.setPointSize(8);
  badge_font.setBold(true);
  QFontMetrics fm(badge_font);

  int badge_w = fm.horizontalAdvance(badge_text) + kPadX * 2;
  int badge_h = fm.height() + kPadY * 2;

  QRect badge_rect(option.rect.right() - badge_w - 4,
                   option.rect.top() + (option.rect.height() - badge_h) / 2,
                   badge_w, badge_h);

  bool is_dark =
      option.palette.color(QPalette::Base).lightness() < 128;

  painter->setPen(Qt::NoPen);
  painter->setBrush(is_dark ? QColor(76, 175, 80, 50)
                            : QColor(76, 175, 80, 30));
  painter->drawRoundedRect(badge_rect, kRadius, kRadius);

  painter->setFont(badge_font);
  painter->setPen(is_dark ? QColor(129, 199, 132) : QColor(56, 142, 60));
  painter->drawText(badge_rect, Qt::AlignCenter, badge_text);

  painter->restore();
}

}  // namespace etest::app
