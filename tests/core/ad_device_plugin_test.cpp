#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include "IADevicePlugin.h"
#include "IDevicePlugin.h"
#include "PluginManager.h"
#include "PluginMetaData.h"

using namespace etest::core::plugin;

// 处理 Qt 事件循环并等待指定毫秒，确保 QTimer 等异步事件能触发
static void processEventsFor(int ms) {
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < ms) {
    QCoreApplication::processEvents();
    QThread::msleep(5);
  }
}

class ADDevicePluginTest : public ::testing::Test {
 protected:
  void SetUp() override {
    QString pluginPath = QCoreApplication::applicationDirPath() + "/../plugins";
    auto& pm = PluginManager::instance();
    pm.addSearchPath(pluginPath);
    pm.loadAll();
  }

  void TearDown() override {
    auto& pm = PluginManager::instance();
    pm.unloadAll();
  }
};

TEST_F(ADDevicePluginTest, LoadMockADPlugin) {
  auto& pm = PluginManager::instance();
  IPlugin* plugin = pm.plugin("etest.plugin.device.mock_ad");
  ASSERT_NE(plugin, nullptr);
  EXPECT_TRUE(plugin->isRunning());
}

TEST_F(ADDevicePluginTest, CastToIDevicePlugin) {
  auto& pm = PluginManager::instance();
  IDevicePlugin* device = pm.pluginAs<IDevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(device, nullptr);
  EXPECT_EQ(device->deviceStatus(), DeviceStatus::Offline);
}

TEST_F(ADDevicePluginTest, CastToIADevicePlugin) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);
}

TEST_F(ADDevicePluginTest, DeviceOpenClose) {
  auto& pm = PluginManager::instance();
  IDevicePlugin* device = pm.pluginAs<IDevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(device, nullptr);

  EXPECT_TRUE(device->openDevice());
  EXPECT_EQ(device->deviceStatus(), DeviceStatus::Online);

  device->closeDevice();
  EXPECT_EQ(device->deviceStatus(), DeviceStatus::Offline);
}

TEST_F(ADDevicePluginTest, DeviceInfo) {
  auto& pm = PluginManager::instance();
  IDevicePlugin* device = pm.pluginAs<IDevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(device, nullptr);

  DeviceInfo info = device->deviceInfo();
  EXPECT_EQ(info.channel_count, 8);
  EXPECT_EQ(info.resolution, 16);
  EXPECT_FALSE(info.model.isEmpty());
  EXPECT_FALSE(info.manufacturer.isEmpty());
}

TEST_F(ADDevicePluginTest, ReadChannelWithoutDevice) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  EXPECT_DOUBLE_EQ(ad->readChannel(0), 0.0);
  EXPECT_DOUBLE_EQ(ad->readChannel(-1), 0.0);
  EXPECT_DOUBLE_EQ(ad->readChannel(8), 0.0);
}

TEST_F(ADDevicePluginTest, SampleRate) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  EXPECT_DOUBLE_EQ(ad->sampleRate(), 1000.0);

  EXPECT_TRUE(ad->setSampleRate(500.0));
  EXPECT_DOUBLE_EQ(ad->sampleRate(), 500.0);

  EXPECT_FALSE(ad->setSampleRate(0.0));
  EXPECT_FALSE(ad->setSampleRate(-100.0));

  EXPECT_DOUBLE_EQ(ad->sampleRate(), 500.0);
}

TEST_F(ADDevicePluginTest, SampleLength) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  EXPECT_EQ(ad->sampleLength(), 1024);

  EXPECT_TRUE(ad->setSampleLength(2048));
  EXPECT_EQ(ad->sampleLength(), 2048);

  EXPECT_FALSE(ad->setSampleLength(0));
  EXPECT_FALSE(ad->setSampleLength(-1));

  EXPECT_EQ(ad->sampleLength(), 2048);
}

TEST_F(ADDevicePluginTest, ChannelConfig) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();

  // 默认配置
  ADChannelConfig defaultCfg = ad->channelConfig(0);
  EXPECT_DOUBLE_EQ(defaultCfg.range, 10.0);
  EXPECT_EQ(defaultCfg.coupling, ADCoupling::DC);

  // 修改量程、耦合和通道模式
  ADChannelConfig cfg;
  cfg.range = 5.0;
  cfg.coupling = ADCoupling::AC;
  cfg.differential = true;
  cfg.gain = 2;
  cfg.trigger_edge = ADTriggerEdge::Rising;
  cfg.trigger_level = 1.5;

  EXPECT_TRUE(ad->setChannelConfig(0, cfg));
  ADChannelConfig readCfg = ad->channelConfig(0);
  EXPECT_DOUBLE_EQ(readCfg.range, 5.0);
  EXPECT_EQ(readCfg.coupling, ADCoupling::AC);
  EXPECT_TRUE(readCfg.differential);
  EXPECT_EQ(readCfg.gain, 2);
  EXPECT_DOUBLE_EQ(readCfg.trigger_level, 1.5);

  // 无效通道
  EXPECT_FALSE(ad->setChannelConfig(-1, cfg));
  EXPECT_FALSE(ad->setChannelConfig(8, cfg));

  ad->closeDevice();
}

