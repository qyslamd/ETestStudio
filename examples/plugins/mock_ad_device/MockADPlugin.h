#ifndef ETEST_EXAMPLES_MOCK_AD_PLUGIN_H_
#define ETEST_EXAMPLES_MOCK_AD_PLUGIN_H_

#include <QObject>
#include <QTimer>
#include "IADevicePlugin.h"

namespace etest {
namespace examples {

class MockADPlugin : public QObject, public core::plugin::IADevicePlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "etest.core.plugin.IADevicePlugin/1.0" FILE "mock_ad_device.json")
  Q_INTERFACES(etest::core::plugin::IPlugin etest::core::plugin::IDevicePlugin etest::core::plugin::IADevicePlugin)

 public:
  MockADPlugin();
  ~MockADPlugin() override;

  // IPlugin
  bool initialize() override;
  bool start() override;
  void stop() override;
  void uninitialize() override;
  core::plugin::PluginMetaData metaData() const override;
  bool isRunning() const override;

  // IDevicePlugin
  bool openDevice() override;
  void closeDevice() override;
  core::plugin::DeviceInfo deviceInfo() const override;
  core::plugin::DeviceStatus deviceStatus() const override;

  // IADevicePlugin
  double readChannel(int channel) override;
  QVector<double> readAllChannels() override;

  bool startAcquisition() override;
  void stopAcquisition() override;
  bool isAcquiring() const override;

  bool setSampleRate(double rate) override;
  double sampleRate() const override;

  bool setChannelConfig(int channel, const core::plugin::ADChannelConfig& config) override;
  core::plugin::ADChannelConfig channelConfig(int channel) const override;

 private:
  void onAcquisitionTick();
  double generateSignal(int channel) const;

  bool running_ = false;
  bool device_opened_ = false;
  bool acquiring_ = false;
  core::plugin::PluginMetaData meta_;

  QTimer* acquisition_timer_ = nullptr;
  double sample_rate_ = 1000.0;
  qint64 sample_counter_ = 0;

  QVector<QVector<double>> ring_buffers_;
  QVector<int> buffer_write_pos_;
  QVector<core::plugin::ADChannelConfig> channel_configs_;

  static constexpr int kChannelCount = 8;
  static constexpr int kResolution = 16;
  static constexpr int kRingBufferSize = 10000;
};

}  // namespace examples
}  // namespace etest

#endif  // ETEST_EXAMPLES_MOCK_AD_PLUGIN_H_
