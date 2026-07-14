#ifndef ETEST_CORE_PLUGIN_ISERIAL_DEVICE_PLUGIN_H_
#define ETEST_CORE_PLUGIN_ISERIAL_DEVICE_PLUGIN_H_

#include "IDevicePlugin.h"
#include <QByteArray>

namespace etest {
namespace core {
namespace plugin {

class ISerialDevicePlugin : public IDevicePlugin {
 public:
  ~ISerialDevicePlugin() override = default;

  virtual qint64 writeData(const QByteArray& data) = 0;
  virtual QByteArray readData(int maxBytes = -1) = 0;
  virtual bool setBaudRate(int baudRate) = 0;
  virtual int baudRate() const = 0;
  virtual bool setPortName(const QString& name) = 0;
  virtual QString portName() const = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_INTERFACE(etest::core::plugin::ISerialDevicePlugin,
                    "etest.core.plugin.ISerialDevicePlugin/1.0")

#endif  // ETEST_CORE_PLUGIN_ISERIAL_DEVICE_PLUGIN_H_
