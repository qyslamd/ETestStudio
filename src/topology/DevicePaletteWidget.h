#pragma once

#include <QListWidget>
#include <QWidget>

#include "TopologyDocument.h"

class QLineEdit;
class QMimeData;

namespace etest::topology {

// Monitored device entry (kept hard-coded — monitors aren't plugin-loaded)
struct MonitorEntry {
  QString deviceType;
  QString displayName;
  int channelCount;
};

// Subclassed to provide custom MIME data for drag operations.
class DeviceListWidget : public QListWidget {
  Q_OBJECT
 public:
  explicit DeviceListWidget(QWidget* parent = nullptr);

 protected:
  void startDrag(Qt::DropActions supportedActions) override;
};

// Panel showing device types that can be dragged onto the topology scene.
class DevicePaletteWidget : public QWidget {
  Q_OBJECT
 public:
  explicit DevicePaletteWidget(QWidget* parent = nullptr);

 private:
  void populateDeviceTypes();
  void addMonitorEntry();
  void onFilterChanged(const QString& text);

  DeviceListWidget* list_widget_ = nullptr;
  QLineEdit* filter_input_ = nullptr;
};

}  // namespace etest::topology
