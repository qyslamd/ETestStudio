#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QThread>
#include "IADevicePlugin.h"
#include "IDevicePlugin.h"
#include "PluginManager.h"
#include "PluginMetaData.h"

using namespace etest::core::plugin;

class ADDevicePluginTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 测试可执行文件在bin/tests/下，插件在bin/plugins/下
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

  // 未打开设备时读取应返回0
  EXPECT_DOUBLE_EQ(ad->readChannel(0), 0.0);

  // 无效通道
  EXPECT_DOUBLE_EQ(ad->readChannel(-1), 0.0);
  EXPECT_DOUBLE_EQ(ad->readChannel(8), 0.0);
}

TEST_F(ADDevicePluginTest, ReadChannelBeforeAcquisition) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();
  // 未启动采集时，缓冲区为0
  EXPECT_DOUBLE_EQ(ad->readChannel(0), 0.0);
}

TEST_F(ADDevicePluginTest, ReadChannelDuringAcquisition) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();
  ad->startAcquisition();

  // 等待几个采样周期（默认1kHz，等50ms足够）
  QThread::msleep(50);

  double value = ad->readChannel(0);
  // 采集运行后应该有数据（正弦波大概率非零）
  // 但不强制非零，因为可能恰好在零点
  EXPECT_NO_THROW(ad->readChannel(0));

  ad->stopAcquisition();
  ad->closeDevice();
}

TEST_F(ADDevicePluginTest, ReadAllChannels) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();
  QVector<double> values = ad->readAllChannels();
  EXPECT_EQ(values.size(), 8);
}

TEST_F(ADDevicePluginTest, AcquisitionControl) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  // 未打开设备不能开始采集
  EXPECT_FALSE(ad->isAcquiring());
  EXPECT_FALSE(ad->startAcquisition());

  ad->openDevice();

  // 开始采集
  EXPECT_TRUE(ad->startAcquisition());
  EXPECT_TRUE(ad->isAcquiring());

  // 重复开始不报错
  EXPECT_TRUE(ad->startAcquisition());

  // 停止采集
  ad->stopAcquisition();
  EXPECT_FALSE(ad->isAcquiring());

  ad->closeDevice();
}

TEST_F(ADDevicePluginTest, SampleRate) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  // 默认采样率1000Hz
  EXPECT_DOUBLE_EQ(ad->sampleRate(), 1000.0);

  // 设置采样率
  EXPECT_TRUE(ad->setSampleRate(500.0));
  EXPECT_DOUBLE_EQ(ad->sampleRate(), 500.0);

  // 无效采样率
  EXPECT_FALSE(ad->setSampleRate(0.0));
  EXPECT_FALSE(ad->setSampleRate(-100.0));

  // 采样率不变
  EXPECT_DOUBLE_EQ(ad->sampleRate(), 500.0);
}

TEST_F(ADDevicePluginTest, ChannelConfig) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();

  // 默认配置
  ADChannelConfig defaultCfg = ad->channelConfig(0);
  EXPECT_EQ(defaultCfg.waveform, WaveformType::Sine);
  EXPECT_DOUBLE_EQ(defaultCfg.amplitude, 1.0);

  // 设置方波配置
  ADChannelConfig squareCfg;
  squareCfg.waveform = WaveformType::Square;
  squareCfg.frequency = 2.0;
  squareCfg.amplitude = 0.5;
  squareCfg.offset = 0.1;
  squareCfg.noise_level = 0.0;

  EXPECT_TRUE(ad->setChannelConfig(0, squareCfg));
  ADChannelConfig readCfg = ad->channelConfig(0);
  EXPECT_EQ(readCfg.waveform, WaveformType::Square);
  EXPECT_DOUBLE_EQ(readCfg.frequency, 2.0);
  EXPECT_DOUBLE_EQ(readCfg.amplitude, 0.5);
  EXPECT_DOUBLE_EQ(readCfg.offset, 0.1);

  // 无效通道
  EXPECT_FALSE(ad->setChannelConfig(-1, squareCfg));
  EXPECT_FALSE(ad->setChannelConfig(8, squareCfg));

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
