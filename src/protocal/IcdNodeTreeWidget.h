#pragma once

#include <QMetaObject>
#include <QWidget>

#include <icd/frame.hpp>
#include <icd/node.hpp>

class QLineEdit;
class QSortFilterProxyModel;
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

  void loadFromRepository(icd::Repository& repo);
  void clear();
  void selectNode(const icd::Node* node);
  void revealNode(const icd::Node* node);

 signals:
  void frameSelected(icd::Frame* frame);
  void nodeSelected(icd::Node* node);
  void addFrameRequested();
  void deleteFrameRequested(int frameId);
  void addNodeRequested(int frameId, const icd::Node* parent = nullptr);
  void deleteNodeRequested(int frameId, const icd::Node* node);

 private:
  void initUi();
  void onContextMenu(const QPoint& pos);
  void applyFilter(const QString& text);
  QStandardItem* createFrameItem(icd::Frame& frame);
  QStandardItem* createNodeItem(icd::Node& node, int frameId = -1);

  QLineEdit* filter_input_ = nullptr;
  QTreeView* tree_view_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QSortFilterProxyModel* proxy_ = nullptr;
  QMetaObject::Connection selection_conn_;
};

}  // namespace etest::protocal
