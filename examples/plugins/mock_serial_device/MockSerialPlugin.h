#ifndef ETEST_EXAMPLES_MOCK_SERIAL_PLUGIN_H_
#define ETEST_EXAMPLES_MOCK_SERIAL_PLUGIN_H_

#include <QObject>
#include <QByteArray>
#include "ISerialDevicePlugin.h"

namespace etest {
namespace examples {

class MockSerialPlugin : public QObject, public core::plugin::ISerialDevicePlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "etest.core.plugin.ISerialDevicePlugin/1.0" FILE "mock_serial_device.json")
  Q_INTERFACES(etest::core::plugin::IPlugin etest::core::plugin::IDevicePlugin etest::core::plugin::ISerialDevicePlugin)

 public:
  MockSerialPlugin();
  ~MockSerialPlugin() override;

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

  // ISerialDevicePlugin
  qint64 writeData(const QByteArray& data) override;
  QByteArray readData(int maxBytes = -1) override;
  bool setBaudRate(int baudRate) override;
  int baudRate() const override;
  bool setPortName(const QString& name) override;
  QString portName() const override;

 private:
  bool running_ = false;
  bool device_opened_ = false;
  core::plugin::PluginMetaData meta_;
  QByteArray rx_buffer_;
  int baud_rate_ = 115200;
  QString port_name_ = QStringLiteral("COM1");
};

}  // namespace examples
}  // namespace etest

#endif  // ETEST_EXAMPLES_MOCK_SERIAL_PLUGIN_H_
