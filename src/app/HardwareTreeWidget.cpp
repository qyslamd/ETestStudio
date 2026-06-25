#include "HardwareTreeWidget.h"

#include <QHeaderView>
#include <QMenu>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include "IDevicePlugin.h"
#include "PluginManager.h"

namespace etest::app {

using namespace etest::core::plugin;

HardwareTreeWidget::HardwareTreeWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
  initSignals();
}

HardwareTreeWidget::~HardwareTreeWidget() {
  if (status_timer_) {
    status_timer_->stop();
  }
}

void HardwareTreeWidget::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  tree_ = new QTreeWidget(this);
  tree_->setHeaderHidden(true);
  tree_->setRootIsDecorated(true);
  tree_->setIndentation(16);
  tree_->setAnimated(false);
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_->header()->setStretchLastSection(true);

  layout->addWidget(tree_);

  // 状态轮询定时器
  status_timer_ = new QTimer(this);
  status_timer_->setInterval(2000);
}

void HardwareTreeWidget::initSignals() {
  connect(tree_, &QTreeWidget::itemDoubleClicked, this,
          &HardwareTreeWidget::onItemDoubleClicked);
  connect(tree_, &QTreeWidget::customContextMenuRequested, this,
          &HardwareTreeWidget::onCustomContextMenu);
  connect(status_timer_, &QTimer::timeout, this,
          &HardwareTreeWidget::updateDeviceStatus);
}

void HardwareTreeWidget::refreshTree() {
  tree_->clear();
  device_items_.clear();

  auto& pm = PluginManager::instance();
  QList<PluginMetaData> allPlugins = pm.loadedPlugins();

  // 筛选设备类插件，按厂家和分类分组
  // 结构: manufacturer -> device_type -> [pluginId, meta]
  QMap<QString, QMap<QString, QList<QPair<QString, PluginMetaData>>>> groups;

  for (const auto& meta : allPlugins) {
    if (meta.category != "device" || meta.device_type.isEmpty()) continue;

    // 需要获取DeviceInfo中的manufacturer
    IDevicePlugin* device = pm.pluginAs<IDevicePlugin>(meta.id);
    if (!device) continue;

    QString manufacturer = device->deviceInfo().manufacturer;
    if (manufacturer.isEmpty()) manufacturer = QStringLiteral("未知厂家");

    groups[manufacturer][meta.device_type].append({meta.id, meta});
  }

  for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
    // 一级节点：厂家
    auto* manufacturerItem = new QTreeWidgetItem(tree_);
    manufacturerItem->setText(0, it.key());
    manufacturerItem->setData(0, Qt::UserRole, QStringLiteral("manufacturer"));
    manufacturerItem->setExpanded(true);

    const auto& typeGroups = it.value();
    for (auto typeIt = typeGroups.constBegin(); typeIt != typeGroups.constEnd();
         ++typeIt) {
      // 二级节点：设备分类
      auto* typeItem = new QTreeWidgetItem(manufacturerItem);
      typeItem->setText(0, deviceTypeDisplayName(typeIt.key()));
      typeItem->setData(0, Qt::UserRole, QStringLiteral("device_type"));
      typeItem->setExpanded(true);

      const auto& devices = typeIt.value();
      for (const auto& pair : devices) {
        // 三级节点：设备实例
        IDevicePlugin* device = pm.pluginAs<IDevicePlugin>(pair.first);
        if (!device) continue;

        auto* deviceItem = new QTreeWidgetItem(typeItem);
        deviceItem->setText(
            0, pair.second.name + QStringLiteral(" ") +
                   statusText(device->deviceStatus()));
        deviceItem->setData(0, Qt::UserRole, QStringLiteral("device"));
        deviceItem->setData(0, Qt::UserRole + 1, pair.first);

        device_items_[pair.first] = deviceItem;
      }
    }
  }

  tree_->expandAll();
  status_timer_->start();
}

