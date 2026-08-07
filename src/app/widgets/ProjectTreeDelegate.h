#pragma once

#include <QHelpEvent>
#include <QStyledItemDelegate>

namespace etest::app {

class ProjectTreeDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit ProjectTreeDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;
  bool helpEvent(QHelpEvent* event, QAbstractItemView* view,
                 const QStyleOptionViewItem& option,
                 const QModelIndex& index) override;

 signals:
  void refreshRequested();
  void showAllToggled(bool showAll);
  void syncRequested();

 public:
  bool isShowAll() const { return show_all_; }
  void resetShowAll(bool showAll) { show_all_ = showAll; }

 private:
  void drawBadge(QPainter* painter, const QStyleOptionViewItem& option,
                 const QString& text, const QColor& color) const;
  void drawRootButtons(QPainter* painter, const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;

  // SyncDoc=0 最右, ShowAll=1 中间, Refresh=2 最左
  enum class RootButton { SyncDoc = 0, ShowAll = 1, Refresh = 2, None = -1 };
  static QRect rootButtonRect(const QStyleOptionViewItem& option,
                              RootButton btn);
  static RootButton hitTestRootButton(const QStyleOptionViewItem& option,
                                      const QPoint& pos);

  // 每个按钮独立 hover role
  static constexpr int kRefreshHoverRole = Qt::UserRole + 10;
  static constexpr int kShowAllHoverRole = Qt::UserRole + 11;
  static constexpr int kSyncDocHoverRole = Qt::UserRole + 12;

  bool show_all_ = true;
};

}  // namespace etest::app
