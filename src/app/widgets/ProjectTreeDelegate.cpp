#include "ProjectTreeDelegate.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QStyleOptionViewItem>

namespace etest::app {

// Role 常量（定义在 ProjectStructureWidget.h 的 ProjectNodeRole 枚举中）
static constexpr int kIsLatestRole = Qt::UserRole + 4;
static constexpr int kIsEffectiveTopologyRole = Qt::UserRole + 5;
static constexpr int kIsIcdConfigRole = Qt::UserRole + 6;
static constexpr int kIsMockConfigRole = Qt::UserRole + 7;

ProjectTreeDelegate::ProjectTreeDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void ProjectTreeDelegate::paint(QPainter* painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
  bool is_latest = index.data(kIsLatestRole).toBool();
  bool is_effective_topo = index.data(kIsEffectiveTopologyRole).toBool();
  bool is_icd_config = index.data(kIsIcdConfigRole).toBool();
  bool is_mock_config = index.data(kIsMockConfigRole).toBool();

  if (!is_latest && !is_effective_topo && !is_icd_config && !is_mock_config) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  // Reserve space on the right for the badge
  static const int kBadgeReserve = 48;
  QStyleOptionViewItem opt = option;
  opt.rect.setWidth(opt.rect.width() - kBadgeReserve);

  QStyledItemDelegate::paint(painter, opt, index);

  if (is_latest) {
    drawBadge(painter, option, tr("最新"), QColor(76, 175, 80));
  } else if (is_effective_topo) {
    drawBadge(painter, option, tr("生效"), QColor(33, 150, 243));
  } else if (is_icd_config) {
    drawBadge(painter, option, tr("配置"), QColor(255, 152, 0));
  } else if (is_mock_config) {
    drawBadge(painter, option, tr("Mock"), QColor(156, 39, 176));
  }
}

void ProjectTreeDelegate::drawBadge(QPainter* painter,
                                    const QStyleOptionViewItem& option,
                                    const QString& text,
                                    const QColor& color) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  static const int kPadX = 6;
  static const int kPadY = 4;
  static const int kRadius = 4;

  QFont badge_font = option.font;
  badge_font.setPointSize(8);
  badge_font.setBold(true);
  QFontMetrics fm(badge_font);

  int badge_w = fm.horizontalAdvance(text) + kPadX * 2;
  int badge_h = fm.height() + kPadY * 2;

  QRect badge_rect(option.rect.right() - badge_w - 4,
                   option.rect.top() + (option.rect.height() - badge_h) / 2,
                   badge_w, badge_h);

  bool is_dark = option.palette.color(QPalette::Base).lightness() < 128;

  painter->setPen(Qt::NoPen);
  painter->setBrush(is_dark ? QColor(color.red(), color.green(), color.blue(),
                                     50)
                            : QColor(color.red(), color.green(), color.blue(),
                                     30));
  painter->drawRoundedRect(badge_rect, kRadius, kRadius);

  painter->setFont(badge_font);
  painter->setPen(is_dark ? color.lighter(130) : color.darker(120));
  painter->drawText(badge_rect, Qt::AlignCenter, text);

  painter->restore();
}

}  // namespace etest::app
