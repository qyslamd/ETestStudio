#ifndef ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_
#define ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_

#include "IDevicePlugin.h"
#include <QVector>

namespace etest {
namespace core {
namespace plugin {

class IADevicePlugin : public IDevicePlugin {
 public:
  ~IADevicePlugin() override = default;

  virtual double readChannel(int channel) = 0;
  virtual QVector<double> readAllChannels() = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_INTERFACE(etest::core::plugin::IADevicePlugin,
                    "etest.core.plugin.IADevicePlugin/1.0")

#endif  // ETEST_CORE_PLUGIN_IA_DEVICE_PLUGIN_H_
