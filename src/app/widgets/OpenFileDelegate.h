#ifndef ETEST_APP_OPEN_FILE_DELEGATE_H_
#define ETEST_APP_OPEN_FILE_DELEGATE_H_

#include <QStyledItemDelegate>

namespace etest::app {

// Custom data roles for the open-files model
enum OpenFileRole {
  FilePathRole = Qt::UserRole + 1,
  DirPathRole = Qt::UserRole + 2,
  CloseBtnHoverRole = Qt::UserRole + 3,  // bool: mouse over the close button
};

// Delegate that renders a single-line item ([bold filename] [normal path])
// with a close button that appears on hover and elides the path when needed.
class OpenFileDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit OpenFileDelegate(QObject* parent = nullptr);

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
  static QRect closeButtonRect(const QStyleOptionViewItem& option);
};

}  // namespace etest::app

#endif  // ETEST_APP_OPEN_FILE_DELEGATE_H_
