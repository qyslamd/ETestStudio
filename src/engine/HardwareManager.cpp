#include "HardwareManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "MockUUTBuilder.h"
#include "SignalResolver.h"

#include "plugin_sdk/IADevicePlugin.h"
#include "plugin_sdk/IArinc429Plugin.h"
#include "plugin_sdk/ICANPlugin.h"
#include "plugin_sdk/IDADevicePlugin.h"
#include "plugin_sdk/IDevicePlugin.h"
#include "plugin_sdk/ISerialDevicePlugin.h"
#include "plugin_sdk/PluginManager.h"
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

bool HardwareManager::loadFromTopology(const QJsonObject& root) {
  QJsonArray devicesArr = root["devices"].toArray();
  if (devicesArr.isEmpty()) {
    LOG_WARN("HARDWARE", "拓扑中没有设备定义");
    return false;
  }

  LOG_INFO("ENGINE", "加载设备拓扑 [devices={}]", devicesArr.size());
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

    // 注入 type 字段（device 级别），供 instantiateDevice 的 mock 分支使用
    properties.insert(QStringLiteral("type"), dObj["type"].toString());

    bool mock = dObj["mock"].toBool(false);
    if (instantiateDevice(deviceId, pluginId, properties, mock)) {
      ++loaded;
    } else {
      emit deviceError(deviceId,
                       QStringLiteral("设备实例化失败: plugin=%1").arg(pluginId));
    }
  }

  LOG_INFO("HARDWARE", "从拓扑加载了 {}/{} 个设备", loaded, devicesArr.size());
  LOG_INFO("ENGINE", "设备加载完成 [loaded={}]", loaded);
  return loaded > 0;
}

// ---------------------------------------------------------------------------
// instantiateDevice — look up plugin (real or mock), open, store in pool
// ---------------------------------------------------------------------------

bool HardwareManager::instantiateDevice(const QString& deviceId,
                                        const QString& pluginId,
                                        const QVariantMap& properties,
                                        bool mock) {
  LOG_INFO("ENGINE", "实例化设备 [id={} plugin={} mock={}]", deviceId.toStdString(), pluginId.toStdString(), mock);
  PluginManager& pm = PluginManager::instance();
  IPlugin* plugin = nullptr;
  QString type = properties.value(QStringLiteral("type")).toString();

  if (mock) {
    // ── 只在 Mock 插件池中搜索 ──
    plugin = pm.plugin(pluginId);
    if (!plugin) {
      QList<PluginMetaData> matches = pm.devicesByMockType(type, true);
      if (!matches.isEmpty()) {
        plugin = pm.plugin(matches.first().id);
      }
    }
    if (!plugin) {
      LOG_ERROR("HARDWARE", "Mock 插件未找到: type={}, pluginId={}",
                type.toStdString(), pluginId.toStdString());
      emit deviceError(deviceId, QStringLiteral("Mock 插件未找到，请检查 plugins/ 目录"));
      return false;
    }
  } else {
    // ── 只在真实插件池中搜索 ──
    plugin = pm.plugin(pluginId);
    if (!plugin) {
      QList<PluginMetaData> matches = pm.devicesByMockType(type, false);
      if (!matches.isEmpty()) {
        plugin = pm.plugin(matches.first().id);
      }
    }
    if (!plugin) {
      LOG_ERROR("HARDWARE", "真实设备插件未找到: type={}, pluginId={}",
                type.toStdString(), pluginId.toStdString());
      emit deviceError(deviceId,
                       QStringLiteral("真实设备插件未找到，请检查硬件驱动是否已安装"));
      return false;
    }
  }

  IDevicePlugin* devPlugin = dynamic_cast<IDevicePlugin*>(plugin);
  if (!devPlugin) {
    LOG_ERROR("HARDWARE", "插件 {} 不是设备插件", pluginId.toStdString());
    return false;
  }

  // Open the hardware device (for mock plugins this is a no-op)
  if (!devPlugin->openDevice()) {
    LOG_ERROR("HARDWARE", "设备 {} 打开失败 (plugin={})", deviceId.toStdString(),
              pluginId.toStdString());
    emit deviceError(deviceId, QStringLiteral("打开设备失败"));
    return false;
  }

  DeviceEntry entry;
  entry.plugin = devPlugin;
  entry.status = (devPlugin->deviceStatus() ==
                          etest::core::plugin::DeviceStatus::Online
                      ? DeviceStatus::Online
                      : DeviceStatus::Offline);
  entry.is_mock = mock;

  device_pool_.insert(deviceId, entry);
  emit deviceStatusChanged(deviceId, entry.status);

  LOG_INFO("HARDWARE", "设备 {} {} 已实例化 (plugin={})", deviceId.toStdString(),
           mock ? "(Mock)" : "(Real)", pluginId.toStdString());
  return true;
}

// ---------------------------------------------------------------------------
// setMockUUT — take ownership of MockUUT instances from Builder
// ---------------------------------------------------------------------------

void HardwareManager::setMockUUT(std::vector<std::unique_ptr<MockUUT>> uuts) {
  LOG_INFO("ENGINE", "注入 MockUUT [count={}]", uuts.size());
  mock_uut_holders_ = std::move(uuts);
  mock_uuts_.clear();
  for (const auto& uut : mock_uut_holders_) {
    mock_uuts_.append(uut.get());
  }
}

