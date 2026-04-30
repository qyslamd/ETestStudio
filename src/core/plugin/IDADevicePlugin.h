#ifndef ETEST_CORE_PLUGIN_IDA_DEVICE_PLUGIN_H_
#define ETEST_CORE_PLUGIN_IDA_DEVICE_PLUGIN_H_

#include "IDevicePlugin.h"

namespace etest {
namespace core {
namespace plugin {

class IDADevicePlugin : public IDevicePlugin {
 public:
  ~IDADevicePlugin() override = default;

  virtual bool writeChannel(int channel, double value) = 0;
  virtual double readbackChannel(int channel) const = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_INTERFACE(etest::core::plugin::IDADevicePlugin,
                    "etest.core.plugin.IDADevicePlugin/1.0")

#endif  // ETEST_CORE_PLUGIN_IDA_DEVICE_PLUGIN_H_
