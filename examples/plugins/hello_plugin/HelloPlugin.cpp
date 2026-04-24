#include "HelloPlugin.h"
#include "logger/Logger.h"

using namespace etest::core::logger;

namespace etest::example {
HelloPlugin::HelloPlugin() {
  meta_.id = "etest.plugin.example.hello";
  meta_.name = "Hello示例插件";
  meta_.version = "1.0.0";
  meta_.description = "通用插件框架验证用示例插件";
  meta_.author = "slamdd";
  meta_.category = "example";
}

HelloPlugin::~HelloPlugin() = default;

bool HelloPlugin::initialize() {
  LOG_INFO("HELLO_PLUGIN", "Hello from plugin! 初始化完成");
  return true;
}

bool HelloPlugin::start() {
  running_ = true;
  LOG_INFO("HELLO_PLUGIN", "Hello插件已启动");
  return true;
}

void HelloPlugin::stop() {
  running_ = false;
  LOG_INFO("HELLO_PLUGIN", "Hello插件已停止");
}

void HelloPlugin::uninitialize() {
  LOG_INFO("HELLO_PLUGIN", "Hello插件已反初始化");
}

etest::core::plugin::PluginMetaData HelloPlugin::metaData() const {
  return meta_;
}

bool HelloPlugin::isRunning() const {
  return running_;
}

}  // namespace etest::example
