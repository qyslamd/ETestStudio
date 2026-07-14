#include "DevicePaletteWidget.h"

#include <QDrag>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMimeData>
#include <QVBoxLayout>

#include "plugin_sdk/PluginManager.h"

namespace etest::topology {

// MIME type used when dragging devices from the palette.
static const char kTopologyDeviceMime[] = "application/x-topology-device";

// ── DeviceListWidget ─────────────────────────────────────────

DeviceListWidget::DeviceListWidget(QWidget* parent) : QListWidget(parent) {
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setFrameShape(QFrame::NoFrame);
}

void DeviceListWidget::startDrag(Qt::DropActions supportedActions) {
  auto items = selectedItems();
  if (items.isEmpty())
    return;

  auto* item = items.first();

  QJsonObject obj;
  obj["deviceType"]    = item->data(Qt::UserRole).toString();
  obj["isMonitor"]     = item->data(Qt::UserRole + 1).toBool();
  obj["channelCount"]  = item->data(Qt::UserRole + 2).toInt();
  obj["pluginId"]      = item->data(Qt::UserRole + 3).toString();
  obj["direction"]     = item->data(Qt::UserRole + 4).toInt();
  obj["functionType"]  = item->data(Qt::UserRole + 5).toInt();

  auto* mime = new QMimeData();
  mime->setData(QLatin1String(kTopologyDeviceMime),
                QJsonDocument(obj).toJson(QJsonDocument::Compact));

  auto* drag = new QDrag(this);
  drag->setMimeData(mime);
  drag->exec(supportedActions);
}

// ── DevicePaletteWidget ──────────────────────────────────────

DevicePaletteWidget::DevicePaletteWidget(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  filter_input_ = new QLineEdit(this);
  filter_input_->setPlaceholderText(QStringLiteral("搜索设备类型..."));
  filter_input_->setClearButtonEnabled(true);
  layout->addWidget(filter_input_);

  list_widget_ = new DeviceListWidget(this);
  layout->addWidget(list_widget_);

  // Connect plugin load/unload to refresh the device list
  auto& pm = etest::core::plugin::PluginManager::instance();
  connect(&pm, &etest::core::plugin::PluginManager::pluginLoaded,
          this, &DevicePaletteWidget::populateDeviceTypes);
  connect(&pm, &etest::core::plugin::PluginManager::pluginUnloaded,
          this, &DevicePaletteWidget::populateDeviceTypes);

  // Initial population (also used as refresh target)
  populateDeviceTypes();

  connect(filter_input_, &QLineEdit::textChanged, this,
          &DevicePaletteWidget::onFilterChanged);
}

void DevicePaletteWidget::populateDeviceTypes() {
  list_widget_->clear();

  // Load device types from PluginManager
  auto& pm = etest::core::plugin::PluginManager::instance();
  for (const auto& meta : pm.loadedPlugins()) {
    if (meta.category != "device") continue;
    if (meta.device_type.isEmpty()) continue;

    auto* item = new QListWidgetItem(list_widget_);
    QString display = meta.name.isEmpty()
        ? meta.device_type
        : QStringLiteral("%1 (%2ch)").arg(meta.name).arg(meta.device_channels);
    item->setText(display);
    item->setData(Qt::UserRole,     meta.device_type);          // deviceType
    item->setData(Qt::UserRole + 1, QVariant(false));            // isMonitor
    item->setData(Qt::UserRole + 2, meta.device_channels);       // channelCount
    item->setData(Qt::UserRole + 3, meta.id);                    // pluginId

    int direction = static_cast<int>(TopologyPort::Direction::Bidirectional);
    if (meta.device_direction == "Input")
      direction = static_cast<int>(TopologyPort::Direction::Input);
    else if (meta.device_direction == "Output")
      direction = static_cast<int>(TopologyPort::Direction::Output);
    item->setData(Qt::UserRole + 4, direction);                  // direction

    FunctionType ft = stringToFunctionType(meta.device_function.isEmpty()
        ? QStringLiteral("CUSTOM") : meta.device_function);
    item->setData(Qt::UserRole + 5, static_cast<int>(ft));       // functionType

    QString tip = QStringLiteral("%1\n%2ch %3 %4\n拖放至画布创建设备")
                      .arg(display)
                      .arg(meta.device_channels)
                      .arg(directionToString(static_cast<TopologyPort::Direction>(direction)))
                      .arg(functionTypeToString(ft));
    item->setToolTip(tip);
    item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
  }

  // Add the hard-coded monitor entry
  addMonitorEntry();
}

void DevicePaletteWidget::addMonitorEntry() {
  static const MonitorEntry kMonitorTypes[] = {
      {"Monitor-4CH", "Monitor-4CH (4通道监听器)", 4},
  };
  static const int kMonitorTypeCount =
      sizeof(kMonitorTypes) / sizeof(kMonitorTypes[0]);

  if (kMonitorTypeCount > 0) {
    auto* sep = new QListWidgetItem(QStringLiteral("─── 监听器 ───"));
    sep->setFlags(sep->flags() & ~Qt::ItemIsSelectable);
    sep->setForeground(QColor(140, 140, 140));
    list_widget_->addItem(sep);

    for (int i = 0; i < kMonitorTypeCount; ++i) {
      const auto& entry = kMonitorTypes[i];
      auto* item = new QListWidgetItem(entry.displayName);
      item->setData(Qt::UserRole, entry.deviceType);
      item->setData(Qt::UserRole + 1, true);                    // isMonitor
      item->setData(Qt::UserRole + 2, entry.channelCount);      // channelCount
      item->setData(Qt::UserRole + 3, QString());                // pluginId（空）
      item->setData(Qt::UserRole + 4,
          static_cast<int>(TopologyPort::Direction::Bidirectional)); // direction
      item->setData(Qt::UserRole + 5,
          static_cast<int>(FunctionType::CUSTOM));               // functionType
      item->setToolTip(QStringLiteral("%1\n拖放至画布添加监听器").arg(
          entry.displayName));
      item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
      list_widget_->addItem(item);
    }
  }
}

void DevicePaletteWidget::onFilterChanged(const QString& text) {
  for (int i = 0; i < list_widget_->count(); ++i) {
    auto* item = list_widget_->item(i);
    item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
  }
}

}  // namespace etest::topology
