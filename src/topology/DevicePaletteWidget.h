#pragma once

#include <QListWidget>
#include <QWidget>

#include "TopologyDocument.h"

class QMimeData;

namespace etest::topology {

// Device type information used to populate the palette and create devices.
struct DeviceEntry {
  const char* deviceType;
  const char* displayName;
  int channelCount;
  TopologyPort::Direction direction;
  FunctionType functionType;
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

  DeviceListWidget* list_widget_ = nullptr;
};

}  // namespace etest::topology
