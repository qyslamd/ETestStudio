#pragma once

#include <QWidget>

class QLineEdit;
class QTreeView;
class QStandardItemModel;

namespace etest::protocal {

class IcdNodeTreeWidget : public QWidget {
  Q_OBJECT
 public:
  explicit IcdNodeTreeWidget(QWidget* parent = nullptr);

 private:
  void initUi();
  void populatePlaceholderData();

  QLineEdit* filter_input_ = nullptr;
  QTreeView* tree_view_ = nullptr;
  QStandardItemModel* model_ = nullptr;
};

}  // namespace etest::protocal
