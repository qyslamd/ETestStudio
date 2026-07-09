#include "HardwareManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "SignalResolver.h"

#include "plugin/IADevicePlugin.h"
#include "plugin/IArinc429Plugin.h"
#include "plugin/ICANPlugin.h"
#include "plugin/IDADevicePlugin.h"
#include "plugin/IDevicePlugin.h"
#include "plugin/ISerialDevicePlugin.h"
#include "plugin/PluginManager.h"
#include "logger/Logger.h"

namespace etest::engine {

using namespace etest::core::plugin;
using namespace etest::core::logger;

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

HardwareManager::HardwareManager(QObject* parent) : QObject(parent) {}

HardwareManager::~HardwareManager() { shutdown(); }

// ---------------------------------------------------------------------------
// loadFromTopology — parse .etopo JSON and instantiate devices
// ---------------------------------------------------------------------------

bool HardwareManager::loadFromTopology(const QString& etopoPath) {
  QFile file(etopoPath);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_ERROR("HARDWARE", "无法打开拓扑文件: {}", etopoPath.toStdString());
    return false;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
  file.close();

  if (parseError.error != QJsonParseError::NoError) {
    LOG_ERROR("HARDWARE", "拓扑文件 JSON 解析失败: {}",
              parseError.errorString().toStdString());
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray devicesArr = root["devices"].toArray();
  if (devicesArr.isEmpty()) {
    LOG_WARN("HARDWARE", "拓扑文件中没有设备定义");
    return false;
  }

  int loaded = 0;
  for (const auto& dVal : devicesArr) {
    QJsonObject dObj = dVal.toObject();
    QString deviceId = dObj["id"].toString();
    QString pluginId = dObj["pluginId"].toString();

    if (deviceId.isEmpty() || pluginId.isEmpty()) {
      LOG_WARN("HARDWARE", "跳过缺少 id 或 pluginId 的设备定义");
      continue;
    }

    // Convert properties array -> QVariantMap
    QVariantMap properties;
    QJsonArray propsArr = dObj["properties"].toArray();
    for (const auto& propVal : propsArr) {
      QJsonObject propObj = propVal.toObject();
      properties.insert(propObj["key"].toString(),
                        propObj["value"].toString());
    }

    if (instantiateDevice(deviceId, pluginId, properties)) {
      ++loaded;
    } else {
      emit deviceError(deviceId,
                       QStringLiteral("设备实例化失败: plugin=%1").arg(pluginId));
    }
  }

  LOG_INFO("HARDWARE", "从拓扑文件加载了 {}/{} 个设备", loaded,
           devicesArr.size());
  return loaded > 0;
}

// ---------------------------------------------------------------------------
// instantiateDevice — look up plugin, open device, store in pool
// ---------------------------------------------------------------------------

bool HardwareManager::instantiateDevice(const QString& deviceId,
                                        const QString& pluginId,
                                        const QVariantMap& properties) {
  Q_UNUSED(properties);  // Phase 1: store properties for later use

  PluginManager& pm = PluginManager::instance();

  // Try direct plugin-ID lookup first
  IPlugin* plugin = pm.plugin(pluginId);
  if (!plugin) {
    // Fallback: search by device_type metadata
    QList<PluginMetaData> matches = pm.devicesByType(pluginId);
    if (matches.isEmpty()) {
      LOG_ERROR("HARDWARE", "未找到匹配的插件: {}", pluginId.toStdString());
      return false;
    }
    plugin = pm.plugin(matches.first().id);
  }

  if (!plugin) {
    LOG_ERROR("HARDWARE", "插件 {} 未能加载", pluginId.toStdString());
    return false;
  }

  IDevicePlugin* devPlugin = dynamic_cast<IDevicePlugin*>(plugin);
  if (!devPlugin) {
    LOG_ERROR("HARDWARE", "插件 {} 不是设备插件", pluginId.toStdString());
    return false;
  }

  // Open the hardware device
  if (!devPlugin->openDevice()) {
    LOG_ERROR("HARDWARE", "设备 {} 打开失败 (plugin={})",
              deviceId.toStdString(), pluginId.toStdString());
    emit deviceError(deviceId, QStringLiteral("打开设备失败"));
    return false;
  }

  DeviceEntry entry;
  entry.plugin = devPlugin;
  entry.status = (devPlugin->deviceStatus() ==
                          etest::core::plugin::DeviceStatus::Online
                      ? DeviceStatus::Online
                      : DeviceStatus::Offline);

  device_pool_.insert(deviceId, entry);
  emit deviceStatusChanged(deviceId, entry.status);

  LOG_INFO("HARDWARE", "设备 {} 已实例化 (plugin={})", deviceId.toStdString(),
           pluginId.toStdString());
  return true;
}

// ---------------------------------------------------------------------------
// pluginForDevice — internal lookup
// ---------------------------------------------------------------------------

IDevicePlugin* HardwareManager::pluginForDevice(
    const QString& deviceId) const {
  auto it = device_pool_.find(deviceId);
  return (it != device_pool_.end()) ? it->plugin : nullptr;
}

// ---------------------------------------------------------------------------
// read — route to appropriate plugin interface based on signal type
// ---------------------------------------------------------------------------

QVariant HardwareManager::read(const ResolvedSignal& signal) {
  IDevicePlugin* dev = pluginForDevice(signal.deviceId);
  if (!dev) {
    throw DeviceException(
        "设备未找到: " + signal.deviceId.toStdString());
  }

  auto it = device_pool_.find(signal.deviceId);
  if (it == device_pool_.end() || it->status != DeviceStatus::Online) {
    throw DeviceException(
        "设备离线: " + signal.deviceId.toStdString());
  }

  switch (signal.signalType) {
    case SignalType::AD: {
      IADevicePlugin* ad = dynamic_cast<IADevicePlugin*>(dev);
      if (!ad) {
        throw DeviceException(
            "设备不支持 AD 读取: " + signal.deviceId.toStdString());
      }
      // Read channel voltage as engineering value
      return QVariant(ad->readChannel(signal.channel));
    }
    case SignalType::DA: {
      IDADevicePlugin* da = dynamic_cast<IDADevicePlugin*>(dev);
      if (!da) {
        throw DeviceException(
            "设备不支持 DA 回读: " + signal.deviceId.toStdString());
      }
      return QVariant(da->readbackChannel(signal.channel));
    }
    case SignalType::CAN: {
      ICANPlugin* can = dynamic_cast<ICANPlugin*>(dev);
      if (!can) {
        throw DeviceException(
            "设备不支持 CAN 读取: " + signal.deviceId.toStdString());
      }
      return QVariant(can->receiveMessage(signal.frameId));
    }
    case SignalType::SERIAL: {
      ISerialDevicePlugin* serial = dynamic_cast<ISerialDevicePlugin*>(dev);
      if (!serial) {
        throw DeviceException(
            "设备不支持串口读取: " + signal.deviceId.toStdString());
      }
      return QVariant(serial->readData());
    }
    case SignalType::A429: {
      IArinc429Plugin* a429 = dynamic_cast<IArinc429Plugin*>(dev);
      if (!a429) {
        throw DeviceException(
            "设备不支持 A429 读取: " + signal.deviceId.toStdString());
      }
      return QVariant(a429->receiveLabel(static_cast<int>(signal.frameId)));
    }
  }
  throw DeviceException(
      "不支持的信号类型");
}

// ---------------------------------------------------------------------------
// readAndWait — Phase 1 mock: delegate directly to read()
// ---------------------------------------------------------------------------

QVariant HardwareManager::readAndWait(const ResolvedSignal& signal,
                                       int timeoutMs) {
  Q_UNUSED(timeoutMs);
  // Phase 1: simply call read(); Phase 2 will introduce QEventLoop + timer.
  return read(signal);
}

// ---------------------------------------------------------------------------
// write — route to appropriate plugin interface
// ---------------------------------------------------------------------------

bool HardwareManager::write(const ResolvedSignal& signal, double engValue) {
  IDevicePlugin* dev = pluginForDevice(signal.deviceId);
  if (!dev) {
    return false;
  }

  auto it = device_pool_.find(signal.deviceId);
  if (it == device_pool_.end() || it->status != DeviceStatus::Online) {
    return false;
  }

  switch (signal.signalType) {
    case SignalType::DA: {
      IDADevicePlugin* da = dynamic_cast<IDADevicePlugin*>(dev);
      if (!da) {
        return false;
      }
      return da->writeChannel(signal.channel, engValue);
    }
    case SignalType::CAN:
    case SignalType::SERIAL:
    case SignalType::A429:
      // Frame-type write requires encoded raw data — use writeFrame() instead
      // Phase 2: implement engineering-value-to-raw encoding
      return false;
    case SignalType::AD:
      // AD devices are input-only
      return false;
  }
  return false;
}

// ---------------------------------------------------------------------------
// writeFrame — write raw frame data to frame-type devices
// ---------------------------------------------------------------------------

bool HardwareManager::writeFrame(const ResolvedSignal& signal,
                                  const QByteArray& frameData) {
  IDevicePlugin* dev = pluginForDevice(signal.deviceId);
  if (!dev) {
    return false;
  }

  auto it = device_pool_.find(signal.deviceId);
  if (it == device_pool_.end() || it->status != DeviceStatus::Online) {
    return false;
  }

  switch (signal.signalType) {
    case SignalType::CAN: {
      ICANPlugin* can = dynamic_cast<ICANPlugin*>(dev);
      if (!can) {
        return false;
      }
      return can->sendMessage(signal.frameId, frameData);
    }
    case SignalType::SERIAL: {
      ISerialDevicePlugin* serial = dynamic_cast<ISerialDevicePlugin*>(dev);
      if (!serial) {
        return false;
      }
      return serial->writeData(frameData) >= 0;
    }
    case SignalType::A429: {
      IArinc429Plugin* a429 = dynamic_cast<IArinc429Plugin*>(dev);
      if (!a429) {
        return false;
      }
      return a429->sendLabel(static_cast<int>(signal.frameId), frameData);
    }
    case SignalType::AD:
    case SignalType::DA:
      // Channel-type devices use write() instead
      return false;
  }
  return false;
}

// ---------------------------------------------------------------------------
// deviceStatus / onlineDevices
// ---------------------------------------------------------------------------

DeviceStatus HardwareManager::deviceStatus(const QString& deviceId) const {
  auto it = device_pool_.find(deviceId);
  if (it == device_pool_.end()) {
    return DeviceStatus::Offline;
  }
  return it->status;
}

QList<QString> HardwareManager::onlineDevices() const {
  QList<QString> result;
  for (auto it = device_pool_.begin(); it != device_pool_.end(); ++it) {
    if (it->status == DeviceStatus::Online) {
      result.append(it.key());
    }
  }
  return result;
}

// ---------------------------------------------------------------------------
// shutdown — close all devices and clear the pool
// ---------------------------------------------------------------------------

void HardwareManager::shutdown() {
  for (auto it = device_pool_.begin(); it != device_pool_.end(); ++it) {
    if (it->plugin) {
      it->plugin->closeDevice();
    }
    emit deviceStatusChanged(it.key(), DeviceStatus::Offline);
  }
  device_pool_.clear();
}

}  // namespace etest::engine