TEST_F(ADDevicePluginTest, TriggerConfig) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  // 默认软件触发
  ADTriggerConfig defaultTrg = ad->triggerConfig();
  EXPECT_EQ(defaultTrg.mode, ADTriggerMode::Software);
  EXPECT_TRUE(defaultTrg.enabled);

  // 切换为内部触发
  ADTriggerConfig trg;
  trg.mode = ADTriggerMode::Internal;
  trg.enabled = true;
  EXPECT_TRUE(ad->setTriggerConfig(trg));
  EXPECT_EQ(ad->triggerConfig().mode, ADTriggerMode::Internal);
}

TEST_F(ADDevicePluginTest, AcquisitionControl) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  EXPECT_FALSE(ad->isAcquiring());
  EXPECT_EQ(ad->sampleStatus(), ADSampleStatus::Idle);
  EXPECT_FALSE(ad->startAcquisition());  // 未打开设备

  ad->openDevice();
  ad->setSampleLength(100000);  // 足够大，不会在测试期间自动完成

  // 触发未使能，直接开始
  ADTriggerConfig trg;
  trg.enabled = false;
  ad->setTriggerConfig(trg);

  EXPECT_TRUE(ad->startAcquisition());
  EXPECT_TRUE(ad->isAcquiring());
  EXPECT_EQ(ad->sampleStatus(), ADSampleStatus::Sampling);

  ad->stopAcquisition();
  EXPECT_FALSE(ad->isAcquiring());
  EXPECT_EQ(ad->sampleStatus(), ADSampleStatus::Idle);

  ad->closeDevice();
}

TEST_F(ADDevicePluginTest, SoftwareTrigger) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();

  // 启用软件触发
  ADTriggerConfig trg;
  trg.mode = ADTriggerMode::Software;
  trg.enabled = true;
  ad->setTriggerConfig(trg);

  // 设置较大的存储深度以避免采集立即完成
  ad->setSampleLength(10000);

  EXPECT_TRUE(ad->startAcquisition());
  processEventsFor(10);  // 让 Waiting 状态生效
  EXPECT_EQ(ad->sampleStatus(), ADSampleStatus::Waiting);

  // 发送软件触发
  EXPECT_TRUE(ad->softwareTrigger());
  EXPECT_EQ(ad->sampleStatus(), ADSampleStatus::Sampling);

  // 等待一些数据
  processEventsFor(50);

  double value = ad->readChannel(0);
  EXPECT_NO_THROW(ad->readChannel(0));

  ad->stopAcquisition();
  ad->closeDevice();
}

TEST_F(ADDevicePluginTest, ReadChannelData) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();
  ad->setSampleLength(10000);

  // 触发未使能，直接开始
  ADTriggerConfig trg;
  trg.enabled = false;
  ad->setTriggerConfig(trg);

  ad->startAcquisition();
  processEventsFor(100);

  QVector<double> data = ad->readChannelData(0, 10);
  EXPECT_GT(data.size(), 0);
  EXPECT_LE(data.size(), 10);

  // 读取值应在量程范围内
  for (double v : data) {
    EXPECT_GE(v, -10.0);
    EXPECT_LE(v, 10.0);
  }

  ad->stopAcquisition();
  ad->closeDevice();
}

TEST_F(ADDevicePluginTest, ReadAllChannelsData) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();
  ad->setSampleLength(10000);

  ADTriggerConfig trg;
  trg.enabled = false;
  ad->setTriggerConfig(trg);

  ad->startAcquisition();
  processEventsFor(100);

  QVector<double> data = ad->readAllChannelsData(5);
  // 8通道 × min(5, available) 个点
  EXPECT_GT(data.size(), 0);

  ad->stopAcquisition();
  ad->closeDevice();
}

TEST_F(ADDevicePluginTest, ReadChannelInRange) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();
  ad->setSampleLength(10000);

  // 设置量程 ±5V
  ADChannelConfig cfg;
  cfg.range = 5.0;
  for (int i = 0; i < 8; ++i) {
    ad->setChannelConfig(i, cfg);
  }

  ADTriggerConfig trg;
  trg.enabled = false;
  ad->setTriggerConfig(trg);

  ad->startAcquisition();
  processEventsFor(100);

  // 读取值应在量程范围内
  double value = ad->readChannel(0);
  EXPECT_GE(value, -5.5);  // 允许少量噪声越界
  EXPECT_LE(value, 5.5);

  ad->stopAcquisition();
  ad->closeDevice();
}

TEST_F(ADDevicePluginTest, MetaDataDeviceType) {
  auto& pm = PluginManager::instance();
  IPlugin* plugin = pm.plugin("etest.plugin.device.mock_ad");
  ASSERT_NE(plugin, nullptr);

  PluginMetaData meta = plugin->metaData();
  EXPECT_EQ(meta.device_type, "ad");
  EXPECT_EQ(meta.device_channels, 8);
  EXPECT_EQ(meta.category, "device");
}

TEST_F(ADDevicePluginTest, DevicesByType) {
  auto& pm = PluginManager::instance();
  QList<PluginMetaData> adDevices = pm.devicesByType("ad");
  EXPECT_EQ(adDevices.size(), 1);
  EXPECT_EQ(adDevices[0].id, "etest.plugin.device.mock_ad");
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  QCoreApplication app(argc, argv);
  return RUN_ALL_TESTS();
}
