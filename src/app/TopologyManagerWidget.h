#ifndef ETEST_APP_TOPOLOGY_MANAGER_WIDGET_H_
#define ETEST_APP_TOPOLOGY_MANAGER_WIDGET_H_

#include <QString>
#include <QWidget>

class QLineEdit;
class QPushButton;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

namespace etest::app {

class TopologyManagerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit TopologyManagerWidget(QWidget* parent = nullptr);

 public slots:
  void refreshList();

 signals:
  void openFileRequested(const QString& filePath);

 private:
  void initUi();
  void initSignals();

  // 树操作
  void onItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onCustomContextMenu(const QPoint& pos);

  // 文件操作
  void onNewTopology();
  bool renameTopologyFile(const QString& oldPath);
  bool removeTopologyFile(const QString& filePath);

  // 解析：读取 .etopo JSON，生成预览子节点
  void addPreviewNodes(QTreeWidgetItem* fileItem, const QString& absPath);

  // 搜索
  void applySearchFilter(const QString& keyword);

  QLineEdit* search_edit_ = nullptr;
  QTimer* search_timer_ = nullptr;
  QPushButton* new_btn_ = nullptr;
  QTreeWidget* tree_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_TOPOLOGY_MANAGER_WIDGET_H_
