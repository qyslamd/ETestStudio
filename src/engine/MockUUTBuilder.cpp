#include "MockUUTBuilder.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <icd/repository.hpp>

#include "logger/Logger.h"

namespace etest::engine {

// ============================================================================
// FramePortSimulator — 基类
// ============================================================================

FramePortSimulator::FramePortSimulator(const QString& portName,
                                         const QString& deviceId,
                                         const QVector<int>& receiveFrameIds,
                                         const QVector<int>& sendFrameIds)
    : device_id_(deviceId),
      port_name_(portName),
      receive_frame_ids_(receiveFrameIds),
      send_frame_ids_(sendFrameIds) {}

bool FramePortSimulator::hasReceiveFrame(int frameId) const {
  return receive_frame_ids_.contains(frameId);
}

// ============================================================================
// SerialPortSimulator
// ============================================================================

SerialPortSimulator::SerialPortSimulator(const QString& portName,
                                           const QString& deviceId,
                                           const QVector<int>& receiveFrameIds,
                                           const QVector<int>& sendFrameIds)
    : FramePortSimulator(portName, deviceId, receiveFrameIds,
                          sendFrameIds) {}

std::optional<MockResponse> SerialPortSimulator::onFrameReceived(
    int frameId, const QByteArray& frameData) {
  Q_UNUSED(frameData);
  if (!receive_frame_ids_.contains(frameId)) {
    return std::nullopt;
  }
  auto it = response_config_.find(frameId);
  if (it == response_config_.end()) {
    return std::nullopt;
  }
  return MockResponse{0, it.value()};
}

void SerialPortSimulator::setResponseConfig(int frameId,
                                             const QByteArray& responseHex) {
  response_config_[frameId] = responseHex;
}

// ============================================================================
// CANPortSimulator
// ============================================================================

CANPortSimulator::CANPortSimulator(const QString& portName,
                                     const QString& deviceId,
                                     const QVector<int>& receiveFrameIds,
                                     const QVector<int>& sendFrameIds)
    : FramePortSimulator(portName, deviceId, receiveFrameIds,
                          sendFrameIds) {}

std::optional<MockResponse> CANPortSimulator::onFrameReceived(
    int frameId, const QByteArray& frameData) {
  Q_UNUSED(frameData);
  if (!receive_frame_ids_.contains(frameId)) {
    return std::nullopt;
  }
  auto it = response_config_.find(frameId);
  if (it == response_config_.end()) {
    return std::nullopt;
  }
  // CAN 回复使用 send_frame_id 作为 CAN ID（VERIFY 按此 ID 读取）
  int targetId = send_frame_ids_.isEmpty() ? frameId : send_frame_ids_.first();
  return MockResponse{targetId, it.value()};
}

void CANPortSimulator::setResponseConfig(int frameId,
                                          const QByteArray& responseHex) {
  response_config_[frameId] = responseHex;
}

// ============================================================================
// A429PortSimulator
// ============================================================================

A429PortSimulator::A429PortSimulator(const QString& portName,
                                       const QString& deviceId,
                                       const QVector<int>& receiveFrameIds,
                                       const QVector<int>& sendFrameIds)
    : FramePortSimulator(portName, deviceId, receiveFrameIds,
                          sendFrameIds) {}

std::optional<MockResponse> A429PortSimulator::onFrameReceived(
    int frameId, const QByteArray& frameData) {
  Q_UNUSED(frameData);
  if (!receive_frame_ids_.contains(frameId)) {
    return std::nullopt;
  }
  auto it = response_config_.find(frameId);
  if (it == response_config_.end()) {
    return std::nullopt;
  }
  // A429 回复使用 send_frame_id 作为 Label（VERIFY 按此 Label 读取）
  int targetId = send_frame_ids_.isEmpty() ? frameId : send_frame_ids_.first();
  return MockResponse{targetId, it.value()};
}

void A429PortSimulator::setResponseConfig(int frameId,
                                           const QByteArray& responseHex) {
  response_config_[frameId] = responseHex;
}

// ============================================================================
// ChannelPortSimulator — 基类
// ============================================================================

ChannelPortSimulator::ChannelPortSimulator(const QString& deviceId,
                                             int frameId)
    : device_id_(deviceId), frame_id_(frameId) {}

// ============================================================================
// ADChannelSimulator
// ============================================================================

ADChannelSimulator::ADChannelSimulator(const QString& deviceId, int frameId,
                                         int channel)
    : ChannelPortSimulator(deviceId, frameId), channel_(channel) {}

double ADChannelSimulator::readChannelValue(int channel) {
  Q_UNUSED(channel);
  return fixed_value_;
}

void ADChannelSimulator::setFixedValue(double value) { fixed_value_ = value; }

// ============================================================================
// MockUUT
// ============================================================================

MockUUT::MockUUT(const QString& name) : name_(name) {}

void MockUUT::addFrameSimulator(std::unique_ptr<FramePortSimulator> sim) {
  frame_sims_.push_back(std::move(sim));
}

void MockUUT::addChannelSimulator(std::unique_ptr<ChannelPortSimulator> sim) {
  channel_sims_.push_back(std::move(sim));
}

FramePortSimulator* MockUUT::findFrameSimulator(const QString& deviceId,
                                                  int frameId) const {
  for (const auto& sim : frame_sims_) {
    if (sim->deviceId() == deviceId && sim->hasReceiveFrame(frameId)) {
      return sim.get();
    }
  }
  return nullptr;
}

ChannelPortSimulator* MockUUT::findChannelSimulator(int frameId) const {
  for (const auto& sim : channel_sims_) {
    if (sim->frameId() == frameId) {
      return sim.get();
    }
  }
  return nullptr;
}

std::optional<MockResponse> MockUUT::onFrameWritten(
    const QString& deviceId, int frameId, const QByteArray& frameData) {
  auto* sim = findFrameSimulator(deviceId, frameId);
  if (!sim) {
    return std::nullopt;
  }
  return sim->onFrameReceived(frameId, frameData);
}

// ============================================================================
// MockUUTBuilder
// ============================================================================

MockUUTBuilder::MockUUTBuilder(icd::Repository* icdRepo,
                                 const QJsonObject& topologyDoc)
    : icd_repo_(icdRepo), topology_doc_(topologyDoc) {
  if (!icd_repo_) {
    last_error_ = QStringLiteral("ICD Repository 为空");
  }
}

void MockUUTBuilder::loadResponseConfigFile(const QString& filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    // 配置文件可选，不报错
    return;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
  file.close();

  if (parseError.error != QJsonParseError::NoError) {
    return;  // 配置解析失败，静默忽略
  }

  QJsonObject root = doc.object();
  frame_responses_ = root["portBehaviors"].toArray();
  LOG_INFO("ENGINE", "加载 Mock 响应配置 [path={}]", filePath.toStdString());
}

bool MockUUTBuilder::buildAll(std::vector<std::unique_ptr<MockUUT>>& out) {
  LOG_INFO("ENGINE", "MockUUTBuilder 开始构建");
  out.clear();

  if (!icd_repo_) {
    last_error_ = QStringLiteral("ICD Repository 为空，无法构建 MockUUT");
    return false;
  }

  QJsonArray products = topology_doc_["products"].toArray();
  QJsonArray connectionsArr = topology_doc_["connections"].toArray();
  QJsonArray devices = topology_doc_["devices"].toArray();

  for (const auto& prodVal : products) {
    QJsonObject product = prodVal.toObject();
    std::unique_ptr<MockUUT> uut;
    if (!buildSingleUUT(product, connectionsArr, devices, uut)) {
      return false;
    }
    if (uut) {
      out.push_back(std::move(uut));
    }
  }

  LOG_INFO("ENGINE", "MockUUTBuilder 构建完成 [uuts={}]", out.size());
  return true;
}

bool MockUUTBuilder::buildSingleUUT(const QJsonObject& product,
                                     const QJsonArray& connectionsArr,
                                     const QJsonArray& devices,
                                     std::unique_ptr<MockUUT>& out) {
  QString productName = product["name"].toString();
  if (productName.isEmpty()) {
    last_error_ = QStringLiteral("UUT 名称不能为空");
    return false;
  }

  auto uut = std::make_unique<MockUUT>(productName);
  QJsonArray ports = product["ports"].toArray();

  for (const auto& portVal : ports) {
    QJsonObject port = portVal.toObject();
    QString portName = port["name"].toString();
    if (portName.isEmpty()) {
      last_error_ = QStringLiteral("UUT '%1' 的端口名称不能为空").arg(productName);
      return false;
    }

    // 找到该 UUT 端口对应的连接
    QJsonObject matchedConn;
    bool found = false;
    for (const auto& connVal : connectionsArr) {
      QJsonObject conn = connVal.toObject();
      if (conn["productName"].toString() == productName &&
          conn["portName"].toString() == portName) {
        matchedConn = conn;
        found = true;
        break;
      }
    }
    if (!found) {
      // 未连接的端口跳过
      continue;
    }

    QString deviceName = matchedConn["deviceName"].toString();
    QString devicePortName = matchedConn["devicePort"].toString();

    // 根据 deviceName 查找设备
    QJsonObject matchedDevice;
    bool devFound = false;
    for (const auto& devVal : devices) {
      QJsonObject dev = devVal.toObject();
      if (dev["id"].toString() == deviceName) {
        matchedDevice = dev;
        devFound = true;
        break;
      }
    }
    if (!devFound) {
      continue;
    }

    QString deviceId = matchedDevice["id"].toString();
    QString deviceType = matchedDevice["type"].toString();

    // 根据设备类型决定创建帧型还是通道型模拟器
    if (deviceType == QStringLiteral("serial") ||
        deviceType == QStringLiteral("can") ||
        deviceType == QStringLiteral("a429")) {
      // 帧型端口
      auto sim = buildFrameSimulator(port, matchedConn, matchedDevice);
      if (sim) {
        // 应用响应配置
        for (const auto& respVal : frame_responses_) {
          QJsonObject resp = respVal.toObject();
          if (resp["productName"].toString() == productName &&
              resp["deviceId"].toString() == deviceId &&
              resp["port"].toString() == portName) {
            QJsonArray responses = resp["responses"].toArray();
            for (const auto& rVal : responses) {
              QJsonObject r = rVal.toObject();
              int frameId = r["frameId"].toInt();
              QString hex = r["responseHex"].toString();
              QByteArray bytes =
                  QByteArray::fromHex(hex.remove(' ').toLatin1());
              sim->setResponseConfig(frameId, bytes);
            }
          }
        }
        uut->addFrameSimulator(std::move(sim));
      }
    } else if (deviceType == QStringLiteral("ad")) {
      // AD 通道型端口
      auto sim = buildChannelSimulator(port, matchedConn, matchedDevice);
      if (sim) {
        // 应用 AD 配置（fixedValue）
        for (const auto& respVal : frame_responses_) {
          QJsonObject resp = respVal.toObject();
          if (resp["productName"].toString() == productName &&
              resp["deviceId"].toString() == deviceId &&
              resp["port"].toString() == portName) {
            if (resp.contains("fixedValue")) {
              auto* adSim = dynamic_cast<ADChannelSimulator*>(sim.get());
              if (adSim) {
                adSim->setFixedValue(resp["fixedValue"].toDouble());
              }
            }
          }
        }
        uut->addChannelSimulator(std::move(sim));
      }
    }
    // DA 设备由插件内部处理，不创建模拟器
  }

  out = std::move(uut);
  return true;
}

std::unique_ptr<FramePortSimulator> MockUUTBuilder::buildFrameSimulator(
    const QJsonObject& port, const QJsonObject& connection,
    const QJsonObject& device) {
  Q_UNUSED(connection);
  QString deviceType = device["type"].toString();
  QString portName = port["name"].toString();
  QString deviceId = device["id"].toString();

  // 解析接收帧名 → ID
  QVector<int> receiveIds;
  QStringList recvNames = frameNamesFromArray(port["receiveFrames"].toArray());
  if (!resolveFrameNamesToIds(recvNames, receiveIds)) {
    return nullptr;
  }

  // 解析发送帧名 → ID
  QVector<int> sendIds;
  QStringList sendNames = frameNamesFromArray(port["sendFrames"].toArray());
  if (!resolveFrameNamesToIds(sendNames, sendIds)) {
    return nullptr;
  }

  if (deviceType == QStringLiteral("serial")) {
    return std::make_unique<SerialPortSimulator>(portName, deviceId,
                                                  receiveIds, sendIds);
  }
  if (deviceType == QStringLiteral("can")) {
    return std::make_unique<CANPortSimulator>(portName, deviceId, receiveIds,
                                               sendIds);
  }
  if (deviceType == QStringLiteral("a429")) {
    return std::make_unique<A429PortSimulator>(portName, deviceId, receiveIds,
                                                sendIds);
  }

  return nullptr;
}

std::unique_ptr<ChannelPortSimulator> MockUUTBuilder::buildChannelSimulator(
    const QJsonObject& port, const QJsonObject& connection,
    const QJsonObject& device) {
  Q_UNUSED(connection);
  QString deviceType = device["type"].toString();
  QString deviceId = device["id"].toString();
  int channel = port["channel"].toInt(0);

  // 解析 receiveFrames → frameId
  QVector<int> receiveIds;
  QStringList recvNames = frameNamesFromArray(port["receiveFrames"].toArray());
  if (!resolveFrameNamesToIds(recvNames, receiveIds)) {
    return nullptr;
  }

  int frameId = receiveIds.isEmpty() ? 0 : receiveIds.first();

  if (deviceType == QStringLiteral("ad")) {
    return std::make_unique<ADChannelSimulator>(deviceId, frameId, channel);
  }

  return nullptr;
}

bool MockUUTBuilder::resolveFrameNamesToIds(const QStringList& frameNames,
                                              QVector<int>& outIds) {
  outIds.clear();
  for (const QString& name : frameNames) {
    const icd::Frame* frame =
        icd_repo_->find(name.toStdString());
    if (!frame) {
      last_error_ =
          QStringLiteral("ICD 中未找到帧: %1").arg(name);
      return false;
    }
    outIds.push_back(frame->id());
  }
  return true;
}

QStringList MockUUTBuilder::frameNamesFromArray(const QJsonArray& arr) const {
  QStringList names;
  for (const auto& val : arr) {
    if (val.isString()) {
      names.append(val.toString());
    }
  }
  return names;
}

}  // namespace etest::engine
