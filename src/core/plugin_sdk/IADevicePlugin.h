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
  DC,         // 直流耦合
  AC,         // 交流耦合
  ICP_DC,     // ICP 直流耦合
  ICP_AC,     // ICP 交流耦合
  GND_DC,     // GND 直流耦合（自检）
  GND_AC      // GND 交流耦合（自检）
};

enum class ADTriggerMode {
  Software,     // 软件触发
  ExternalPos,  // 外部正沿触发
  ExternalNeg,  // 外部负沿触发
  SystemPos,    // 系统正沿触发
  SystemNeg,    // 系统负沿触发
  StarPos,      // PXI 星型触发正沿
  StarNeg,      // PXI 星型触发负沿
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

enum class ADReadMode {
  Direct,  // 普通寄存器读取
  DMA,     // DMA 传输
  MAP,     // 内存映射
  FIFO     // FIFO 方式
};

enum class ADMemoryMode {
  ChannelStorage,  // 按通道号存放
  ScanStorage      // 按扫描表顺序存放
};

// ============ 配置结构 ============

struct ADChannelConfig {
  double range = 10.0;                    // 量程 (V)，如 ±10V → range=10.0
  ADCoupling coupling = ADCoupling::DC;   // 耦合方式
  double icp_current = 0.004;             // ICP 电流值 (A)，默认 4mA

  // 通道模式
  bool differential = false;              // true=差分输入，false=单端输入
  int gain = 1;                           // 可编程增益 (1/2/20/200 等)
  int filter = 0;                         // 抗混叠滤波器档位，0=不滤波

  // 内部触发参数（仅 trigger_mode == Internal 时有效）
  ADTriggerEdge trigger_edge = ADTriggerEdge::Rising;
  double trigger_level = 0.0;             // 触发电平 (V)
};

struct ADTriggerConfig {
  ADTriggerMode mode = ADTriggerMode::Software;
  bool enabled = true;                    // 触发使能
  int pretrigger_length = 0;              // 预触发采样点数
  int trigger_length = 1024;              // 触发后采样点数
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

  // --- 通道配置（量程、耦合、差分、增益、滤波、ICP、内部触发参数）---
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

  // --- 数据传输模式 ---
  virtual bool setReadMode(ADReadMode mode) = 0;
  virtual ADReadMode readMode() const = 0;

  // --- 存储模式（通道存储 / 扫描表存储）---
  virtual bool setMemoryMode(ADMemoryMode mode) = 0;
  virtual ADMemoryMode memoryMode() const = 0;

  // --- 扫描表配置（仅 ScanStorage 模式有效）---
  virtual bool setScanList(const QVector<int>& scanList) = 0;
  virtual QVector<int> scanList() const = 0;
  virtual int maxScanDepth() const = 0;

  // --- 单点读取（CVT模式：读取各通道最新采样值的电压）---
  virtual double readChannel(int channel) = 0;
  virtual QVector<double> readAllChannels() = 0;

  // --- 批量读取（读取 count 个采样点的电压值）---
  virtual QVector<double> readChannelData(int channel, int count) = 0;
  virtual QVector<double> readAllChannelsData(int count) = 0;

  // --- 原始 AD 码读取（返回未经转换的 ADC 原始值）---
  virtual QVector<qint16> readChannelRaw(int channel, int count) = 0;
  virtual QVector<qint16> readAllChannelsRaw(int count) = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_METATYPE(etest::core::plugin::ADChannelConfig)
Q_DECLARE_METATYPE(etest::core::plugin::ADTriggerConfig)
Q_DECLARE_METATYPE(etest::core::plugin::ADSampleStatus)
Q_DECLARE_METATYPE(etest::core::plugin::ADReadMode)
Q_DECLARE_METATYPE(etest::core::plugin::ADMemoryMode)

Q_DECLARE_INTERFACE(etest::core::plugin::IADevicePlugin,
                    "etest.core.plugin.IADevicePlugin/3.0")

#endif  // ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_
