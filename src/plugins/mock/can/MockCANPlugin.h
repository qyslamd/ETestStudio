#ifndef ETEST_PLUGINS_MOCK_CAN_PLUGIN_H_
#define ETEST_PLUGINS_MOCK_CAN_PLUGIN_H_

#include <QObject>
#include <QMap>
#include "ICANPlugin.h"

namespace etest {
namespace plugins {
namespace mock {

class MockCANPlugin : public QObject, public core::plugin::ICANPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "etest.core.plugin.ICANPlugin/1.0" FILE "mock_can_device.json")
  Q_INTERFACES(etest::core::plugin::IPlugin etest::core::plugin::IDevicePlugin etest::core::plugin::ICANPlugin)

 public:
  MockCANPlugin();
  ~MockCANPlugin() override;

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

  // ICANPlugin
  bool sendMessage(quint32 id, const QByteArray& data, bool extended = false) override;
  QByteArray receiveMessage(quint32 id) override;
  bool setBitrate(int bitrate) override;
  int bitrate() const override;

 private:
  bool running_ = false;
  bool device_opened_ = false;
  core::plugin::PluginMetaData meta_;
  QMap<quint32, QByteArray> msg_data_;
  int bitrate_ = 500000;
};

}  // namespace mock
}  // namespace plugins
}  // namespace etest

#endif  // ETEST_PLUGINS_MOCK_CAN_PLUGIN_H_
