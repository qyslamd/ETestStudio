#include "MockADPlugin.h"

#include <QRandomGenerator>
#include <QtMath>
#include <cmath>
#include "logger/Logger.h"

namespace etest {
namespace plugins {
namespace mock {

using namespace core::plugin;
using namespace core::logger;

MockADPlugin::MockADPlugin() {
  meta_.id = "etest.plugin.device.mock_ad";
  meta_.name = "Mock AD采集设备";
  meta_.version = "2.0.0";
  meta_.description = "模拟8通道16位AD采集设备，支持量程/耦合/触发配置";
  meta_.author = "etest";
  meta_.category = "device";
  meta_.device_type = "ad";
  meta_.device_channels = kChannelCount;
  meta_.device_function = "AD";
  meta_.is_mock = true;

  acquisition_timer_ = new QTimer(this);
  connect(acquisition_timer_, &QTimer::timeout, this,
          &MockADPlugin::onAcquisitionTick);

  trigger_delay_timer_ = new QTimer(this);
  trigger_delay_timer_->setSingleShot(true);
  connect(trigger_delay_timer_, &QTimer::timeout, this,
          &MockADPlugin::startFillingData);

  // 初始化每通道波形生成器：默认正弦波（归一化幅值 1.0，频率 1Hz）
  // openDevice 中重置为默认以清除之前 injectChannelData 设置的 series
  waveform_gens_.resize(kChannelCount);
  for (auto& gen : waveform_gens_) {
    gen.setWaveform(etest::core::WaveformGenerator::WaveformType::Sine,
                    1.0, 1.0, 0.0);
  }
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

  ring_buffers_.resize(kChannelCount);
  buffer_write_pos_.resize(kChannelCount);
  buffer_write_pos_.fill(0);
  channel_configs_.resize(kChannelCount);
  for (auto& gen : waveform_gens_) {
    gen.setWaveform(etest::core::WaveformGenerator::WaveformType::Sine,
                    1.0, 1.0, 0.0);
  }

  for (int i = 0; i < kChannelCount; ++i) {
    ring_buffers_[i].resize(sample_length_);
    ring_buffers_[i].fill(0.0);
  }

  sample_counter_ = 0;
  samples_collected_ = 0;
  sample_status_ = ADSampleStatus::Idle;
  LOG_INFO("MOCK_AD", "Mock AD设备已打开");
  return true;
}

void MockADPlugin::closeDevice() {
  if (acquiring_) stopAcquisition();
  device_opened_ = false;
  ring_buffers_.clear();
  buffer_write_pos_.clear();
  channel_configs_.clear();
  sample_status_ = ADSampleStatus::Idle;
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

bool MockADPlugin::setSampleRate(double rate) {
  if (rate <= 0) return false;
  sample_rate_ = rate;
  if (acquiring_) {
    int interval = static_cast<int>(1000.0 / sample_rate_);
    if (interval < 1) interval = 1;
    acquisition_timer_->setInterval(interval);
  }
  return true;
}

double MockADPlugin::sampleRate() const { return sample_rate_; }

bool MockADPlugin::setSampleLength(int length) {
  if (length <= 0) return false;
  sample_length_ = length;
  // 如果设备已打开，重新分配缓冲区
  if (device_opened_) {
    for (int i = 0; i < kChannelCount; ++i) {
      ring_buffers_[i].resize(sample_length_);
      ring_buffers_[i].fill(0.0);
      buffer_write_pos_[i] = 0;
    }
  }
  return true;
}

int MockADPlugin::sampleLength() const { return sample_length_; }

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

bool MockADPlugin::setTriggerConfig(const ADTriggerConfig& config) {
  trigger_config_ = config;
  return true;
}

ADTriggerConfig MockADPlugin::triggerConfig() const {
  return trigger_config_;
}

bool MockADPlugin::softwareTrigger() {
  if (!acquiring_ || trigger_fired_) return false;
  if (trigger_config_.mode != ADTriggerMode::Software) return false;

  startFillingData();
  return true;
}

bool MockADPlugin::startAcquisition() {
  if (!device_opened_) return false;
  if (acquiring_) return true;

  acquiring_ = true;
  trigger_fired_ = false;
  samples_collected_ = 0;
  sample_counter_ = 0;
  buffer_write_pos_.fill(0);

  // 根据触发模式决定行为
  if (!trigger_config_.enabled || trigger_config_.mode == ADTriggerMode::Software) {
    // 软件触发模式：等待 softwareTrigger() 调用
    // 如果触发未使能，直接开始
    if (!trigger_config_.enabled) {
      startFillingData();
    } else {
      sample_status_ = ADSampleStatus::Waiting;
      LOG_INFO("MOCK_AD", "等待软件触发...");
    }
  } else {
    // 硬件触发模式：模拟延时后自动触发
    sample_status_ = ADSampleStatus::Waiting;
    int delay = (trigger_config_.mode == ADTriggerMode::Internal) ? 50 : 200;
    trigger_delay_timer_->start(delay);
    LOG_INFO("MOCK_AD", "等待触发，模式={}，延时={}ms",
             static_cast<int>(trigger_config_.mode), delay);
  }

  return true;
}

void MockADPlugin::startFillingData() {
  trigger_fired_ = true;
  sample_status_ = ADSampleStatus::Sampling;

  int interval = static_cast<int>(1000.0 / sample_rate_);
  if (interval < 1) interval = 1;
  acquisition_timer_->start(interval);
  LOG_INFO("MOCK_AD", "开始采集，采样率={}Hz，存储深度={}", sample_rate_,
           sample_length_);
}

void MockADPlugin::stopAcquisition() {
  acquisition_timer_->stop();
  trigger_delay_timer_->stop();
  acquiring_ = false;
  trigger_fired_ = false;
  sample_status_ = ADSampleStatus::Idle;
  LOG_INFO("MOCK_AD", "停止采集");
}

bool MockADPlugin::isAcquiring() const { return acquiring_; }

ADSampleStatus MockADPlugin::sampleStatus() const { return sample_status_; }

void MockADPlugin::onAcquisitionTick() {
  for (int i = 0; i < kChannelCount; ++i) {
    double value = generateSample(i);
    if (samples_collected_ < sample_length_) {
      ring_buffers_[i][samples_collected_] = value;
    }
  }
  buffer_write_pos_[0] = samples_collected_;  // 所有通道同步写入
  ++sample_counter_;
  ++samples_collected_;

  // 存储深度达到后自动完成
  if (samples_collected_ >= sample_length_) {
    acquisition_timer_->stop();
    acquiring_ = false;
    sample_status_ = ADSampleStatus::Completed;
    LOG_INFO("MOCK_AD", "采集完成，共 {} 个采样点", samples_collected_);
  }
}

double MockADPlugin::generateSample(int channel) const {
  double range = channel_configs_[channel].range;

  // WaveformGenerator 输出归一化值，乘以量程缩放为实际采集值
  double value = waveform_gens_[channel].generate(sample_counter_,
                                                   sample_rate_);

  // 加少量噪声模拟真实采集
  value += (QRandomGenerator::global()->bounded(1.0) * 2.0 - 1.0) * 0.01;

  return value * range;
}

double MockADPlugin::readChannel(int channel) {
  if (!device_opened_) return 0.0;
  if (channel < 0 || channel >= kChannelCount) return 0.0;
  if (ring_buffers_.isEmpty()) return 0.0;

  // 返回最新采集到的采样点
  int latest = qMin(samples_collected_, sample_length_) - 1;
  if (latest < 0) return 0.0;
  return ring_buffers_[channel][latest];
}

QVector<double> MockADPlugin::readAllChannels() {
  QVector<double> values(kChannelCount);
  for (int i = 0; i < kChannelCount; ++i) {
    values[i] = readChannel(i);
  }
  return values;
}

QVector<double> MockADPlugin::readChannelData(int channel, int count) {
  if (!device_opened_) return {};
  if (channel < 0 || channel >= kChannelCount) return {};
  if (count <= 0) return {};

  int available = qMin(samples_collected_, sample_length_);
  count = qMin(count, available);

  QVector<double> data(count);
  for (int i = 0; i < count; ++i) {
    data[i] = ring_buffers_[channel][i];
  }
  return data;
}

QVector<double> MockADPlugin::readAllChannelsData(int count) {
  if (!device_opened_) return {};
  if (count <= 0) return {};

  int available = qMin(samples_collected_, sample_length_);
  count = qMin(count, available);

  QVector<double> data;
  data.reserve(kChannelCount * count);
  for (int ch = 0; ch < kChannelCount; ++ch) {
    for (int i = 0; i < count; ++i) {
      data.append(ring_buffers_[ch][i]);
    }
  }
  return data;
}

void MockADPlugin::injectChannelData(int channel, const QVector<double>& data) {
  if (channel < 0 || channel >= kChannelCount) {
    return;
  }
  waveform_gens_[channel].setSeries(data);
  LOG_INFO("MOCK_AD", "通道 {} 注入 {} 个采样点", channel, data.size());
}

void MockADPlugin::clearInjectedData() {
  for (auto& gen : waveform_gens_) {
    gen.setWaveform(etest::core::WaveformGenerator::WaveformType::Sine,
                    1.0, 1.0, 0.0);
  }
  LOG_INFO("MOCK_AD", "已清除所有注入数据");
}

// ============ IADevicePlugin v3.0 新增接口 ============

bool MockADPlugin::setReadMode(ADReadMode mode) {
  read_mode_ = mode;
  LOG_INFO("MOCK_AD", "设置读取模式={}", static_cast<int>(mode));
  return true;
}

ADReadMode MockADPlugin::readMode() const {
  return read_mode_;
}

bool MockADPlugin::setMemoryMode(ADMemoryMode mode) {
  memory_mode_ = mode;
  LOG_INFO("MOCK_AD", "设置存储模式={}", static_cast<int>(mode));
  return true;
}

ADMemoryMode MockADPlugin::memoryMode() const {
  return memory_mode_;
}

bool MockADPlugin::setScanList(const QVector<int>& scanList) {
  scan_list_ = scanList;
  LOG_INFO("MOCK_AD", "设置扫描表，深度={}", scanList.size());
  return true;
}

QVector<int> MockADPlugin::scanList() const {
  return scan_list_;
}

int MockADPlugin::maxScanDepth() const {
  return 256;  // 模拟值
}

QVector<qint16> MockADPlugin::readChannelRaw(int channel, int count) {
  QVector<double> volts = readChannelData(channel, count);
  QVector<qint16> raw(volts.size());
  for (int i = 0; i < volts.size(); ++i) {
    double range = channel_configs_.value(channel).range;
    if (range > 0.0) {
      raw[i] = static_cast<qint16>(volts[i] / range * 32768.0);
    }
  }
  return raw;
}

QVector<qint16> MockADPlugin::readAllChannelsRaw(int count) {
  QVector<qint16> data;
  for (int ch = 0; ch < kChannelCount; ++ch) {
    QVector<qint16> chRaw = readChannelRaw(ch, count);
    data.append(chRaw);
  }
  return data;
}

}  // namespace mock
}  // namespace plugins
}  // namespace etest
