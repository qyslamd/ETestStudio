#ifndef ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_
#define ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_

#include "IDevicePlugin.h"
#include <QVector>

namespace etest {
namespace core {
namespace plugin {

enum class WaveformType {
  Sine,
  Square,
  Triangle,
  DC
};

struct ADChannelConfig {
  WaveformType waveform = WaveformType::Sine;
  double frequency = 1.0;       // Hz
  double amplitude = 1.0;       // 归一化幅值 [0, 1]
  double offset = 0.0;          // 归一化偏移 [-1, 1]
  double noise_level = 0.05;    // 噪声幅度 [0, 1]
};

class IADevicePlugin : public IDevicePlugin {
 public:
  ~IADevicePlugin() override = default;

  // 读取
  virtual double readChannel(int channel) = 0;
  virtual QVector<double> readAllChannels() = 0;

  // 采集控制
  virtual bool startAcquisition() = 0;
  virtual void stopAcquisition() = 0;
  virtual bool isAcquiring() const = 0;

  // 采样率
  virtual bool setSampleRate(double rate) = 0;
  virtual double sampleRate() const = 0;

  // 通道信号配置
  virtual bool setChannelConfig(int channel, const ADChannelConfig& config) = 0;
  virtual ADChannelConfig channelConfig(int channel) const = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_INTERFACE(etest::core::plugin::IADevicePlugin,
                    "etest.core.plugin.IADevicePlugin/1.0")

#endif  // ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_
