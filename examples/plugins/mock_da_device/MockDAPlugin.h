#ifndef ETEST_EXAMPLES_MOCK_DA_PLUGIN_H_
#define ETEST_EXAMPLES_MOCK_DA_PLUGIN_H_

#include <QObject>
#include <QVector>
#include "IDADevicePlugin.h"

namespace etest {
namespace examples {

class MockDAPlugin : public QObject, public core::plugin::IDADevicePlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "etest.core.plugin.IDADevicePlugin/1.0" FILE "mock_da_device.json")
  Q_INTERFACES(etest::core::plugin::IPlugin etest::core::plugin::IDevicePlugin etest::core::plugin::IDADevicePlugin)

 public:
  MockDAPlugin();
  ~MockDAPlugin() override;

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

  // IDADevicePlugin
  bool writeChannel(int channel, double value) override;
  double readbackChannel(int channel) const override;

 private:
  bool running_ = false;
  bool device_opened_ = false;
  core::plugin::PluginMetaData meta_;
  QVector<double> channel_values_;
  static constexpr int kChannelCount = 4;
  static constexpr int kResolution = 16;
};

}  // namespace examples
}  // namespace etest

#endif  // ETEST_EXAMPLES_MOCK_DA_PLUGIN_H_
