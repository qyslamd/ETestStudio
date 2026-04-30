#ifndef ETEST_CORE_PLUGIN_IDEVICE_PLUGIN_H_
#define ETEST_CORE_PLUGIN_IDEVICE_PLUGIN_H_

#include "IPlugin.h"

namespace etest {
namespace core {
namespace plugin {

enum class DeviceStatus {
  Offline,
  Online,
  Error
};

struct DeviceInfo {
  int channel_count = 0;
  int resolution = 0;
  QString model;
  QString manufacturer;
};

class IDevicePlugin : public IPlugin {
 public:
  ~IDevicePlugin() override = default;

  virtual bool openDevice() = 0;
  virtual void closeDevice() = 0;
  virtual DeviceInfo deviceInfo() const = 0;
  virtual DeviceStatus deviceStatus() const = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_INTERFACE(etest::core::plugin::IDevicePlugin,
                    "etest.core.plugin.IDevicePlugin/1.0")

#endif  // ETEST_CORE_PLUGIN_IDEVICE_PLUGIN_H_
