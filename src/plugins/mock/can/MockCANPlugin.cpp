#include "MockCANPlugin.h"

#include "logger/Logger.h"

namespace etest {
namespace plugins {
namespace mock {

using namespace core::plugin;
using namespace core::logger;

MockCANPlugin::MockCANPlugin() {
  meta_.id = "etest.plugin.device.mock_can";
  meta_.name = "Mock CAN设备";
  meta_.version = "1.0.0";
  meta_.description = "模拟CAN总线设备，用于验证设备插件框架";
  meta_.author = "etest";
  meta_.category = "device";
  meta_.device_type = "can";
  meta_.device_channels = 2;
  meta_.device_function = "CAN";
  meta_.is_mock = true;
}

MockCANPlugin::~MockCANPlugin() = default;

bool MockCANPlugin::initialize() {
  LOG_INFO("MOCK_CAN", "Mock CAN插件初始化完成");
  return true;
}

bool MockCANPlugin::start() {
  running_ = true;
  LOG_INFO("MOCK_CAN", "Mock CAN插件已启动");
  return true;
}

void MockCANPlugin::stop() {
  running_ = false;
  LOG_INFO("MOCK_CAN", "Mock CAN插件已停止");
}

void MockCANPlugin::uninitialize() {
  LOG_INFO("MOCK_CAN", "Mock CAN插件已反初始化");
}

PluginMetaData MockCANPlugin::metaData() const { return meta_; }
bool MockCANPlugin::isRunning() const { return running_; }

bool MockCANPlugin::openDevice() {
  device_opened_ = true;
  msg_data_.clear();
  LOG_INFO("MOCK_CAN", "Mock CAN设备已打开");
  return true;
}

void MockCANPlugin::closeDevice() {
  device_opened_ = false;
  msg_data_.clear();
  LOG_INFO("MOCK_CAN", "Mock CAN设备已关闭");
}

DeviceInfo MockCANPlugin::deviceInfo() const {
  DeviceInfo info;
  info.channel_count = 2;
  info.resolution = 0;
  info.model = "MOCK-CAN-2CH";
  info.manufacturer = "ETest Mock";
  return info;
}

DeviceStatus MockCANPlugin::deviceStatus() const {
  return device_opened_ ? DeviceStatus::Online : DeviceStatus::Offline;
}

bool MockCANPlugin::sendMessage(quint32 id, const QByteArray& data, bool extended) {
  Q_UNUSED(extended);
  if (!device_opened_) return false;
  msg_data_[id] = data;
  return true;
}

QByteArray MockCANPlugin::receiveMessage(quint32 id) {
  if (!device_opened_) return QByteArray();
  return msg_data_.value(id, QByteArray());
}

bool MockCANPlugin::setBitrate(int bitrate) {
  bitrate_ = bitrate;
  return true;
}

int MockCANPlugin::bitrate() const { return bitrate_; }

}  // namespace mock
}  // namespace plugins
}  // namespace etest
