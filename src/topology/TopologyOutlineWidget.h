#pragma once

#include <QSet>
#include <QString>
#include <QWidget>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace etest::topology {

class TopologyDocument;

class TopologyOutlineWidget : public QWidget {
  Q_OBJECT
 public:
  explicit TopologyOutlineWidget(QWidget* parent = nullptr);

  void rebuildTree(TopologyDocument* doc);

  void selectForItem(int itemType, int mainIndex, int subIndex);
  void clearSelection();

 signals:
  // itemType: 0=UUT, 1=Device, 2=Connection, 3=Port, 4=DevicePort
  void navigateRequested(int itemType, int mainIndex, int subIndex);

 private slots:
  void onFilterTextChanged(const QString& text);
  void onTreeItemClicked(QTreeWidgetItem* item, int column);

 private:
  // Values align with onOutlineNavigate scheme: 0=UUT, 1=Device, 2=Connection, 3=Port, 4=DevicePort
  enum class ItemTag { Category = -1, Uut = 0, Device = 1, Connection = 2, Port = 3, DevicePort = 4 };

  static constexpr int kRoleTag = Qt::UserRole + 1;
  static constexpr int kRoleMainIdx = Qt::UserRole + 2;
  static constexpr int kRoleSubIdx = Qt::UserRole + 3;

  QTreeWidgetItem* addCategoryItem(const QString& label);
  void addUutItem(int index, TopologyDocument* doc, QTreeWidgetItem* parent);
  void addDeviceItem(int index, TopologyDocument* doc, QTreeWidgetItem* parent);
  void addConnectionItem(int index, TopologyDocument* doc,
                         QTreeWidgetItem* parent);
  bool applyFilter(QTreeWidgetItem* item, const QString& filter);
  void saveExpandedState();
  void restoreExpandedState();

  QLineEdit* filter_input_ = nullptr;
  QSet<QString> expanded_keys_;
  QTreeWidget* tree_ = nullptr;
  bool updating_selection_ = false;
};

}  // namespace etest::topology
