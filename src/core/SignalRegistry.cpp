#include "SignalRegistry.h"

#include <QCryptographicHash>

namespace etest::core {

SignalRegistry::SignalRegistry(QObject* parent) : QObject(parent) {}

// ══════════════════════════════════════════════════════════════════════════════
//  UUID 计算（确定性纯函数，不依赖索引）
// ══════════════════════════════════════════════════════════════════════════════

QString SignalRegistry::computeUuid(const QString& deviceId,
                                    const QString& portName,
                                    const QString& frameName,
                                    const QString& nodePath) {
  // 4 元组用 \x1f 分隔（US, 不可见字符, 避免与 nodePath 的 / 冲突）
  QByteArray raw;
  raw.append(deviceId.toUtf8()).append('\x1f');
  raw.append(portName.toUtf8()).append('\x1f');
  raw.append(frameName.toUtf8()).append('\x1f');
  raw.append(nodePath.toUtf8());
  return QString::fromLatin1(
             QCryptographicHash::hash(raw, QCryptographicHash::Sha1).toHex())
      .left(32);
}

// ══════════════════════════════════════════════════════════════════════════════
//  设备注册
// ══════════════════════════════════════════════════════════════════════════════

void SignalRegistry::registerDevice(const QString& deviceId,
                                    const QString& deviceName,
                                    const QString& deviceType) {
  device_names_[deviceId] = deviceName;
  if (!deviceType.isEmpty()) {
    device_types_[deviceId] = deviceType;
  }
}

QStringList SignalRegistry::registeredDeviceIds() const {
  return device_names_.keys();
}

QString SignalRegistry::deviceName(const QString& deviceId) const {
  return device_names_.value(deviceId);
}

QString SignalRegistry::deviceType(const QString& deviceId) const {
  return device_types_.value(deviceId);
}

// ══════════════════════════════════════════════════════════════════════════════
//  端口绑定
// ══════════════════════════════════════════════════════════════════════════════

void SignalRegistry::bindPortToFrames(const QString& deviceId,
                                      const QString& portName,
                                      const QStringList& frameNames) {
  port_to_frames_[{deviceId, portName}] = frameNames;
  emit bindingsChanged();
}

void SignalRegistry::unbindPort(const QString& deviceId,
                                const QString& portName) {
  port_to_frames_.remove({deviceId, portName});
  // 同时也清理该端口的信号索引
  QVector<QString> toRemove;
  for (auto it = uuid_index_.begin(); it != uuid_index_.end(); ++it) {
    if (it->deviceId == deviceId && it->portName == portName) {
      toRemove.append(it.key());
    }
  }
  for (const QString& uuid : toRemove) {
    const auto& sig = uuid_index_.value(uuid);
    // 仅从 node_to_uuids_ 列表中移除当前 UUID，不要删整个 key
    // 因为多台设备的同一 (frameName, nodePath) 可能共享该 key
    auto nIt = node_to_uuids_.find({sig.frameName, sig.nodePath});
    if (nIt != node_to_uuids_.end()) {
      nIt->removeAll(uuid);
      if (nIt->isEmpty()) {
        node_to_uuids_.erase(nIt);
      }
    }
    uuid_index_.remove(uuid);
  }
  emit bindingsChanged();
}

void SignalRegistry::forEachPortBinding(PortBindingCallback cb) const {
  for (auto it = port_to_frames_.begin(); it != port_to_frames_.end(); ++it) {
    cb(it.key().first, it.key().second, it.value());
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  信号索引
// ══════════════════════════════════════════════════════════════════════════════

void SignalRegistry::registerSignals(const QVector<SignalEntry>& entries) {
  for (const auto& entry : entries) {
    QString uuid = computeUuid(entry.deviceId, entry.portName, entry.frameName,
                               entry.nodePath);
    ResolvedSignal sig;
    sig.uuid = uuid;
    sig.deviceId = entry.deviceId;
    sig.deviceName = device_names_.value(entry.deviceId);
    sig.deviceType = device_types_.value(entry.deviceId);
    sig.portName = entry.portName;
    sig.frameName = entry.frameName;
    sig.nodePath = entry.nodePath;
    uuid_index_[uuid] = sig;
    node_to_uuids_[{entry.frameName, entry.nodePath}].append(uuid);
  }
  emit bindingsChanged();
}

// ══════════════════════════════════════════════════════════════════════════════
//  查询
// ══════════════════════════════════════════════════════════════════════════════

std::optional<ResolvedSignal> SignalRegistry::resolve(
    const QString& uuid) const {
  auto it = uuid_index_.find(uuid);
  if (it != uuid_index_.end()) {
    return it.value();
  }
  return std::nullopt;
}

QString SignalRegistry::resolveByTuple(const QString& deviceId,
                                       const QString& portName,
                                       const QString& frameName,
                                       const QString& nodePath) const {
  QString uuid = computeUuid(deviceId, portName, frameName, nodePath);
  // 如果索引中存在该 UUID 直接返回，否则返回计算值
  if (uuid_index_.contains(uuid)) {
    return uuid;
  }
  return uuid;
}

QVector<ResolvedSignal> SignalRegistry::findByNode(
    const QString& frameName, const QString& nodePath) const {
  QVector<ResolvedSignal> result;
  auto it = node_to_uuids_.find({frameName, nodePath});
  if (it != node_to_uuids_.end()) {
    for (const QString& uuid : it.value()) {
      auto sig = uuid_index_.value(uuid);
      result.append(sig);
    }
  }
  return result;
}

QVector<ResolvedSignal> SignalRegistry::findByPort(
    const QString& deviceId, const QString& portName) const {
  QVector<ResolvedSignal> result;
  for (auto it = uuid_index_.begin(); it != uuid_index_.end(); ++it) {
    if (it->deviceId == deviceId && it->portName == portName) {
      result.append(it.value());
    }
  }
  return result;
}

// ══════════════════════════════════════════════════════════════════════════════
//  清理
// ══════════════════════════════════════════════════════════════════════════════

void SignalRegistry::clearSignals() {
  uuid_index_.clear();
  node_to_uuids_.clear();
  emit bindingsChanged();
}

void SignalRegistry::clear() {
  uuid_index_.clear();
  port_to_frames_.clear();
  device_names_.clear();
  device_types_.clear();
  node_to_uuids_.clear();
  emit bindingsChanged();
}

}  // namespace etest::core
