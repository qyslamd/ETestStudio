#include "MockA429Plugin.h"

#include "logger/Logger.h"

namespace etest {
namespace examples {

using namespace core::plugin;
using namespace core::logger;

MockA429Plugin::MockA429Plugin() {
  meta_.id = "etest.plugin.device.mock_a429";
  meta_.name = "Mock A429设备";
  meta_.version = "1.0.0";
  meta_.description = "模拟ARINC 429总线设备，用于验证设备插件框架";
  meta_.author = "etest";
  meta_.category = "device";
  meta_.device_type = "a429";
  meta_.device_channels = 2;
}

MockA429Plugin::~MockA429Plugin() = default;

bool MockA429Plugin::initialize() {
  LOG_INFO("MOCK_A429", "Mock A429插件初始化完成");
  return true;
}

bool MockA429Plugin::start() {
  running_ = true;
  LOG_INFO("MOCK_A429", "Mock A429插件已启动");
  return true;
}

void MockA429Plugin::stop() {
  running_ = false;
  LOG_INFO("MOCK_A429", "Mock A429插件已停止");
}

void MockA429Plugin::uninitialize() {
  LOG_INFO("MOCK_A429", "Mock A429插件已反初始化");
}

PluginMetaData MockA429Plugin::metaData() const { return meta_; }
bool MockA429Plugin::isRunning() const { return running_; }

bool MockA429Plugin::openDevice() {
  device_opened_ = true;
  label_data_.clear();
  LOG_INFO("MOCK_A429", "Mock A429设备已打开");
  return true;
}

void MockA429Plugin::closeDevice() {
  device_opened_ = false;
  label_data_.clear();
  LOG_INFO("MOCK_A429", "Mock A429设备已关闭");
}

DeviceInfo MockA429Plugin::deviceInfo() const {
  DeviceInfo info;
  info.channel_count = 2;
  info.resolution = 0;
  info.model = "MOCK-A429-2CH";
  info.manufacturer = "ETest Mock";
  return info;
}

DeviceStatus MockA429Plugin::deviceStatus() const {
  return device_opened_ ? DeviceStatus::Online : DeviceStatus::Offline;
}

bool MockA429Plugin::sendLabel(int label, const QByteArray& data) {
  if (!device_opened_) return false;
  label_data_[label] = data;
  return true;
}

QByteArray MockA429Plugin::receiveLabel(int label) {
  if (!device_opened_) return QByteArray();
  return label_data_.value(label, QByteArray());
}

bool MockA429Plugin::setSpeed(Arinc429Speed speed) {
  speed_ = speed;
  return true;
}

Arinc429Speed MockA429Plugin::speed() const { return speed_; }

}  // namespace examples
}  // namespace etest
