#include "MockADPlugin.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QtMath>
#include "logger/Logger.h"

namespace etest {
namespace examples {

using namespace core::plugin;
using namespace core::logger;

MockADPlugin::MockADPlugin() {
  meta_.id = "etest.plugin.device.mock_ad";
  meta_.name = "Mock AD采集设备";
  meta_.version = "1.0.0";
  meta_.description = "模拟8通道16位AD采集设备，用于验证设备插件框架";
  meta_.author = "etest";
  meta_.category = "device";
  meta_.device_type = "ad";
  meta_.device_channels = kChannelCount;
}

MockADPlugin::~MockADPlugin() = default;

bool MockADPlugin::initialize() {
  LOG_INFO("MOCK_AD", "Mock AD插件初始化完成");
  return true;
}

bool MockADPlugin::start() {
  running_ = true;
  LOG_INFO("MOCK_AD", "Mock AD插件已启动");
  return true;
}

void MockADPlugin::stop() {
  running_ = false;
  LOG_INFO("MOCK_AD", "Mock AD插件已停止");
}

void MockADPlugin::uninitialize() {
  LOG_INFO("MOCK_AD", "Mock AD插件已反初始化");
}

PluginMetaData MockADPlugin::metaData() const { return meta_; }
bool MockADPlugin::isRunning() const { return running_; }

bool MockADPlugin::openDevice() {
  device_opened_ = true;
  LOG_INFO("MOCK_AD", "Mock AD设备已打开");
  return true;
}

void MockADPlugin::closeDevice() {
  device_opened_ = false;
  LOG_INFO("MOCK_AD", "Mock AD设备已关闭");
}

DeviceInfo MockADPlugin::deviceInfo() const {
  DeviceInfo info;
  info.channel_count = kChannelCount;
  info.resolution = kResolution;
  info.model = "MOCK-AD-8CH-16BIT";
  info.manufacturer = "ETest Mock";
  return info;
}

DeviceStatus MockADPlugin::deviceStatus() const {
  return device_opened_ ? DeviceStatus::Online : DeviceStatus::Offline;
}

double MockADPlugin::readChannel(int channel) {
  if (!device_opened_) return 0.0;
  if (channel < 0 || channel >= kChannelCount) return 0.0;
  return generateMockValue(channel);
}

QVector<double> MockADPlugin::readAllChannels() {
  QVector<double> values(kChannelCount);
  for (int i = 0; i < kChannelCount; ++i) {
    values[i] = generateMockValue(i);
  }
  return values;
}

double MockADPlugin::generateMockValue(int channel) const {
  // 正弦波 + 随机噪声，模拟真实AD采集
  double t = QDateTime::currentMSecsSinceEpoch() / 1000.0;
  double sine = qSin(2.0 * M_PI * (channel + 1) * 0.5 * t);
  double noise = QRandomGenerator::global()->bounded(1.0) * 0.1 - 0.05;
  double maxVal = (1 << kResolution) / 2.0 - 1;
  return (sine + noise) * maxVal;
}

}  // namespace examples
}  // namespace etest
