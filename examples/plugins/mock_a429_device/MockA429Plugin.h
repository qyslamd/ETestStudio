#ifndef ETEST_EXAMPLES_MOCK_A429_PLUGIN_H_
#define ETEST_EXAMPLES_MOCK_A429_PLUGIN_H_

#include <QObject>
#include <QMap>
#include "IArinc429Plugin.h"

namespace etest {
namespace examples {

class MockA429Plugin : public QObject, public core::plugin::IArinc429Plugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "etest.core.plugin.IArinc429Plugin/1.0" FILE "mock_a429_device.json")
  Q_INTERFACES(etest::core::plugin::IPlugin etest::core::plugin::IDevicePlugin etest::core::plugin::IArinc429Plugin)

 public:
  MockA429Plugin();
  ~MockA429Plugin() override;

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

  // IArinc429Plugin
  bool sendLabel(int label, const QByteArray& data) override;
  QByteArray receiveLabel(int label) override;
  bool setSpeed(core::plugin::Arinc429Speed speed) override;
  core::plugin::Arinc429Speed speed() const override;

 private:
  bool running_ = false;
  bool device_opened_ = false;
  core::plugin::PluginMetaData meta_;
  QMap<int, QByteArray> label_data_;
  core::plugin::Arinc429Speed speed_ = core::plugin::Arinc429Speed::Low;
};

}  // namespace examples
}  // namespace etest

#endif  // ETEST_EXAMPLES_MOCK_A429_PLUGIN_H_
