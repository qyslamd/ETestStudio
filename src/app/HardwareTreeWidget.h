#ifndef ETEST_APP_HARDWARE_TREE_WIDGET_H_
#define ETEST_APP_HARDWARE_TREE_WIDGET_H_

#include <QMap>
#include <QTimer>
#include <QTreeWidget>
#include <QWidget>
#include "IDevicePlugin.h"

namespace etest::app {

class HardwareTreeWidget : public QWidget {
  Q_OBJECT

 public:
  explicit HardwareTreeWidget(QWidget* parent = nullptr);
  ~HardwareTreeWidget() override;

  void refreshTree();
  void highlightDeviceType(const QString& deviceType,
                           const QString& pluginId);

 private:
  void setupUi();
  void initSignals();

  void onItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onCustomContextMenu(const QPoint& pos);
  void updateDeviceStatus();

  QString deviceTypeDisplayName(const QString& deviceType) const;
  QString statusText(etest::core::plugin::DeviceStatus status) const;

  QTreeWidget* tree_ = nullptr;
  QTimer* status_timer_ = nullptr;

  // 存储pluginId到设备节点的映射，方便状态刷新
  QMap<QString, QTreeWidgetItem*> device_items_;
};

}  // namespace etest::app

#endif  // ETEST_APP_HARDWARE_TREE_WIDGET_H_
