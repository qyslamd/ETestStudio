#include "MockSerialPlugin.h"

#include "logger/Logger.h"

namespace etest {
namespace examples {

using namespace core::plugin;
using namespace core::logger;

MockSerialPlugin::MockSerialPlugin() {
  meta_.id = "etest.plugin.device.mock_serial";
  meta_.name = "Mock 串口设备";
  meta_.version = "1.0.0";
  meta_.description = "模拟串口通信设备，用于验证设备插件框架";
  meta_.author = "etest";
  meta_.category = "device";
  meta_.device_type = "serial";
  meta_.device_channels = 1;
}

MockSerialPlugin::~MockSerialPlugin() = default;

bool MockSerialPlugin::initialize() {
  LOG_INFO("MOCK_SERIAL", "Mock 串口插件初始化完成");
  return true;
}

bool MockSerialPlugin::start() {
  running_ = true;
  LOG_INFO("MOCK_SERIAL", "Mock 串口插件已启动");
  return true;
}

void MockSerialPlugin::stop() {
  running_ = false;
  LOG_INFO("MOCK_SERIAL", "Mock 串口插件已停止");
}

void MockSerialPlugin::uninitialize() {
  LOG_INFO("MOCK_SERIAL", "Mock 串口插件已反初始化");
}

PluginMetaData MockSerialPlugin::metaData() const { return meta_; }
bool MockSerialPlugin::isRunning() const { return running_; }

bool MockSerialPlugin::openDevice() {
  device_opened_ = true;
  rx_buffer_.clear();
  LOG_INFO("MOCK_SERIAL", "Mock 串口设备已打开");
  return true;
}

void MockSerialPlugin::closeDevice() {
  device_opened_ = false;
  rx_buffer_.clear();
  LOG_INFO("MOCK_SERIAL", "Mock 串口设备已关闭");
}

DeviceInfo MockSerialPlugin::deviceInfo() const {
  DeviceInfo info;
  info.channel_count = 1;
  info.resolution = 0;
  info.model = "MOCK-SERIAL-COM1";
  info.manufacturer = "ETest Mock";
  return info;
}

DeviceStatus MockSerialPlugin::deviceStatus() const {
  return device_opened_ ? DeviceStatus::Online : DeviceStatus::Offline;
}

qint64 MockSerialPlugin::writeData(const QByteArray& data) {
  if (!device_opened_) return 0;
  // 回环：写入的数据追加到接收缓冲区
  rx_buffer_.append(data);
  return data.size();
}

QByteArray MockSerialPlugin::readData(int maxBytes) {
  if (!device_opened_) return QByteArray();

  if (maxBytes < 0 || maxBytes >= rx_buffer_.size()) {
    QByteArray result = rx_buffer_;
    rx_buffer_.clear();
    return result;
  }

  QByteArray result = rx_buffer_.left(maxBytes);
  rx_buffer_.remove(0, maxBytes);
  return result;
}

bool MockSerialPlugin::setBaudRate(int baudRate) {
  baud_rate_ = baudRate;
  return true;
}

int MockSerialPlugin::baudRate() const { return baud_rate_; }

bool MockSerialPlugin::setPortName(const QString& name) {
  port_name_ = name;
  return true;
}

QString MockSerialPlugin::portName() const { return port_name_; }

}  // namespace examples
}  // namespace etest
