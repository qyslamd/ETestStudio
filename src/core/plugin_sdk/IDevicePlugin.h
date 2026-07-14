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
  int bus_number = 0;       // PXI 总线号
  int slot_number = 0;      // 设备号
  int card_serial = 0;      // 板卡序列号
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
