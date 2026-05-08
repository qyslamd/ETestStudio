#ifndef ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_
#define ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_

#include "IDevicePlugin.h"
#include <QMetaType>
#include <QVector>

namespace etest {
namespace core {
namespace plugin {

// ============ 枚举 ============

enum class ADCoupling {
  DC,  // 直流耦合
  AC   // 交流耦合
};

enum class ADTriggerMode {
  Software,     // 软件触发
  ExternalPos,  // 外部正沿触发
  ExternalNeg,  // 外部负沿触发
  SystemPos,    // 系统正沿触发
  SystemNeg,    // 系统负沿触发
  Internal      // 内部触发（按通道电平/边沿）
};

enum class ADTriggerEdge {
  Rising,   // 上升沿
  Falling   // 下降沿
};

enum class ADSampleStatus {
  Idle,      // 空闲
  Waiting,   // 等待触发
  Sampling,  // 采集中
  Completed  // 采集完成
};

// ============ 配置结构 ============

struct ADChannelConfig {
  double range = 10.0;                    // 量程 (V)，如 ±10V → range=10.0
  ADCoupling coupling = ADCoupling::DC;   // 耦合方式
  bool icp_enabled = false;               // ICP 恒流源使能
  double icp_current = 0.004;             // ICP 电流值 (A)，默认 4mA

  // 内部触发参数（仅 trigger_mode == Internal 时有效）
  ADTriggerEdge trigger_edge = ADTriggerEdge::Rising;
  double trigger_level = 0.0;             // 触发电平 (V)
};

struct ADTriggerConfig {
  ADTriggerMode mode = ADTriggerMode::Software;
  bool enabled = true;  // 触发使能
};

// ============ 接口 ============

class IADevicePlugin : public IDevicePlugin {
 public:
  ~IADevicePlugin() override = default;

  // --- 采样率 ---
  virtual bool setSampleRate(double rate) = 0;
  virtual double sampleRate() const = 0;

  // --- 存储深度 ---
  virtual bool setSampleLength(int length) = 0;
  virtual int sampleLength() const = 0;

  // --- 通道配置（量程、耦合、ICP、内部触发参数）---
  virtual bool setChannelConfig(int channel, const ADChannelConfig& config) = 0;
  virtual ADChannelConfig channelConfig(int channel) const = 0;

  // --- 触发配置 ---
  virtual bool setTriggerConfig(const ADTriggerConfig& config) = 0;
  virtual ADTriggerConfig triggerConfig() const = 0;

  // 软件触发（仅 trigger_mode == Software 时有效）
  virtual bool softwareTrigger() = 0;

  // --- 采集控制 ---
  virtual bool startAcquisition() = 0;
  virtual void stopAcquisition() = 0;
  virtual bool isAcquiring() const = 0;
  virtual ADSampleStatus sampleStatus() const = 0;

  // --- 单点读取（CVT模式：读取各通道最新采样值的电压）---
  virtual double readChannel(int channel) = 0;
  virtual QVector<double> readAllChannels() = 0;

  // --- 批量读取（读取 count 个采样点的电压值）---
  virtual QVector<double> readChannelData(int channel, int count) = 0;
  virtual QVector<double> readAllChannelsData(int count) = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_METATYPE(etest::core::plugin::ADChannelConfig)
Q_DECLARE_METATYPE(etest::core::plugin::ADTriggerConfig)
Q_DECLARE_METATYPE(etest::core::plugin::ADSampleStatus)

Q_DECLARE_INTERFACE(etest::core::plugin::IADevicePlugin,
                    "etest.core.plugin.IADevicePlugin/2.0")

#endif  // ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_
