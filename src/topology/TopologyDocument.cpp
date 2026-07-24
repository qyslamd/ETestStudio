#include "TopologyDocument.h"

namespace etest::topology {

QString functionTypeToString(FunctionType t) {
  switch (t) {
    case FunctionType::A429:
      return QStringLiteral("A429");
    case FunctionType::AD:
      return QStringLiteral("AD");
    case FunctionType::DA:
      return QStringLiteral("DA");
    case FunctionType::DISCRETE:
      return QStringLiteral("DISCRETE");
    case FunctionType::SERIAL:
      return QStringLiteral("SERIAL");
    case FunctionType::MIL1553:
      return QStringLiteral("MIL1553");
    case FunctionType::POWER:
      return QStringLiteral("POWER");
    case FunctionType::CAMERA:
      return QStringLiteral("CAMERA");
    case FunctionType::OSCILLOSCOPE:
      return QStringLiteral("OSCILLOSCOPE");
    case FunctionType::CUSTOM:
      return QStringLiteral("CUSTOM");
  }
  return QStringLiteral("CUSTOM");
}

FunctionType stringToFunctionType(const QString& s) {
  if (s == QStringLiteral("A429"))
    return FunctionType::A429;
  if (s == QStringLiteral("AD"))
    return FunctionType::AD;
  if (s == QStringLiteral("DA"))
    return FunctionType::DA;
  if (s == QStringLiteral("DISCRETE"))
    return FunctionType::DISCRETE;
  if (s == QStringLiteral("SERIAL"))
    return FunctionType::SERIAL;
  if (s == QStringLiteral("MIL1553"))
    return FunctionType::MIL1553;
  if (s == QStringLiteral("POWER"))
    return FunctionType::POWER;
  if (s == QStringLiteral("CAMERA"))
    return FunctionType::CAMERA;
  if (s == QStringLiteral("OSCILLOSCOPE"))
    return FunctionType::OSCILLOSCOPE;
  return FunctionType::CUSTOM;
}

QString directionToString(TopologyPort::Direction d) {
  switch (d) {
    case TopologyPort::Direction::Input:
      return QStringLiteral("Input");
    case TopologyPort::Direction::Output:
      return QStringLiteral("Output");
    case TopologyPort::Direction::Bidirectional:
      return QStringLiteral("Bidirectional");
  }
  return QStringLiteral("Output");
}

TopologyPort::Direction stringToDirection(const QString& s) {
  QString lower = s.toLower();
  if (lower == QStringLiteral("input"))
    return TopologyPort::Direction::Input;
  if (lower == QStringLiteral("bidirectional"))
    return TopologyPort::Direction::Bidirectional;
  return TopologyPort::Direction::Output;
}

TopologyDocument::TopologyDocument(QObject* parent)
    : QObject(parent), undo_stack_(new QUndoStack(this)) {}

static bool isDirectionCompatible(TopologyPort::Direction portDir,
                                   TopologyPort::Direction devPortDir) {
  // Bidirectional is compatible with any direction
  if (portDir == TopologyPort::Direction::Bidirectional ||
      devPortDir == TopologyPort::Direction::Bidirectional)
    return true;
  // Opposites connect: Input ↔ Output
  return portDir != devPortDir;
}

bool TopologyDocument::isModified() const {
  return !undo_stack_->isClean();
}

int TopologyDocument::addProduct(const TopologyProduct& product) {
  int index = products_.size();
  products_.append(product);
  emit productAdded(index);
  return index;
}

int TopologyDocument::insertProduct(int index,
                                    const TopologyProduct& product) {
  if (index < 0 || index > products_.size())
    return -1;
  products_.insert(index, product);
  emit productAdded(index);
  return index;
}

void TopologyDocument::removeProduct(int index) {
  if (index < 0 || index >= products_.size())
    return;
  products_.removeAt(index);
  emit productRemoved(index);
}

TopologyProduct* TopologyDocument::product(int index) {
  if (index < 0 || index >= products_.size())
    return nullptr;
  return &products_[index];
}

const TopologyProduct* TopologyDocument::product(int index) const {
  if (index < 0 || index >= products_.size())
    return nullptr;
  return &products_[index];
}

int TopologyDocument::productCount() const {
  return products_.size();
}

int TopologyDocument::findProductIndex(const QString& name) const {
  for (int i = 0; i < products_.size(); ++i) {
    if (products_[i].name == name)
      return i;
  }
  return -1;
}

bool TopologyDocument::renameProduct(int index, const QString& newName) {
  auto* product = this->product(index);
  if (!product)
    return false;
  const QString oldName = product->name;
  product->name = newName;
  for (auto& connection : connections_) {
    if (connection.productName == oldName)
      connection.productName = newName;
  }
  for (auto& monitor : monitors_) {
    for (auto& tap : monitor.taps) {
      if (tap.productName == oldName)
        tap.productName = newName;
    }
  }
  emit productChanged(index);
  return true;
}

int TopologyDocument::addDevice(const TopologyDevice& device) {
  int index = devices_.size();
  TopologyDevice dev = device;
  // M2: id 为空时自动生成 UUID v4
  if (dev.id.isEmpty()) {
    dev.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  devices_.append(dev);
  emit deviceAdded(index);
  return index;
}

int TopologyDocument::insertDevice(int index, const TopologyDevice& device) {
  if (index < 0 || index > devices_.size())
    return -1;
  TopologyDevice dev = device;
  if (dev.id.isEmpty()) {
    dev.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  devices_.insert(index, dev);
  emit deviceAdded(index);
  return index;
}

void TopologyDocument::removeDevice(int index) {
  if (index < 0 || index >= devices_.size())
    return;
  devices_.removeAt(index);
  emit deviceRemoved(index);
}

TopologyDevice* TopologyDocument::device(int index) {
  if (index < 0 || index >= devices_.size())
    return nullptr;
  return &devices_[index];
}

const TopologyDevice* TopologyDocument::device(int index) const {
  if (index < 0 || index >= devices_.size())
    return nullptr;
  return &devices_[index];
}

int TopologyDocument::deviceCount() const {
  return devices_.size();
}

int TopologyDocument::findDeviceIndex(const QString& name) const {
  for (int i = 0; i < devices_.size(); ++i) {
    if (devices_[i].name == name)
      return i;
  }
  return -1;
}

int TopologyDocument::findDeviceIndexById(const QString& id) const {
  for (int i = 0; i < devices_.size(); ++i) {
    if (devices_[i].id == id)
      return i;
  }
  return -1;
}

void TopologyDocument::setDevicePortFrames(int deviceIndex, int portIndex,
                                           const QStringList& frames) {
  auto* dev = device(deviceIndex);
  if (!dev) return;
  if (portIndex < 0 || portIndex >= dev->ports.size()) return;
  dev->ports[portIndex].boundFrameNames = frames;
  emit devicePortFramesChanged(deviceIndex, portIndex);
}

QStringList TopologyDocument::devicePortFrames(int deviceIndex,
                                               int portIndex) const {
  const auto* dev = device(deviceIndex);
  if (!dev) return {};
  if (portIndex < 0 || portIndex >= dev->ports.size()) return {};
  return dev->ports[portIndex].boundFrameNames;
}

bool TopologyDocument::renameDevice(int index, const QString& newName) {
  auto* dev = device(index);
  if (!dev)
    return false;
  const QString oldName = dev->name;
  dev->name = newName;
  for (auto& connection : connections_) {
    if (connection.deviceName == oldName)
      connection.deviceName = newName;
  }
  for (auto& monitor : monitors_) {
    for (auto& tap : monitor.taps) {
      if (tap.deviceName == oldName)
        tap.deviceName = newName;
    }
  }
  emit deviceChanged(index);
  return true;
}

void TopologyDocument::addDevicePort(int deviceIndex,
                                     const TopologyDevicePort& port) {
  if (deviceIndex < 0 || deviceIndex >= devices_.size())
    return;
  int portIndex = devices_[deviceIndex].ports.size();
  devices_[deviceIndex].ports.append(port);
  emit devicePortAdded(deviceIndex, portIndex);
}

int TopologyDocument::insertDevicePort(int deviceIndex, int portIndex,
                                       const TopologyDevicePort& port) {
  if (deviceIndex < 0 || deviceIndex >= devices_.size())
    return -1;
  auto& dev = devices_[deviceIndex];
  if (portIndex < 0 || portIndex > dev.ports.size())
    return -1;
  dev.ports.insert(portIndex, port);
  emit devicePortAdded(deviceIndex, portIndex);
  return portIndex;
}

void TopologyDocument::addProductPort(int productIndex,
                                      const TopologyPort& port) {
  if (productIndex < 0 || productIndex >= products_.size())
    return;
  int portIndex = products_[productIndex].ports.size();
  products_[productIndex].ports.append(port);
  emit productPortAdded(productIndex, portIndex);
}

int TopologyDocument::insertProductPort(int productIndex, int portIndex,
                                        const TopologyPort& port) {
  if (productIndex < 0 || productIndex >= products_.size())
    return -1;
  auto& prod = products_[productIndex];
  if (portIndex < 0 || portIndex > prod.ports.size())
    return -1;
  prod.ports.insert(portIndex, port);
  emit productPortAdded(productIndex, portIndex);
  return portIndex;
}

void TopologyDocument::removeProductPort(int productIndex, int portIndex) {
  if (productIndex < 0 || productIndex >= products_.size())
    return;
  auto& prod = products_[productIndex];
  if (portIndex < 0 || portIndex >= prod.ports.size())
    return;
  prod.ports.removeAt(portIndex);
  emit productPortRemoved(productIndex, portIndex);
}

int TopologyDocument::findProductPortIndex(int productIndex,
                                           const QString& name) const {
  if (productIndex < 0 || productIndex >= products_.size())
    return -1;
  const auto& prod = products_[productIndex];
  for (int i = 0; i < prod.ports.size(); ++i) {
    if (prod.ports[i].name == name)
      return i;
  }
  return -1;
}

bool TopologyDocument::renameProductPort(int productIndex, int portIndex,
                                         const QString& newName) {
  auto* prod = product(productIndex);
  if (!prod || portIndex < 0 || portIndex >= prod->ports.size())
    return false;
  const QString oldName = prod->ports[portIndex].name;
  prod->ports[portIndex].name = newName;
  for (auto& connection : connections_) {
    if (connection.productName == prod->name && connection.portName == oldName)
      connection.portName = newName;
  }
  for (auto& monitor : monitors_) {
    for (auto& tap : monitor.taps) {
      if (tap.productName == prod->name && tap.portName == oldName)
        tap.portName = newName;
    }
  }
  emit productChanged(productIndex);
  return true;
}

void TopologyDocument::removeDevicePort(int deviceIndex, int portIndex) {
  if (deviceIndex < 0 || deviceIndex >= devices_.size())
    return;
  auto& dev = devices_[deviceIndex];
  if (portIndex < 0 || portIndex >= dev.ports.size())
    return;
  dev.ports.removeAt(portIndex);
  emit devicePortRemoved(deviceIndex, portIndex);
}

int TopologyDocument::findDevicePortIndex(int deviceIndex,
                                          const QString& name) const {
  if (deviceIndex < 0 || deviceIndex >= devices_.size())
    return -1;
  const auto& dev = devices_[deviceIndex];
  for (int i = 0; i < dev.ports.size(); ++i) {
    if (dev.ports[i].name == name)
      return i;
  }
  return -1;
}

bool TopologyDocument::renameDevicePort(int deviceIndex, int portIndex,
                                        const QString& newName) {
  auto* dev = device(deviceIndex);
  if (!dev || portIndex < 0 || portIndex >= dev->ports.size())
    return false;
  const QString oldName = dev->ports[portIndex].name;
  dev->ports[portIndex].name = newName;
  for (auto& connection : connections_) {
    if (connection.deviceName == dev->name && connection.devicePort == oldName)
      connection.devicePort = newName;
  }
  for (auto& monitor : monitors_) {
    for (auto& tap : monitor.taps) {
      if (tap.deviceName == dev->name && tap.devicePort == oldName)
        tap.devicePort = newName;
    }
  }
  emit deviceChanged(deviceIndex);
  return true;
}

int TopologyDocument::addConnection(const TopologyConnection& conn) {
  int index = connections_.size();
  TopologyConnection c = conn;
  if (c.id.isEmpty()) {
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  connections_.append(c);
  emit connectionAdded(index);
  return index;
}

int TopologyDocument::insertConnection(int index,
                                       const TopologyConnection& conn) {
  if (index < 0 || index > connections_.size())
    return -1;
  TopologyConnection c = conn;
  if (c.id.isEmpty()) {
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  connections_.insert(index, c);
  emit connectionAdded(index);
  return index;
}

void TopologyDocument::removeConnection(int index) {
  if (index < 0 || index >= connections_.size())
    return;
  connections_.removeAt(index);
  emit connectionRemoved(index);
}

TopologyConnection* TopologyDocument::connection(int index) {
  if (index < 0 || index >= connections_.size())
    return nullptr;
  return &connections_[index];
}

const TopologyConnection* TopologyDocument::connection(int index) const {
  if (index < 0 || index >= connections_.size())
    return nullptr;
  return &connections_[index];
}

int TopologyDocument::connectionCount() const {
  return connections_.size();
}

bool TopologyDocument::canConnect(const QString& productName,
                                  const QString& portName,
                                  const QString& deviceName,
                                  const QString& devicePortName) const {
  int pi = findProductIndex(productName);
  if (pi < 0)
    return false;
  int di = findDeviceIndex(deviceName);
  if (di < 0)
    return false;
  int dpi = findDevicePortIndex(di, devicePortName);
  if (dpi < 0)
    return false;

  const auto& product = products_[pi];
  const auto& dev = devices_[di];
  const auto& devPort = dev.ports[dpi];

  for (const auto& port : product.ports) {
    if (port.name == portName) {
      // Direction compatibility: UUT Input ↔ Device Output, UUT Output ↔ Device Input,
      // Bidirectional ↔ anything.
      if (!isDirectionCompatible(port.direction, devPort.direction))
        return false;

      // 支持设备类型名（如 "EPH6272T"）或功能类型名（如 "A429"）
      return port.allowedDeviceTypes.contains(dev.deviceType) ||
             port.allowedDeviceTypes.contains(
                 functionTypeToString(devPort.functionType));
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Monitor management
// ---------------------------------------------------------------------------
int TopologyDocument::addMonitor(const TopologyMonitor& monitor) {
  int index = monitors_.size();
  monitors_.append(monitor);
  emit monitorAdded(index);
  return index;
}

int TopologyDocument::insertMonitor(int index,
                                    const TopologyMonitor& monitor) {
  if (index < 0 || index > monitors_.size())
    return -1;
  monitors_.insert(index, monitor);
  emit monitorAdded(index);
  return index;
}

void TopologyDocument::removeMonitor(int index) {
  if (index < 0 || index >= monitors_.size())
    return;
  monitors_.removeAt(index);
  emit monitorRemoved(index);
}

TopologyMonitor* TopologyDocument::monitor(int index) {
  if (index < 0 || index >= monitors_.size())
    return nullptr;
  return &monitors_[index];
}

const TopologyMonitor* TopologyDocument::monitor(int index) const {
  if (index < 0 || index >= monitors_.size())
    return nullptr;
  return &monitors_[index];
}

int TopologyDocument::monitorCount() const { return monitors_.size(); }

int TopologyDocument::findMonitorIndex(const QString& name) const {
  for (int i = 0; i < monitors_.size(); ++i) {
    if (monitors_[i].name == name)
      return i;
  }
  return -1;
}

void TopologyDocument::addTap(int monitorIndex,
                               const TopologyMonitorTap& tap) {
  if (monitorIndex < 0 || monitorIndex >= monitors_.size())
    return;
  monitors_[monitorIndex].taps.append(tap);
  emit monitorChanged(monitorIndex);
}

int TopologyDocument::insertTap(int monitorIndex, int tapIndex,
                                const TopologyMonitorTap& tap) {
  if (monitorIndex < 0 || monitorIndex >= monitors_.size())
    return -1;
  auto& mon = monitors_[monitorIndex];
  if (tapIndex < 0 || tapIndex > mon.taps.size())
    return -1;
  mon.taps.insert(tapIndex, tap);
  emit monitorChanged(monitorIndex);
  return tapIndex;
}

void TopologyDocument::removeTap(int monitorIndex, int tapIndex) {
  if (monitorIndex < 0 || monitorIndex >= monitors_.size())
    return;
  auto& mon = monitors_[monitorIndex];
  if (tapIndex < 0 || tapIndex >= mon.taps.size())
    return;
  mon.taps.removeAt(tapIndex);
  emit monitorChanged(monitorIndex);
}

void TopologyDocument::clear() {
  undo_stack_->clear();
  products_.clear();
  devices_.clear();
  connections_.clear();
  monitors_.clear();
  emit documentCleared();
}

}  // namespace etest::topology
