#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QPluginLoader>
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

TEST_F(ADDevicePluginTest, ReadChannel) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  // 未打开设备时读取应返回0
  EXPECT_DOUBLE_EQ(ad->readChannel(0), 0.0);

  // 打开设备后读取
  ad->openDevice();
  double value = ad->readChannel(0);
  EXPECT_NE(value, 0.0);  // 模拟正弦波，大概率非零

  // 无效通道
  EXPECT_DOUBLE_EQ(ad->readChannel(-1), 0.0);
  EXPECT_DOUBLE_EQ(ad->readChannel(8), 0.0);
}

TEST_F(ADDevicePluginTest, ReadAllChannels) {
  auto& pm = PluginManager::instance();
  IADevicePlugin* ad = pm.pluginAs<IADevicePlugin>("etest.plugin.device.mock_ad");
  ASSERT_NE(ad, nullptr);

  ad->openDevice();
  QVector<double> values = ad->readAllChannels();
  EXPECT_EQ(values.size(), 8);
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
