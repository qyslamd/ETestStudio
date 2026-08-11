#pragma once

#include <QStyledItemDelegate>

class QAbstractItemModel;
class QEvent;
class QModelIndex;
class QPainter;
class QStyleOptionViewItem;

namespace etest::app {

// WelcomeV2 最近项目区条目 delegate：三行卡片（名称/路径/时间竖排，空时间不占
// 行），hover 出现右上移除按钮。背景自绘（ThemeManager），hover/选中明暗自适应。
// 数据 role 复用 RecentItemRole（RecentProjOrFileDelegate.h）。
class WelcomeRecentDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit WelcomeRecentDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

 signals:
  // 点击条目右上移除按钮
  void removeRequested(const QString& path);

 private:
  static QRect removeButtonRect(const QStyleOptionViewItem& option);
};

}  // namespace etest::app
