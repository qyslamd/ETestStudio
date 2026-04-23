#ifndef ETEST_EXAMPLE_HELLO_PLUGIN_H_
#define ETEST_EXAMPLE_HELLO_PLUGIN_H_

#include <QtPlugin>
#include "plugin/IPlugin.h"

namespace etest {
namespace example {

class HelloPlugin : public QObject, public etest::core::plugin::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "etest.core.plugin.IPlugin/1.0"
                        FILE "hello_plugin.json")
  Q_INTERFACES(etest::core::plugin::IPlugin)

 public:
  HelloPlugin();
  ~HelloPlugin() override;

  bool initialize() override;
  bool start() override;
  void stop() override;
  void uninitialize() override;

  etest::core::plugin::PluginMetaData metaData() const override;
  bool isRunning() const override;

 private:
  bool running_ = false;
  etest::core::plugin::PluginMetaData meta_;
};

}  // namespace example
}  // namespace etest

#endif  // ETEST_EXAMPLE_HELLO_PLUGIN_H_
