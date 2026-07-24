#ifndef ETEST_ENGINE_MOCK_UUT_BUILDER_H_
#define ETEST_ENGINE_MOCK_UUT_BUILDER_H_

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <optional>
#include <vector>

#include "MockTypes.h"

namespace icd {
class Repository;
}  // namespace icd

namespace etest::engine {

// ============================================================================
// FramePortSimulator — 帧型端口模拟器基类（Serial、CAN、A429）
// ============================================================================

class FramePortSimulator {
 public:
  virtual ~FramePortSimulator() = default;

  // 收到帧 → 返回模拟回复。无配置时返回 std::nullopt
  virtual std::optional<MockResponse> onFrameReceived(
      int frameId, const QByteArray& frameData) = 0;

  // 设置指定 frameId 的回复配置（十六进制字符串转字节数组）
  virtual void setResponseConfig(int frameId, const QByteArray& responseHex) = 0;

  bool hasReceiveFrame(int frameId) const;
  const QString& deviceId() const { return device_id_; }
  const QString& portName() const { return port_name_; }

 protected:
  FramePortSimulator(const QString& portName, const QString& deviceId,
                     const QVector<int>& receiveFrameIds,
                     const QVector<int>& sendFrameIds);

  QString device_id_;
  QString port_name_;
  QVector<int> receive_frame_ids_;
  QVector<int> send_frame_ids_;
  QHash<int, QByteArray> response_config_;
};

// ============================================================================
// SerialPortSimulator — 串口端口模拟器
// ============================================================================

class SerialPortSimulator : public FramePortSimulator {
 public:
  SerialPortSimulator(const QString& portName, const QString& deviceId,
                      const QVector<int>& receiveFrameIds,
                      const QVector<int>& sendFrameIds);

  std::optional<MockResponse> onFrameReceived(
      int frameId, const QByteArray& frameData) override;

  void setResponseConfig(int frameId, const QByteArray& responseHex);
};

// ============================================================================
// CANPortSimulator — CAN 端口模拟器
// ============================================================================

class CANPortSimulator : public FramePortSimulator {
 public:
  CANPortSimulator(const QString& portName, const QString& deviceId,
                   const QVector<int>& receiveFrameIds,
                   const QVector<int>& sendFrameIds);

  std::optional<MockResponse> onFrameReceived(
      int frameId, const QByteArray& frameData) override;

  void setResponseConfig(int frameId, const QByteArray& responseHex);
};

// ============================================================================
// A429PortSimulator — A429 端口模拟器
// ============================================================================

class A429PortSimulator : public FramePortSimulator {
 public:
  A429PortSimulator(const QString& portName, const QString& deviceId,
                    const QVector<int>& receiveFrameIds,
                    const QVector<int>& sendFrameIds);

  std::optional<MockResponse> onFrameReceived(
      int frameId, const QByteArray& frameData) override;

  void setResponseConfig(int frameId, const QByteArray& responseHex);
};

// ============================================================================
// ChannelPortSimulator — 通道型端口模拟器基类（AD、DA）
// ============================================================================

class ChannelPortSimulator {
 public:
  virtual ~ChannelPortSimulator() = default;
  virtual double readChannelValue(int channel) = 0;

  int frameId() const { return frame_id_; }
  const QString& deviceId() const { return device_id_; }

 protected:
  ChannelPortSimulator(const QString& deviceId, int frameId);

  QString device_id_;
  int frame_id_ = 0;
};

// ============================================================================
// ADChannelSimulator — AD 通道模拟器
// ============================================================================

class ADChannelSimulator : public ChannelPortSimulator {
 public:
  ADChannelSimulator(const QString& deviceId, int frameId, int channel);

  double readChannelValue(int channel) override;
  void setFixedValue(double value);

 private:
  int channel_;
  double fixed_value_ = 0.0;
  qint64 sample_counter_ = 0;
  static constexpr double kFrequency = 1.0;    // 正弦波频率 1Hz，2048 点约画 2 个周期
  static constexpr double kAmplitude = 5000.0;  // 幅值 ±5000（经 ICD scale 0.001 后为 ±5V）
};

// ============================================================================
// MockUUT — 对应一个 UUT（product）
// ============================================================================

class MockUUT {
 public:
  explicit MockUUT(const QString& name);

  const QString& name() const { return name_; }

  void addFrameSimulator(std::unique_ptr<FramePortSimulator> sim);
  void addChannelSimulator(std::unique_ptr<ChannelPortSimulator> sim);

  // 在自身模拟器中查找，同时匹配 deviceId 和 frameId
  FramePortSimulator* findFrameSimulator(const QString& deviceId,
                                         int frameId) const;
  ChannelPortSimulator* findChannelSimulator(int frameId) const;

  // 收到设备发来的帧 → 返回模拟回复。无配置时返回 std::nullopt
  std::optional<MockResponse> onFrameWritten(const QString& deviceId,
                                              int frameId,
                                              const QByteArray& frameData);

 private:
  QString name_;
  std::vector<std::unique_ptr<FramePortSimulator>> frame_sims_;
  std::vector<std::unique_ptr<ChannelPortSimulator>> channel_sims_;
};

// ============================================================================
// MockUUTBuilder — 从拓扑 + ICD 组装 MockUUT
// ============================================================================

class MockUUTBuilder {
 public:
  MockUUTBuilder(icd::Repository* icdRepo, const QJsonObject& topologyDoc);

  // 构建所有 MockUUT
  // 返回 false 表示构建失败（帧名解析失败等硬错误），此时 out 为空
  // 调用 lastError() 获取错误详情
  bool buildAll(std::vector<std::unique_ptr<MockUUT>>& out);

  // 加载响应配置。filePath 是 mock/MockResponses.json 的完整路径
  // 必须在 buildAll() 之前调用
  void loadResponseConfigFile(const QString& filePath);

  QString lastError() const { return last_error_; }

 private:
  bool buildSingleUUT(const QJsonObject& product,
                      const QJsonArray& connections,
                      const QJsonArray& devices,
                      std::unique_ptr<MockUUT>& out);
  std::unique_ptr<FramePortSimulator> buildFrameSimulator(
      const QJsonObject& port, const QJsonObject& connection,
      const QJsonObject& device);
  std::unique_ptr<ChannelPortSimulator> buildChannelSimulator(
      const QJsonObject& port, const QJsonObject& connection,
      const QJsonObject& device);

  // 将帧名列表解析为帧 ID 列表
  // 任一帧名在 ICD Repository 中找不到 → 返回 false 并设置 last_error_
  bool resolveFrameNamesToIds(const QStringList& frameNames,
                              QVector<int>& outIds);

  // 从端口 JSON 中提取 sendFrames/receiveFrames 的 name 列表
  QStringList frameNamesFromArray(const QJsonArray& arr) const;

  icd::Repository* icd_repo_;
  QJsonObject topology_doc_;
  QJsonArray frame_responses_;
  QString last_error_;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_MOCK_UUT_BUILDER_H_
