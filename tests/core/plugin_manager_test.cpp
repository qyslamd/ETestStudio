#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include "plugin_sdk/IPlugin.h"
#include "plugin_sdk/PluginManager.h"
#include "plugin_sdk/PluginMetaData.h"

using namespace etest::core::plugin;

class PluginManagerTest : public ::testing::Test {
 protected:
  void SetUp() override { pm_ = &PluginManager::instance(); }

  PluginManager* pm_;
};

TEST_F(PluginManagerTest, SingletonInstance) {
  PluginManager& a = PluginManager::instance();
  PluginManager& b = PluginManager::instance();
  EXPECT_EQ(&a, &b);
}

TEST_F(PluginManagerTest, DefaultSearchPath) {
  QStringList paths = pm_->searchPaths();
  ASSERT_FALSE(paths.isEmpty());
  // 默认搜索路径应包含可执行文件旁的plugins/目录
  EXPECT_TRUE(paths.first().endsWith("plugins"));
}

TEST_F(PluginManagerTest, AddSearchPath) {
  QString testPath = "/tmp/etest_test_plugins";
  pm_->addSearchPath(testPath);
  QStringList paths = pm_->searchPaths();
  EXPECT_TRUE(paths.contains(testPath));
}

TEST_F(PluginManagerTest, AddSearchPathNoDuplicate) {
  QString testPath = "/tmp/etest_test_plugins_dup";
  pm_->addSearchPath(testPath);
  pm_->addSearchPath(testPath);
  int count = pm_->searchPaths().filter(testPath).size();
  EXPECT_EQ(count, 1);
}

TEST_F(PluginManagerTest, LoadAllEmptyDir) {
  // 添加一个不存在的目录，不应崩溃
  pm_->addSearchPath("/tmp/etest_nonexistent_dir");
  pm_->loadAll();
  EXPECT_TRUE(pm_->loadedPlugins().isEmpty());
}

TEST_F(PluginManagerTest, LoadPluginNotFound) {
  bool result = pm_->loadPlugin("etest.plugin.nonexistent");
  EXPECT_FALSE(result);
}

TEST_F(PluginManagerTest, UnloadPluginNotLoaded) {
  bool result = pm_->unloadPlugin("etest.plugin.notloaded");
  EXPECT_FALSE(result);
}

TEST_F(PluginManagerTest, PluginReturnsNullForNotLoaded) {
  IPlugin* p = pm_->plugin("etest.plugin.notloaded");
  EXPECT_EQ(p, nullptr);
}

TEST_F(PluginManagerTest, PluginAsReturnsNullForNotLoaded) {
  auto p = pm_->pluginAs<IPlugin>("etest.plugin.notloaded");
  EXPECT_EQ(p, nullptr);
}

TEST_F(PluginManagerTest, LoadedPluginsEmptyInitially) {
  // 因为我们还没真正加载有效插件，列表应为空
  EXPECT_TRUE(pm_->loadedPlugins().isEmpty());
}
