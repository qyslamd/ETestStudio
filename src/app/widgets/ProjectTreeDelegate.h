#ifndef ETEST_APP_PROJECT_TREE_DELEGATE_H_
#define ETEST_APP_PROJECT_TREE_DELEGATE_H_

#include <QStyledItemDelegate>

namespace etest::app {

class ProjectTreeDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit ProjectTreeDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

 private:
  void drawBadge(QPainter* painter, const QStyleOptionViewItem& option,
                 const QString& text, const QColor& color) const;
};

}  // namespace etest::app

#endif  // ETEST_APP_PROJECT_TREE_DELEGATE_H_
