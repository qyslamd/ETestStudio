#ifndef ETEST_CORE_PLUGIN_IARINC429_PLUGIN_H_
#define ETEST_CORE_PLUGIN_IARINC429_PLUGIN_H_

#include "IDevicePlugin.h"
#include <QByteArray>

namespace etest {
namespace core {
namespace plugin {

enum class Arinc429Speed {
  Low,
  High
};

class IArinc429Plugin : public IDevicePlugin {
 public:
  ~IArinc429Plugin() override = default;

  virtual bool sendLabel(int label, const QByteArray& data) = 0;
  virtual QByteArray receiveLabel(int label) = 0;
  virtual bool setSpeed(Arinc429Speed speed) = 0;
  virtual Arinc429Speed speed() const = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_INTERFACE(etest::core::plugin::IArinc429Plugin,
                    "etest.core.plugin.IArinc429Plugin/1.0")

#endif  // ETEST_CORE_PLUGIN_IARINC429_PLUGIN_H_
