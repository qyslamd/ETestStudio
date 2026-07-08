#ifndef ETEST_PROGRAM_VERTICAL_TAB_LIST_DELEGATE_H_
#define ETEST_PROGRAM_VERTICAL_TAB_LIST_DELEGATE_H_

#include <QStyledItemDelegate>

namespace etest::app {

// 纵向标签列表自定义 role
enum VerticalTabRole {
  TabIndexRole = Qt::UserRole + 1,  // int: 对应 tab_widget_ 的索引
};

// 纵向标签列表 delegate：左 icon + 文字(elide)。
// 选中/悬停背景交由 style 绘制（主题一致）。
class VerticalTabListDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit VerticalTabListDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_VERTICAL_TAB_LIST_DELEGATE_H_
