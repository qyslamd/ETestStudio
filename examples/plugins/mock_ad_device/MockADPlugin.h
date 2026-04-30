#ifndef ETEST_EXAMPLES_MOCK_AD_PLUGIN_H_
#define ETEST_EXAMPLES_MOCK_AD_PLUGIN_H_

#include <QObject>
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

 private:
  double generateMockValue(int channel) const;

  bool running_ = false;
  bool device_opened_ = false;
  core::plugin::PluginMetaData meta_;
  static constexpr int kChannelCount = 8;
  static constexpr int kResolution = 16;
};

}  // namespace examples
}  // namespace etest

#endif  // ETEST_EXAMPLES_MOCK_AD_PLUGIN_H_
