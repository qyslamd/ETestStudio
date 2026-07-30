#include "DevicePaletteWidget.h"

#include <QDrag>
#include <QFont>
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

  auto& pm = etest::core::plugin::PluginManager::instance();

  // 按 is_mock 分组：真实设备在前，Mock 设备在后
  QVector<etest::core::plugin::PluginMetaData> real_devices;
  QVector<etest::core::plugin::PluginMetaData> mock_devices;
  for (const auto& meta : pm.loadedPlugins()) {
    if (meta.category != "device") {
      continue;
    }
    if (meta.device_type.isEmpty()) {
      continue;
    }
    if (meta.is_mock) {
      mock_devices.append(meta);
    } else {
      real_devices.append(meta);
    }
  }

  // 分节标题项（不可选、不可拖拽）
  auto addHeader = [this](const QString& title) {
    auto* header = new QListWidgetItem(list_widget_);
    header->setText(title);
    header->setFlags(Qt::NoItemFlags);
    QFont font = header->font();
    font.setBold(true);
    header->setFont(font);
    header->setData(Qt::UserRole + 6, true);  // 标记为标题项（过滤时跳过）
  };

  // 设备项
  auto addDevice = [this](const etest::core::plugin::PluginMetaData& meta,
                          bool is_mock) {
    auto* item = new QListWidgetItem(list_widget_);
    QString base_name = meta.name.isEmpty() ? meta.device_type : meta.name;
    QString display = is_mock
        ? QStringLiteral("[Mock] %1 (%2ch)").arg(base_name).arg(meta.device_channels)
        : QStringLiteral("%1 (%2ch)").arg(base_name).arg(meta.device_channels);
    item->setText(display);
    item->setData(Qt::UserRole,     meta.device_type);
    item->setData(Qt::UserRole + 2, meta.device_channels);
    item->setData(Qt::UserRole + 3, meta.id);

    int direction = static_cast<int>(TopologyPort::Direction::Bidirectional);
    if (meta.device_direction == "Input") {
      direction = static_cast<int>(TopologyPort::Direction::Input);
    } else if (meta.device_direction == "Output") {
      direction = static_cast<int>(TopologyPort::Direction::Output);
    }
    item->setData(Qt::UserRole + 4, direction);

    FunctionType ft = stringToFunctionType(meta.device_function.isEmpty()
        ? QStringLiteral("CUSTOM") : meta.device_function);
    item->setData(Qt::UserRole + 5, static_cast<int>(ft));

    QString tip = QStringLiteral("%1\n%2ch %3 %4\n拖放至画布创建设备")
                      .arg(display)
                      .arg(meta.device_channels)
                      .arg(directionToString(static_cast<TopologyPort::Direction>(direction)))
                      .arg(functionTypeToString(ft));
    item->setToolTip(tip);
    item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
  };

  if (!real_devices.isEmpty()) {
    addHeader(QStringLiteral("真实设备"));
    for (const auto& meta : real_devices) {
      addDevice(meta, false);
    }
  }
  if (!mock_devices.isEmpty()) {
    addHeader(QStringLiteral("Mock 设备"));
    for (const auto& meta : mock_devices) {
      addDevice(meta, true);
    }
  }
}

void DevicePaletteWidget::onFilterChanged(const QString& text) {
  for (int i = 0; i < list_widget_->count(); ++i) {
    auto* item = list_widget_->item(i);
    if (item->data(Qt::UserRole + 6).toBool()) {
      continue;  // 跳过标题项
    }
    item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
  }
}

}  // namespace etest::topology