// ---------------------------------------------------------------------------
// closeAllDevices — close all devices without clearing MockUUT
// ---------------------------------------------------------------------------

void HardwareManager::closeAllDevices() {
  LOG_INFO("ENGINE", "关闭所有设备");
  for (auto it = device_pool_.begin(); it != device_pool_.end(); ++it) {
    if (it->plugin) {
      it->plugin->closeDevice();
    }
  }
  device_pool_.clear();
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
// findMockUUTForFrame — locate MockUUT by deviceId + frameId
// ---------------------------------------------------------------------------

MockUUT* HardwareManager::findMockUUTForFrame(const QString& deviceId,
                                                int frameId) const {
  for (auto* uut : mock_uuts_) {
    if (uut->findFrameSimulator(deviceId, frameId)) {
      return uut;
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// read — route to appropriate plugin interface based on signal type
// ---------------------------------------------------------------------------

QVariant HardwareManager::read(const ResolvedSignal& signal) {
  LOG_INFO("ENGINE", "读取 [device={}]", signal.deviceId.toStdString());
  // ── Mock AD/DA: direct channel read from MockUUT ──
  if (!mock_uuts_.isEmpty() &&
      (signal.signalType == SignalType::AD ||
       signal.signalType == SignalType::DA)) {
    for (auto* uut : mock_uuts_) {
      auto* sim = uut->findChannelSimulator(signal.frameId);
      if (sim) {
        double val = sim->readChannelValue(signal.channel);
        return QVariant(val);
      }
    }
  }

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
// readAndWait — delegate directly to read()
// ---------------------------------------------------------------------------

QVariant HardwareManager::readAndWait(const ResolvedSignal& signal,
                                       int timeoutMs) {
  Q_UNUSED(timeoutMs);
  return read(signal);
}

// ---------------------------------------------------------------------------
// write — route to appropriate plugin interface
// ---------------------------------------------------------------------------

bool HardwareManager::write(const ResolvedSignal& signal, double engValue) {
  LOG_INFO("ENGINE", "写入 [device={} value={}]", signal.deviceId.toStdString(), engValue);
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
      return false;
    case SignalType::AD:
      return false;
  }
  return false;
}

// ---------------------------------------------------------------------------
// writeFrame — write raw frame data + handle MockUUT response
// ---------------------------------------------------------------------------

bool HardwareManager::writeFrame(const ResolvedSignal& signal,
                                  const QByteArray& frameData) {
  LOG_INFO("ENGINE", "写入帧 [device={} frameId={}]", signal.deviceId.toStdString(), signal.frameId);
  IDevicePlugin* dev = pluginForDevice(signal.deviceId);
  if (!dev) {
    return false;
  }

  auto it = device_pool_.find(signal.deviceId);
  if (it == device_pool_.end() || it->status != DeviceStatus::Online) {
    return false;
  }

  // ── ① 先写设备（环回保障基本读写） ──
  switch (signal.signalType) {
    case SignalType::CAN: {
      ICANPlugin* can = dynamic_cast<ICANPlugin*>(dev);
      if (!can) return false;
      if (!can->sendMessage(signal.frameId, frameData)) return false;
      break;
    }
    case SignalType::SERIAL: {
      ISerialDevicePlugin* serial = dynamic_cast<ISerialDevicePlugin*>(dev);
      if (!serial) return false;
      if (serial->writeData(frameData) < 0) return false;
      break;
    }
    case SignalType::A429: {
      IArinc429Plugin* a429 = dynamic_cast<IArinc429Plugin*>(dev);
      if (!a429) return false;
      if (!a429->sendLabel(static_cast<int>(signal.frameId), frameData))
        return false;
      break;
    }
    default:
      return false;
  }

  // ── ② Mock 模式：查 MockUUT 获取模拟回复 ──
  if (auto* mockUUT = findMockUUTForFrame(signal.deviceId,
                                            signal.frameId)) {
    auto resp = mockUUT->onFrameWritten(signal.deviceId, signal.frameId,
                                         frameData);
    if (resp) {
      // ③ 把回复帧写入设备插件（Mock 插件退化为内存操作）
      switch (signal.signalType) {
        case SignalType::SERIAL: {
          auto* serial = dynamic_cast<ISerialDevicePlugin*>(dev);
          if (serial) serial->writeData(resp->data);
          break;
        }
        case SignalType::CAN: {
          auto* can = dynamic_cast<ICANPlugin*>(dev);
          if (can)
            can->sendMessage(static_cast<quint32>(resp->targetFrameId),
                             resp->data);
          break;
        }
        case SignalType::A429: {
          auto* a429 = dynamic_cast<IArinc429Plugin*>(dev);
          if (a429) a429->sendLabel(resp->targetFrameId, resp->data);
          break;
        }
        default:
          break;
      }
    }
  }

  return true;
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
// shutdown — close all devices and clear pools
// ---------------------------------------------------------------------------

void HardwareManager::shutdown() {
  for (auto it = device_pool_.begin(); it != device_pool_.end(); ++it) {
    if (it->plugin) {
      it->plugin->closeDevice();
    }
    emit deviceStatusChanged(it.key(), DeviceStatus::Offline);
  }
  device_pool_.clear();
  mock_uuts_.clear();
  mock_uut_holders_.clear();
}

}  // namespace etest::engine
