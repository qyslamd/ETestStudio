#ifndef ETEST_PROGRAM_VERTICAL_TAB_LIST_DELEGATE_H_
#define ETEST_PROGRAM_VERTICAL_TAB_LIST_DELEGATE_H_

#include <QStyledItemDelegate>

namespace etest::app {

// 纵向标签列表自定义 role
enum VerticalTabRole {
  TabIndexRole = Qt::UserRole + 1,       // int: 对应 tab_widget_ 的索引
  ClosableRole = Qt::UserRole + 2,       // bool: 是否可关闭（仅用例 tab）
  CloseBtnHoverRole = Qt::UserRole + 3,  // bool: 关闭按钮悬停态
};

// 纵向标签列表 delegate：左 icon + 文字(elide) + 悬停时右侧关闭按钮。
// 选中/悬停背景交由 style 绘制（主题一致），关闭按钮点击发 closeRequested。
class VerticalTabListDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit VerticalTabListDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

 signals:
  // 用户点击某项的关闭按钮，参数为对应 tab 索引
  void closeRequested(int tabIndex);

 private:
  static QRect closeButtonRect(const QStyleOptionViewItem& option);
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_VERTICAL_TAB_LIST_DELEGATE_H_
