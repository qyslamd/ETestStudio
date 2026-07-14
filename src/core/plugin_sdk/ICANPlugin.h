#ifndef ETEST_CORE_PLUGIN_ICAN_PLUGIN_H_
#define ETEST_CORE_PLUGIN_ICAN_PLUGIN_H_

#include "IDevicePlugin.h"
#include <QByteArray>

namespace etest {
namespace core {
namespace plugin {

class ICANPlugin : public IDevicePlugin {
 public:
  ~ICANPlugin() override = default;

  virtual bool sendMessage(quint32 id, const QByteArray& data, bool extended = false) = 0;
  virtual QByteArray receiveMessage(quint32 id) = 0;
  virtual bool setBitrate(int bitrate) = 0;
  virtual int bitrate() const = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_INTERFACE(etest::core::plugin::ICANPlugin,
                    "etest.core.plugin.ICANPlugin/1.0")

#endif  // ETEST_CORE_PLUGIN_ICAN_PLUGIN_H_
