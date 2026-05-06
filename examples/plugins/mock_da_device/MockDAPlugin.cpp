#include "MockDAPlugin.h"

#include "logger/Logger.h"

namespace etest {
namespace examples {

using namespace core::plugin;
using namespace core::logger;

MockDAPlugin::MockDAPlugin()
    : channel_values_(kChannelCount, 0.0) {
  meta_.id = "etest.plugin.device.mock_da";
  meta_.name = "Mock DA输出设备";
  meta_.version = "1.0.0";
  meta_.description = "模拟4通道16位DA输出设备，用于验证设备插件框架";
  meta_.author = "etest";
  meta_.category = "device";
  meta_.device_type = "da";
  meta_.device_channels = kChannelCount;
}

MockDAPlugin::~MockDAPlugin() = default;

bool MockDAPlugin::initialize() {
  LOG_INFO("MOCK_DA", "Mock DA插件初始化完成");
  return true;
}

bool MockDAPlugin::start() {
  running_ = true;
  LOG_INFO("MOCK_DA", "Mock DA插件已启动");
  return true;
}

void MockDAPlugin::stop() {
  running_ = false;
  LOG_INFO("MOCK_DA", "Mock DA插件已停止");
}

void MockDAPlugin::uninitialize() {
  LOG_INFO("MOCK_DA", "Mock DA插件已反初始化");
}

PluginMetaData MockDAPlugin::metaData() const { return meta_; }
bool MockDAPlugin::isRunning() const { return running_; }

bool MockDAPlugin::openDevice() {
  device_opened_ = true;
  LOG_INFO("MOCK_DA", "Mock DA设备已打开");
  return true;
}

void MockDAPlugin::closeDevice() {
  device_opened_ = false;
  LOG_INFO("MOCK_DA", "Mock DA设备已关闭");
}

DeviceInfo MockDAPlugin::deviceInfo() const {
  DeviceInfo info;
  info.channel_count = kChannelCount;
  info.resolution = kResolution;
  info.model = "MOCK-DA-4CH-16BIT";
  info.manufacturer = "ETest Mock";
  return info;
}

DeviceStatus MockDAPlugin::deviceStatus() const {
  return device_opened_ ? DeviceStatus::Online : DeviceStatus::Offline;
}

bool MockDAPlugin::writeChannel(int channel, double value) {
  if (!device_opened_) return false;
  if (channel < 0 || channel >= kChannelCount) return false;
  channel_values_[channel] = value;
  return true;
}

double MockDAPlugin::readbackChannel(int channel) const {
  if (!device_opened_) return 0.0;
  if (channel < 0 || channel >= kChannelCount) return 0.0;
  return channel_values_[channel];
}

}  // namespace examples
}  // namespace etest
