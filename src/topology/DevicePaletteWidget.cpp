#include "DevicePaletteWidget.h"

#include <QDrag>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMimeData>
#include <QVBoxLayout>

namespace etest::topology {

// MIME type used when dragging devices from the palette.
static const char kTopologyDeviceMime[] = "application/x-topology-device";

// ── Device palette entries ───────────────────────────────────

static const DeviceEntry kDeviceTypes[] = {
    {"EPH6272T", "EPH6272T ARINC429 4CH", 4, TopologyPort::Direction::Bidirectional,
     FunctionType::A429},
    {"EPH6633A", "EPH6633A Analog 8CH", 8, TopologyPort::Direction::Bidirectional,
     FunctionType::AD},
    {"EPH5121A", "EPH5121A Discrete 32CH", 32, TopologyPort::Direction::Bidirectional,
     FunctionType::DISCRETE},
};

static const int kDeviceTypeCount =
    sizeof(kDeviceTypes) / sizeof(kDeviceTypes[0]);

// ── Monitor palette entries ──────────────────────────────────

struct MonitorEntry {
  QString deviceType;
  QString displayName;
  int channelCount;
};

static const MonitorEntry kMonitorTypes[] = {
    {"Monitor-4CH", "Monitor-4CH (4通道监听器)", 4},
};

static const int kMonitorTypeCount =
    sizeof(kMonitorTypes) / sizeof(kMonitorTypes[0]);

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

  // Build JSON with full device entry for the selected item
  QString dt = items.first()->data(Qt::UserRole).toString();

  QJsonObject obj;
  obj["deviceType"] = dt;

  // Check if this is a monitor item
  bool isMonitor = items.first()->data(Qt::UserRole + 1).toBool();
  if (isMonitor) {
    obj["isMonitor"] = true;
    for (int i = 0; i < kMonitorTypeCount; ++i) {
      if (dt == kMonitorTypes[i].deviceType) {
        obj["channelCount"] = kMonitorTypes[i].channelCount;
        break;
      }
    }
  } else {
    for (int i = 0; i < kDeviceTypeCount; ++i) {
      if (dt == kDeviceTypes[i].deviceType) {
        obj["channelCount"] = kDeviceTypes[i].channelCount;
        obj["direction"] = static_cast<int>(kDeviceTypes[i].direction);
        obj["functionType"] = static_cast<int>(kDeviceTypes[i].functionType);
        break;
      }
    }
  }

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

  populateDeviceTypes();

  connect(filter_input_, &QLineEdit::textChanged, this,
          &DevicePaletteWidget::onFilterChanged);
}

void DevicePaletteWidget::populateDeviceTypes() {
  for (int i = 0; i < kDeviceTypeCount; ++i) {
    const auto& entry = kDeviceTypes[i];

    auto* item = new QListWidgetItem(entry.displayName);
    item->setData(Qt::UserRole, entry.deviceType);

    QString tip =
        QStringLiteral("%1\n%2ch %3 %4\n拖放至画布创建设备")
            .arg(entry.displayName)
            .arg(entry.channelCount)
            .arg(directionToString(entry.direction))
            .arg(functionTypeToString(entry.functionType));
    item->setToolTip(tip);

    item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
    list_widget_->addItem(item);
  }

  // ── Monitor section ──
  if (kMonitorTypeCount > 0) {
    auto* sep = new QListWidgetItem(QStringLiteral("─── 监听器 ───"));
    sep->setFlags(sep->flags() & ~Qt::ItemIsSelectable);
    sep->setForeground(QColor(140, 140, 140));
    list_widget_->addItem(sep);

    for (int i = 0; i < kMonitorTypeCount; ++i) {
      const auto& entry = kMonitorTypes[i];
      auto* item = new QListWidgetItem(entry.displayName);
      item->setData(Qt::UserRole, entry.deviceType);
      item->setData(Qt::UserRole + 1, true);  // isMonitor flag
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