void HardwareTreeWidget::highlightDeviceType(const QString& deviceType,
                                              const QString& pluginId) {
  // Iterate all leaf (device-level) items to find a match
  QTreeWidgetItemIterator it(tree_);
  while (*it) {
    if ((*it)->data(0, Qt::UserRole).toString() == QLatin1String("device")) {
      QString itemPluginId = (*it)->data(0, Qt::UserRole + 1).toString();
      if (itemPluginId == pluginId) {
        tree_->setCurrentItem(*it);
        tree_->scrollToItem(*it);
        return;
      }
    }
    ++it;
  }
  // Fallback: try matching by device type display name
  QTreeWidgetItemIterator it2(tree_);
  while (*it2) {
    if ((*it2)->data(0, Qt::UserRole).toString() == QLatin1String("device_type")) {
      if ((*it2)->text(0).contains(deviceType, Qt::CaseInsensitive)) {
        tree_->setCurrentItem(*it2);
        tree_->scrollToItem(*it2);
        return;
      }
    }
    ++it2;
  }
}

void HardwareTreeWidget::onItemDoubleClicked(QTreeWidgetItem* item,
                                             int column) {
  Q_UNUSED(column);

  if (item->data(0, Qt::UserRole).toString() != "device") return;

  QString pluginId = item->data(0, Qt::UserRole + 1).toString();
  auto& pm = PluginManager::instance();
  IDevicePlugin* device = pm.pluginAs<IDevicePlugin>(pluginId);
  if (!device) return;

  if (device->deviceStatus() == DeviceStatus::Offline) {
    device->openDevice();
  } else {
    device->closeDevice();
  }

  item->setText(0, device->metaData().name + QStringLiteral(" ") +
                       statusText(device->deviceStatus()));
}

void HardwareTreeWidget::onCustomContextMenu(const QPoint& pos) {
  QTreeWidgetItem* item = tree_->itemAt(pos);
  if (!item || item->data(0, Qt::UserRole).toString() != "device") return;

  QString pluginId = item->data(0, Qt::UserRole + 1).toString();
  auto& pm = PluginManager::instance();
  IDevicePlugin* device = pm.pluginAs<IDevicePlugin>(pluginId);
  if (!device) return;

  auto* menu = new QMenu(this);

  if (device->deviceStatus() == DeviceStatus::Offline) {
    auto* openAction = menu->addAction(QStringLiteral("打开设备"));
    connect(openAction, &QAction::triggered, this, [this, device, item]() {
      device->openDevice();
      item->setText(0, device->metaData().name + QStringLiteral(" ") +
                           statusText(device->deviceStatus()));
    });
  } else {
    auto* closeAction = menu->addAction(QStringLiteral("关闭设备"));
    connect(closeAction, &QAction::triggered, this, [this, device, item]() {
      device->closeDevice();
      item->setText(0, device->metaData().name + QStringLiteral(" ") +
                           statusText(device->deviceStatus()));
    });
  }

  menu->addSeparator();
  auto* refreshAction = menu->addAction(QStringLiteral("刷新"));
  connect(refreshAction, &QAction::triggered, this,
          &HardwareTreeWidget::refreshTree);

  menu->exec(tree_->mapToGlobal(pos));
  menu->deleteLater();
}

void HardwareTreeWidget::updateDeviceStatus() {
  auto& pm = PluginManager::instance();

  for (auto it = device_items_.constBegin(); it != device_items_.constEnd();
       ++it) {
    IDevicePlugin* device = pm.pluginAs<IDevicePlugin>(it.key());
    if (!device) continue;

    QTreeWidgetItem* item = it.value();
    item->setText(0, device->metaData().name + QStringLiteral(" ") +
                         statusText(device->deviceStatus()));
  }
}

QString HardwareTreeWidget::deviceTypeDisplayName(
    const QString& deviceType) const {
  static const QMap<QString, QString> names = {
      {"ad", QStringLiteral("AD采集")},
      {"da", QStringLiteral("DA输出")},
      {"serial", QStringLiteral("串口")},
      {"a429", QStringLiteral("A429")},
      {"can", QStringLiteral("CAN")}};

  return names.value(deviceType, deviceType);
}

QString HardwareTreeWidget::statusText(etest::core::plugin::DeviceStatus status) const {
  switch (status) {
    case DeviceStatus::Online:
      return QStringLiteral("[在线]");
    case DeviceStatus::Error:
      return QStringLiteral("[错误]");
    case DeviceStatus::Offline:
    default:
      return QStringLiteral("[离线]");
  }
}

}  // namespace etest::app
