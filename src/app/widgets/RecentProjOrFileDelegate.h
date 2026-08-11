#pragma once

#include <QStyledItemDelegate>

namespace etest::app {

// 最近项目/最近文件/已打开 列表项的自定义数据 role
enum RecentItemRole {
  FilePathRole = Qt::UserRole + 1,
  DirPathRole = Qt::UserRole + 2,
  CloseBtnHoverRole = Qt::UserRole + 3,  // bool: mouse over the close button
  TimeStrRole = Qt::UserRole + 4,        // QString: 最近项目的时间串（可空）
};

// 渲染两行条目的 delegate：第 1 行名称（主色粗体）+ 第 2 行路径（次要色小字），
// 属性开关控制是否显示右侧时间与 hover 关闭按钮，三个列表各取所需：
//   - 最近项目：showTime 开、关闭按钮关
//   - 最近文件：showTime 关、关闭按钮关
//   - 已打开：showTime 关、关闭按钮开
// hover/选中背景由 QSS（QListView#PhRecentList::item）提供，此处不重复绘制。
class RecentProjOrFileDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit RecentProjOrFileDelegate(QObject* parent = nullptr);

  void setCloseButtonVisible(bool visible);
  void setShowTime(bool show);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

 signals:
  // Emitted when the user clicks the close button on an item.
  void closeRequested(const QString& filePath);

 private:
  bool close_button_visible_ = true;
  bool show_time_ = false;
  static QRect closeButtonRect(const QStyleOptionViewItem& option);
};

}  // namespace etest::app
