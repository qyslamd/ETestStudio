#pragma once

#include <QMetaObject>
#include <QWidget>

#include <icd/frame.hpp>
#include <icd/node.hpp>

class QLineEdit;
class QStandardItem;
class QStandardItemModel;
class QTreeView;

namespace icd {
class Repository;
}

namespace etest::protocal {

class IcdNodeTreeWidget : public QWidget {
  Q_OBJECT
 public:
  explicit IcdNodeTreeWidget(QWidget* parent = nullptr);

  void loadFromRepository(const icd::Repository& repo);
  void clear();

 signals:
  void frameSelected(const icd::Frame* frame);
  void nodeSelected(const icd::Node* node);

 private:
  void initUi();
  QStandardItem* createFrameItem(const icd::Frame& frame);
  QStandardItem* createNodeItem(const icd::Node& node);

  QLineEdit* filter_input_ = nullptr;
  QTreeView* tree_view_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QMetaObject::Connection selection_conn_;
};

}  // namespace etest::protocal
