#ifndef ETEST_PLUGINS_MOCK_AD_PLUGIN_H_
#define ETEST_PLUGINS_MOCK_AD_PLUGIN_H_

#include <QObject>
#include <QTimer>
#include "IADevicePlugin.h"

namespace etest {
namespace plugins {
namespace mock {

class MockADPlugin : public QObject, public core::plugin::IADevicePlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "etest.core.plugin.IADevicePlugin/3.0" FILE "mock_ad_device.json")
  Q_INTERFACES(etest::core::plugin::IPlugin etest::core::plugin::IDevicePlugin etest::core::plugin::IADevicePlugin)

 public:
  MockADPlugin();
  ~MockADPlugin() override;

  // IPlugin
  bool initialize() override;
  bool start() override;
  void stop() override;
  void uninitialize() override;
  core::plugin::PluginMetaData metaData() const override;
  bool isRunning() const override;

  // IDevicePlugin
  bool openDevice() override;
  void closeDevice() override;
  core::plugin::DeviceInfo deviceInfo() const override;
  core::plugin::DeviceStatus deviceStatus() const override;

  // IADevicePlugin
  bool setSampleRate(double rate) override;
  double sampleRate() const override;

  bool setSampleLength(int length) override;
  int sampleLength() const override;

  bool setChannelConfig(int channel, const core::plugin::ADChannelConfig& config) override;
  core::plugin::ADChannelConfig channelConfig(int channel) const override;

  bool setTriggerConfig(const core::plugin::ADTriggerConfig& config) override;
  core::plugin::ADTriggerConfig triggerConfig() const override;
  bool softwareTrigger() override;

  bool startAcquisition() override;
  void stopAcquisition() override;
  bool isAcquiring() const override;
  core::plugin::ADSampleStatus sampleStatus() const override;

  double readChannel(int channel) override;
  QVector<double> readAllChannels() override;

  QVector<double> readChannelData(int channel, int count) override;
  QVector<double> readAllChannelsData(int count) override;

  // IADevicePlugin (新增 v3.0)
  bool setReadMode(core::plugin::ADReadMode mode) override;
  core::plugin::ADReadMode readMode() const override;
  bool setMemoryMode(core::plugin::ADMemoryMode mode) override;
  core::plugin::ADMemoryMode memoryMode() const override;
  bool setScanList(const QVector<int>& scanList) override;
  QVector<int> scanList() const override;
  int maxScanDepth() const override;
  QVector<qint16> readChannelRaw(int channel, int count) override;
  QVector<qint16> readAllChannelsRaw(int count) override;

  // 测试用：注入预定义波形数据，采集时循环回放
  void injectChannelData(int channel, const QVector<double>& data);
  void clearInjectedData();

 private:
  void onAcquisitionTick();
  double generateSample(int channel) const;
  void startFillingData();

  bool running_ = false;
  bool device_opened_ = false;
  bool acquiring_ = false;
  bool trigger_fired_ = false;
  core::plugin::PluginMetaData meta_;

  QTimer* acquisition_timer_ = nullptr;
  QTimer* trigger_delay_timer_ = nullptr;
  double sample_rate_ = 1000.0;
  int sample_length_ = 1024;
  qint64 sample_counter_ = 0;
  int samples_collected_ = 0;

  core::plugin::ADTriggerConfig trigger_config_;
  core::plugin::ADSampleStatus sample_status_ = core::plugin::ADSampleStatus::Idle;
  core::plugin::ADReadMode read_mode_ = core::plugin::ADReadMode::Direct;
  core::plugin::ADMemoryMode memory_mode_ = core::plugin::ADMemoryMode::ChannelStorage;
  QVector<int> scan_list_;

  QVector<QVector<double>> ring_buffers_;
  QVector<int> buffer_write_pos_;
  QVector<core::plugin::ADChannelConfig> channel_configs_;

  // 注入数据（测试用）
  QVector<QVector<double>> injected_data_;

  static constexpr int kChannelCount = 8;
  static constexpr int kResolution = 16;
  static constexpr double kDefaultFrequency = 1.0;  // 默认正弦波频率 1Hz
};

}  // namespace mock
}  // namespace plugins
}  // namespace etest

#endif  // ETEST_PLUGINS_MOCK_AD_PLUGIN_H_