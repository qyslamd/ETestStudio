#include "MockUUTBuilder.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>

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
    : ChannelPortSimulator(deviceId, frameId), channel_(channel) {
  // 默认正弦波：幅值 ±5000（经 ICD scale 0.001 后为 ±5V），频率 1Hz
  waveform_gen_.setWaveform(
      etest::core::WaveformGenerator::WaveformType::Sine, 5000.0, 1.0, 0.0);
}

double ADChannelSimulator::readChannelValue(int channel) {
  Q_UNUSED(channel);
  ++sample_counter_;
  return waveform_gen_.generate(sample_counter_, 1000.0);
}

void ADChannelSimulator::setFixedValue(double value) {
  waveform_gen_.setFixed(value);
}

void ADChannelSimulator::setWaveform(
    etest::core::WaveformGenerator::WaveformType type,
    double amplitude, double frequency, double offset) {
  waveform_gen_.setWaveform(type, amplitude, frequency, offset);
}

void ADChannelSimulator::setSeries(const QVector<double>& data) {
  waveform_gen_.setSeries(data);
}

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
  signal_resolver_ = std::make_unique<SignalResolver>(&signal_registry_, icdRepo);
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

  // 建立 deviceName → deviceId 映射（连接中使用 deviceName，查找需用 deviceId）
  QHash<QString, QString> nameToId;
  for (const auto& devVal : devices) {
    QJsonObject dev = devVal.toObject();
    QString id = dev["id"].toString();
    QString name = dev["name"].toString();
    if (!id.isEmpty() && !name.isEmpty()) {
      nameToId.insert(name, id);
    }
  }

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
      if (conn["product"].toString() == productName &&
          conn["port"].toString() == portName) {
        matchedConn = conn;
        found = true;
        break;
      }
    }
    if (!found) {
      // 未连接的端口跳过
      continue;
    }

    QString deviceName = matchedConn["device"].toString();
    QString devicePortName = matchedConn["devicePort"].toString();

    // 根据 deviceName（显示名）查找设备，通过 nameToId 转为 deviceId
    QJsonObject matchedDevice;
    bool devFound = false;
    QString deviceId = nameToId.value(deviceName);
    if (!deviceId.isEmpty()) {
      for (const auto& devVal : devices) {
        QJsonObject dev = devVal.toObject();
        if (dev["id"].toString() == deviceId) {
          matchedDevice = dev;
          devFound = true;
          break;
        }
      }
    }
    if (!devFound) {
      LOG_WARN("HARDWARE", "MockUUT 设备未找到: name={}", deviceName.toStdString());
      continue;
    }

    deviceId = matchedDevice["id"].toString();
    QString deviceType = matchedDevice["deviceType"].toString();

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
              if (r.contains("responseHex")) {
                // 旧格式兼容：frameId + responseHex
                int frameId = r["frameId"].toInt();
                QString hex = r["responseHex"].toString();
                QByteArray bytes =
                    QByteArray::fromHex(hex.remove(' ').toLatin1());
                sim->setResponseConfig(frameId, bytes);
                continue;
              }
              // 新格式：frameName + replyFrameName + fieldValues[]
              QString frameName = r["frameName"].toString();
              QString replyFrameName = r["replyFrameName"].toString();
              QJsonArray fieldValues = r["fieldValues"].toArray();

              // 从 frameName 查 ICD 得到收到帧的 frameId
              QVector<int> ids;
              if (!resolveFrameNamesToIds({frameName}, ids) || ids.isEmpty()) {
                LOG_WARN("ENGINE",
                         "MockUUT: 无法解析触发帧名 frame={}",
                         frameName.toStdString());
                continue;
              }

              // 逐字段编码为回复帧字节，按 bitOffset 合并
              QByteArray frameBytes;
              for (const auto& fvVal : fieldValues) {
                QJsonObject fv = fvVal.toObject();
                QString nodePath = fv["nodePath"].toString();
                double engValue = fv["engValue"].toDouble();

                ResolvedSignal signal =
                    signal_resolver_->buildFromIcd(replyFrameName, nodePath);
                if (!signal.valid) {
                  LOG_WARN("ENGINE",
                           "MockUUT: 无法解析回复帧字段 frame={} node={}",
                           replyFrameName.toStdString(),
                           nodePath.toStdString());
                  continue;
                }
                QByteArray fieldBytes =
                    signal_codec_.encodeToFrame(engValue, signal);
                if (frameBytes.size() < fieldBytes.size()) {
                  int oldSize = frameBytes.size();
                  frameBytes.resize(fieldBytes.size());
                  for (int k = oldSize; k < frameBytes.size(); ++k) {
                    frameBytes[k] = '\0';
                  }
                }
                for (int i = 0; i < fieldBytes.size(); ++i) {
                  frameBytes[i] = static_cast<char>(
                      static_cast<unsigned char>(frameBytes[i]) |
                      static_cast<unsigned char>(fieldBytes[i]));
                }
              }
              sim->setResponseConfig(ids[0], frameBytes);
            }
          }
        }
        uut->addFrameSimulator(std::move(sim));
      }
    } else if (deviceType == QStringLiteral("ad")) {
      // AD 通道型端口
      auto sim = buildChannelSimulator(port, matchedConn, matchedDevice);
      if (sim) {
        // 应用 AD 配置
        for (const auto& respVal : frame_responses_) {
          QJsonObject resp = respVal.toObject();
          if (resp["productName"].toString() == productName &&
              resp["deviceId"].toString() == deviceId &&
              resp["port"].toString() == portName) {
            auto* adSim = dynamic_cast<ADChannelSimulator*>(sim.get());
            if (!adSim) {
              break;
            }
            if (resp.contains("mode")) {
              // 新格式：按 mode 配置 WaveformGenerator
              QString mode = resp["mode"].toString();
              if (mode == "fixed") {
                adSim->setFixedValue(resp["fixedValue"].toDouble());
              } else if (mode == "waveform") {
                QJsonObject wf = resp["waveform"].toObject();
                auto type = etest::core::WaveformGenerator::WaveformType::Sine;
                QString typeStr = wf["type"].toString();
                if (typeStr == "square") {
                  type = etest::core::WaveformGenerator::WaveformType::Square;
                } else if (typeStr == "triangle") {
                  type = etest::core::WaveformGenerator::WaveformType::Triangle;
                }
                adSim->setWaveform(type, wf["amplitude"].toDouble(),
                                   wf["frequency"].toDouble(),
                                   wf["offset"].toDouble());
              } else if (mode == "series") {
                QJsonArray seriesArr = resp["series"].toArray();
                QVector<double> series;
                for (const auto& sv : seriesArr) {
                  series.append(sv.toDouble());
                }
                adSim->setSeries(series);
              }
            } else if (resp.contains("fixedValue")) {
              // 旧格式兼容：fixedValue != 0 用固定值，fixedValue == 0 也用固定值（0V）
              double fv = resp["fixedValue"].toDouble();
              adSim->setFixedValue(fv);
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
  QString deviceType = device["deviceType"].toString();
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
  Q_UNUSED(port);
  QString deviceType = device["deviceType"].toString();
  QString deviceId = device["id"].toString();

  // 从连接获取设备端口名，在设备端口中找到对应的 boundFrames
  QString devicePortName = connection["devicePort"].toString();
  QJsonArray devPorts = device["ports"].toArray();
  QJsonObject matchedDevPort;
  for (const auto& dpVal : devPorts) {
    QJsonObject dp = dpVal.toObject();
    if (dp["name"].toString() == devicePortName) {
      matchedDevPort = dp;
      break;
    }
  }
  if (matchedDevPort.isEmpty()) {
    LOG_WARN("HARDWARE", "设备 {} 中未找到端口 {}", deviceId.toStdString(), devicePortName.toStdString());
    return nullptr;
  }

  // 解析 boundFrames → frameId（从设备端口读取，UUT 端口无此字段）
  QVector<int> frameIds;
  QStringList recvNames = frameNamesFromArray(matchedDevPort["boundFrames"].toArray());
  if (!resolveFrameNamesToIds(recvNames, frameIds)) {
    return nullptr;
  }

  int frameId = frameIds.isEmpty() ? 0 : frameIds.first();
  int channel = matchedDevPort["channel"].toInt(0);

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
