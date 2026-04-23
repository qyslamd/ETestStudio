#ifndef ETEST_CORE_PLUGIN_IPLUGIN_H_
#define ETEST_CORE_PLUGIN_IPLUGIN_H_

#include <QtPlugin>
#include "PluginMetaData.h"

namespace etest {
namespace core {
namespace plugin {

class IPlugin {
 public:
  virtual ~IPlugin() = default;

  // 生命周期
  virtual bool initialize() = 0;
  virtual bool start() = 0;
  virtual void stop() = 0;
  virtual void uninitialize() = 0;

  // 元数据
  virtual PluginMetaData metaData() const = 0;

  // 状态查询
  virtual bool isRunning() const = 0;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

Q_DECLARE_INTERFACE(etest::core::plugin::IPlugin,
                    "etest.core.plugin.IPlugin/1.0")

#endif  // ETEST_CORE_PLUGIN_IPLUGIN_H_
