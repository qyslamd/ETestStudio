#include "MockADPlugin.h"

#include <QRandomGenerator>
#include <QtMath>
#include <cmath>
#include "logger/Logger.h"

namespace etest {
namespace examples {

using namespace core::plugin;
using namespace core::logger;

MockADPlugin::MockADPlugin() {
  meta_.id = "etest.plugin.device.mock_ad";
  meta_.name = "Mock AD采集设备";
  meta_.version = "1.0.0";
  meta_.description = "模拟8通道16位AD采集设备，支持可配置采样率和信号类型";
  meta_.author = "etest";
  meta_.category = "device";
  meta_.device_type = "ad";
  meta_.device_channels = kChannelCount;

  acquisition_timer_ = new QTimer(this);
  connect(acquisition_timer_, &QTimer::timeout, this,
          &MockADPlugin::onAcquisitionTick);
}

MockADPlugin::~MockADPlugin() {
  if (acquiring_) stopAcquisition();
}

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
  if (acquiring_) stopAcquisition();
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

  // 分配环形缓冲区和通道配置
  ring_buffers_.resize(kChannelCount);
  buffer_write_pos_.resize(kChannelCount);
  buffer_write_pos_.fill(0);
  channel_configs_.resize(kChannelCount);

  for (int i = 0; i < kChannelCount; ++i) {
    ring_buffers_[i].resize(kRingBufferSize);
    ring_buffers_[i].fill(0.0);
    channel_configs_[i] = ADChannelConfig{};
    channel_configs_[i].frequency = (i + 1) * 0.5;  // 每通道不同频率
  }

  sample_counter_ = 0;
  LOG_INFO("MOCK_AD", "Mock AD设备已打开");
  return true;
}

void MockADPlugin::closeDevice() {
  if (acquiring_) stopAcquisition();
  device_opened_ = false;
  ring_buffers_.clear();
  buffer_write_pos_.clear();
  channel_configs_.clear();
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

bool MockADPlugin::startAcquisition() {
  if (!device_opened_) return false;
  if (acquiring_) return true;

  int interval = static_cast<int>(1000.0 / sample_rate_);
  if (interval < 1) interval = 1;
  acquisition_timer_->start(interval);
  acquiring_ = true;
  LOG_INFO("MOCK_AD", "开始采集，采样率={}Hz", sample_rate_);
  return true;
}

void MockADPlugin::stopAcquisition() {
  acquisition_timer_->stop();
  acquiring_ = false;
  LOG_INFO("MOCK_AD", "停止采集");
}

bool MockADPlugin::isAcquiring() const { return acquiring_; }

bool MockADPlugin::setSampleRate(double rate) {
  if (rate <= 0) return false;
  sample_rate_ = rate;
  // 采集中切换采样率，立即生效
  if (acquiring_) {
    int interval = static_cast<int>(1000.0 / sample_rate_);
    if (interval < 1) interval = 1;
    acquisition_timer_->setInterval(interval);
  }
  return true;
}

double MockADPlugin::sampleRate() const { return sample_rate_; }

bool MockADPlugin::setChannelConfig(int channel,
                                     const ADChannelConfig& config) {
  if (channel < 0 || channel >= kChannelCount) return false;
  channel_configs_[channel] = config;
  return true;
}

ADChannelConfig MockADPlugin::channelConfig(int channel) const {
  if (channel < 0 || channel >= kChannelCount) return ADChannelConfig{};
  return channel_configs_[channel];
}

void MockADPlugin::onAcquisitionTick() {
  for (int i = 0; i < kChannelCount; ++i) {
    double value = generateSignal(i);
    ring_buffers_[i][buffer_write_pos_[i]] = value;
    buffer_write_pos_[i] = (buffer_write_pos_[i] + 1) % kRingBufferSize;
  }
  ++sample_counter_;
}

double MockADPlugin::readChannel(int channel) {
  if (!device_opened_) return 0.0;
  if (channel < 0 || channel >= kChannelCount) return 0.0;
  if (ring_buffers_.isEmpty()) return 0.0;

  // 读最新值：write_pos前一个位置
  int latest = (buffer_write_pos_[channel] - 1 + kRingBufferSize) % kRingBufferSize;
  return ring_buffers_[channel][latest];
}

QVector<double> MockADPlugin::readAllChannels() {
  QVector<double> values(kChannelCount);
  for (int i = 0; i < kChannelCount; ++i) {
    values[i] = readChannel(i);
  }
  return values;
}

double MockADPlugin::generateSignal(int channel) const {
  const auto& cfg = channel_configs_[channel];
  double t = sample_counter_ / sample_rate_;
  double value = 0.0;

  switch (cfg.waveform) {
    case WaveformType::Sine:
      value = qSin(2.0 * M_PI * cfg.frequency * t);
      break;
    case WaveformType::Square:
      value = std::fmod(t * cfg.frequency, 1.0) < 0.5 ? 1.0 : -1.0;
      break;
    case WaveformType::Triangle:
      value = 2.0 * std::fabs(2.0 * std::fmod(t * cfg.frequency, 1.0) - 1.0) - 1.0;
      break;
    case WaveformType::DC:
      value = 0.0;
      break;
  }

  value = value * cfg.amplitude + cfg.offset;
  if (cfg.noise_level > 0) {
    value += (QRandomGenerator::global()->bounded(1.0) * 2.0 - 1.0) * cfg.noise_level;
  }

  double maxVal = (1 << kResolution) / 2.0 - 1;
  return value * maxVal;
}

}  // namespace examples
}  // namespace etest
