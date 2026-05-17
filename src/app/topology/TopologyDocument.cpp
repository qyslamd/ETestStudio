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
    case TopologyPort::Input:
      return QStringLiteral("Input");
    case TopologyPort::Output:
      return QStringLiteral("Output");
    case TopologyPort::Bidirectional:
      return QStringLiteral("Bidirectional");
  }
  return QStringLiteral("Output");
}

TopologyPort::Direction stringToDirection(const QString& s) {
  QString lower = s.toLower();
  if (lower == QStringLiteral("input"))
    return TopologyPort::Input;
  if (lower == QStringLiteral("bidirectional"))
    return TopologyPort::Bidirectional;
  return TopologyPort::Output;
}

TopologyDocument::TopologyDocument(QObject* parent)
    : QObject(parent), undo_stack_(new QUndoStack(this)) {}

bool TopologyDocument::isModified() const {
  return !undo_stack_->isClean();
}

int TopologyDocument::addProduct(const TopologyProduct& product) {
  int index = products_.size();
  products_.append(product);
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

int TopologyDocument::addDevice(const TopologyDevice& device) {
  int index = devices_.size();
  devices_.append(device);
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

void TopologyDocument::addDevicePort(int deviceIndex,
                                     const TopologyDevicePort& port) {
  if (deviceIndex < 0 || deviceIndex >= devices_.size())
    return;
  devices_[deviceIndex].ports.append(port);
  emit deviceChanged(deviceIndex);
}

void TopologyDocument::removeDevicePort(int deviceIndex, int portIndex) {
  if (deviceIndex < 0 || deviceIndex >= devices_.size())
    return;
  auto& dev = devices_[deviceIndex];
  if (portIndex < 0 || portIndex >= dev.ports.size())
    return;
  dev.ports.removeAt(portIndex);
  emit deviceChanged(deviceIndex);
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

int TopologyDocument::addConnection(const TopologyConnection& conn) {
  int index = connections_.size();
  connections_.append(conn);
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
      return port.allowedDeviceTypes.contains(
          functionTypeToString(devPort.functionType));
    }
  }
  return false;
}

void TopologyDocument::clear() {
  undo_stack_->clear();
  products_.clear();
  devices_.clear();
  connections_.clear();
  emit documentCleared();
}

}  // namespace etest::topology
