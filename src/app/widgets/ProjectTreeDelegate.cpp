#include "ProjectTreeDelegate.h"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QToolTip>

#include "AppIconProvider.h"

namespace etest::app {

using etest::core_ui::AppIconProvider;

// Role 常量（与 ProjectStructureWidget.h 的 ProjectNodeRole 枚举对齐）
static constexpr int kNodeTypeRole = Qt::UserRole + 1;
static constexpr int kIsLatestRole = Qt::UserRole + 4;
static constexpr int kIsEffectiveTopologyRole = Qt::UserRole + 5;
static constexpr int kIsIcdConfigRole = Qt::UserRole + 6;
static constexpr int kIsMockConfigRole = Qt::UserRole + 7;

// 按钮布局常量
static constexpr int kBtnSize = 16;
static constexpr int kBtnSpacing = 4;
static constexpr int kBtnMarginRight = 8;
static constexpr int kRootBtnReserve =
    kBtnSize * 3 + kBtnSpacing * 2 + kBtnMarginRight;

ProjectTreeDelegate::ProjectTreeDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void ProjectTreeDelegate::paint(QPainter* painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
  bool is_root =
      index.data(kNodeTypeRole).toString() == QStringLiteral("root");

  if (is_root) {
    QStyleOptionViewItem opt = option;
    opt.rect.setWidth(opt.rect.width() - kRootBtnReserve);
    QStyledItemDelegate::paint(painter, opt, index);
    drawRootButtons(painter, option, index);
    return;
  }

  bool is_latest = index.data(kIsLatestRole).toBool();
  bool is_effective_topo = index.data(kIsEffectiveTopologyRole).toBool();
  bool is_icd_config = index.data(kIsIcdConfigRole).toBool();
  bool is_mock_config = index.data(kIsMockConfigRole).toBool();

  if (!is_latest && !is_effective_topo && !is_icd_config && !is_mock_config) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  static const int kBadgeReserve = 48;
  QStyleOptionViewItem opt = option;
  opt.rect.setWidth(opt.rect.width() - kBadgeReserve);
  QStyledItemDelegate::paint(painter, opt, index);

  if (is_latest) {
    drawBadge(painter, option, tr("最新"), QColor(76, 175, 80));
  } else if (is_effective_topo) {
    drawBadge(painter, option, tr("生效"), QColor(33, 150, 243));
  } else if (is_icd_config) {
    drawBadge(painter, option, tr("协议"), QColor(255, 152, 0));
  } else if (is_mock_config) {
    drawBadge(painter, option, tr("响应"), QColor(156, 39, 176));
  }
}

QSize ProjectTreeDelegate::sizeHint(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const {
  int h = qMax(option.fontMetrics.height() + 8, 24);
  if (index.data(kNodeTypeRole).toString() == QStringLiteral("root")) {
    h = qMax(h, 28);
  }
  return QSize(0, h);
}

bool ProjectTreeDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                      const QStyleOptionViewItem& option,
                                      const QModelIndex& index) {
  if (index.data(kNodeTypeRole).toString() != QStringLiteral("root")) {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }

  auto* me = dynamic_cast<QMouseEvent*>(event);
  if (!me) {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }

  // 参照 reference：进入时清除所有 hover 状态
  model->setData(index, false, kRefreshHoverRole);
  model->setData(index, false, kShowAllHoverRole);
  model->setData(index, false, kSyncDocHoverRole);

  switch (me->type()) {
    case QEvent::MouseMove: {
      RootButton btn = hitTestRootButton(option, me->pos());
      if (btn == RootButton::Refresh) {
        model->setData(index, true, kRefreshHoverRole);
      } else if (btn == RootButton::ShowAll) {
        model->setData(index, true, kShowAllHoverRole);
      } else if (btn == RootButton::SyncDoc) {
        model->setData(index, true, kSyncDocHoverRole);
      }
      break;
    }
    case QEvent::MouseButtonRelease: {
      if (me->button() != Qt::LeftButton) {
        break;
      }
      RootButton btn = hitTestRootButton(option, me->pos());
      if (btn == RootButton::Refresh) {
        emit refreshRequested();
      } else if (btn == RootButton::ShowAll) {
        show_all_ = !show_all_;
        emit showAllToggled(show_all_);
      } else if (btn == RootButton::SyncDoc) {
        emit syncRequested();
      }
      break;
    }
    default:
      break;
  }
  // 参照 reference：始终 return 基类
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

bool ProjectTreeDelegate::helpEvent(QHelpEvent* event,
                                    QAbstractItemView* view,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index) {
  if (index.data(kNodeTypeRole).toString() == QStringLiteral("root")) {
    RootButton btn = hitTestRootButton(option, event->pos());
    if (btn != RootButton::None) {
      QStringList tips = {tr("定位当前编辑文件"),   // SyncDoc=0
                          tr("显示/隐藏其他文件"),   // ShowAll=1
                          tr("刷新项目树")};         // Refresh=2
      QToolTip::showText(event->globalPos(),
                         tips[static_cast<int>(btn)]);
      return true;
    }
  }
  return QStyledItemDelegate::helpEvent(event, view, option, index);
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

void ProjectTreeDelegate::drawRootButtons(QPainter* painter,
                                          const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  auto isHovered = [&](RootButton btn) -> bool {
    switch (btn) {
      case RootButton::Refresh:
        return index.data(kRefreshHoverRole).toBool();
      case RootButton::ShowAll:
        return index.data(kShowAllHoverRole).toBool();
      case RootButton::SyncDoc:
        return index.data(kSyncDocHoverRole).toBool();
      default:
        return false;
    }
  };

  RootButton buttons[] = {RootButton::Refresh, RootButton::ShowAll,
                          RootButton::SyncDoc};
  for (int i = 0; i < 3; ++i) {
    RootButton btn = buttons[i];
    QRect rect = rootButtonRect(option, btn);

    if (isHovered(btn)) {
      painter->setPen(Qt::NoPen);
      QColor bg = option.palette.color(QPalette::Highlight);
      bg.setAlpha(40);
      painter->setBrush(bg);
      painter->drawRoundedRect(rect.adjusted(-2, -2, 2, 2), 4, 4);
    }

    QString icon_name;
    switch (btn) {
      case RootButton::Refresh:
        icon_name = QStringLiteral("refresh");
        break;
      case RootButton::ShowAll:
        icon_name = show_all_ ? QStringLiteral("eye")
                              : QStringLiteral("eye_closed");
        break;
      case RootButton::SyncDoc:
        icon_name = QStringLiteral("sync");
        break;
      default:
        break;
    }

    if (!icon_name.isEmpty()) {
      AppIconProvider::instance().icon(icon_name).paint(
          painter, rect, Qt::AlignCenter, QIcon::Normal);
    }
  }

  painter->restore();
}

QRect ProjectTreeDelegate::rootButtonRect(const QStyleOptionViewItem& option,
                                          RootButton btn) {
  int idx = static_cast<int>(btn);
  int x = option.rect.right() - kBtnMarginRight - kBtnSize -
          idx * (kBtnSize + kBtnSpacing);
  int y = option.rect.center().y() - kBtnSize / 2;
  return QRect(x, y, kBtnSize, kBtnSize);
}

ProjectTreeDelegate::RootButton ProjectTreeDelegate::hitTestRootButton(
    const QStyleOptionViewItem& option, const QPoint& pos) {
  for (int i = 0; i < 3; ++i) {
    RootButton btn = static_cast<RootButton>(i);
    if (rootButtonRect(option, btn).contains(pos)) {
      return btn;
    }
  }
  return RootButton::None;
}

}  // namespace etest::app
