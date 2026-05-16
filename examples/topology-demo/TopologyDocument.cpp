#include "TopologyDocument.h"

namespace topology {

TopologyDocument::TopologyDocument(QObject* parent)
    : QObject(parent) {}

int TopologyDocument::addProduct(const TopologyProduct& product) {
    int index = products_.size();
    products_.append(product);
    emit productAdded(index);
    return index;
}

void TopologyDocument::removeProduct(int index) {
    if (index < 0 || index >= products_.size()) return;
    products_.removeAt(index);
    emit productRemoved(index);
}

TopologyProduct* TopologyDocument::product(int index) {
    if (index < 0 || index >= products_.size()) return nullptr;
    return &products_[index];
}

const TopologyProduct* TopologyDocument::product(int index) const {
    if (index < 0 || index >= products_.size()) return nullptr;
    return &products_[index];
}

int TopologyDocument::productCount() const {
    return products_.size();
}

int TopologyDocument::findProductIndex(const QString& name) const {
    for (int i = 0; i < products_.size(); ++i) {
        if (products_[i].name == name) return i;
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
    if (index < 0 || index >= devices_.size()) return;
    devices_.removeAt(index);
    emit deviceRemoved(index);
}

TopologyDevice* TopologyDocument::device(int index) {
    if (index < 0 || index >= devices_.size()) return nullptr;
    return &devices_[index];
}

const TopologyDevice* TopologyDocument::device(int index) const {
    if (index < 0 || index >= devices_.size()) return nullptr;
    return &devices_[index];
}

int TopologyDocument::deviceCount() const {
    return devices_.size();
}

int TopologyDocument::findDeviceIndex(const QString& name) const {
    for (int i = 0; i < devices_.size(); ++i) {
        if (devices_[i].name == name) return i;
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
    if (index < 0 || index >= connections_.size()) return;
    connections_.removeAt(index);
    emit connectionRemoved(index);
}

TopologyConnection* TopologyDocument::connection(int index) {
    if (index < 0 || index >= connections_.size()) return nullptr;
    return &connections_[index];
}

const TopologyConnection* TopologyDocument::connection(int index) const {
    if (index < 0 || index >= connections_.size()) return nullptr;
    return &connections_[index];
}

int TopologyDocument::connectionCount() const {
    return connections_.size();
}

bool TopologyDocument::canConnect(const QString& productName,
                                  const QString& portName,
                                  const QString& deviceName) const {
    int pi = findProductIndex(productName);
    if (pi < 0) return false;
    int di = findDeviceIndex(deviceName);
    if (di < 0) return false;

    const auto& product = products_[pi];
    const auto& dev = devices_[di];

    for (const auto& port : product.ports) {
        if (port.name == portName) {
            return port.allowedDeviceTypes.contains(dev.deviceType);
        }
    }
    return false;
}

void TopologyDocument::clear() {
    products_.clear();
    devices_.clear();
    connections_.clear();
    emit documentCleared();
}

}  // namespace topology
