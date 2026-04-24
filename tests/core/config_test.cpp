#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "logger/Logger.h"

using namespace etest::core::config;
using namespace etest::core::logger;

TEST(ConfigManagerTest, Instance) {
  ConfigManager& instance1 = ConfigManager::instance();
  ConfigManager& instance2 = ConfigManager::instance();
  EXPECT_EQ(&instance1, &instance2);  // 单例验证
}

TEST(ConfigManagerTest, ReadWrite) {
  ConfigManager& config = ConfigManager::instance();

  // 测试整数读写
  config.set(CONFIG_WINDOW_WIDTH, 1920);
  EXPECT_EQ(config.get<int>(CONFIG_WINDOW_WIDTH), 1920);

  // 测试布尔读写
  config.set(CONFIG_WINDOW_MAXIMIZED, true);
  EXPECT_EQ(config.get<bool>(CONFIG_WINDOW_MAXIMIZED), true);

  // 测试字符串读写
  config.set(CONFIG_RECENT_LAST_OPEN_PATH, QString("D:/test/path"));
  EXPECT_EQ(config.get<QString>(CONFIG_RECENT_LAST_OPEN_PATH),
            QString("D:/test/path"));

  // 测试字符串列表读写
  QStringList projects = {"D:/test1.etest", "D:/test2.etest"};
  config.set(CONFIG_RECENT_PROJECT_LIST, projects);
  EXPECT_EQ(config.get<QStringList>(CONFIG_RECENT_PROJECT_LIST), projects);
}

TEST(ConfigManagerTest, DefaultValue) {
  ConfigManager& config = ConfigManager::instance();
  config.resetAllToDefault();

  // 验证默认值
  EXPECT_EQ(config.get<int>(CONFIG_LOG_LEVEL), CONFIG_LOG_DEFAULT_LEVEL);
  EXPECT_EQ(config.get<bool>(CONFIG_BACKUP_ENABLED),
            CONFIG_BACKUP_DEFAULT_ENABLED);
  EXPECT_EQ(config.get<int>(CONFIG_BACKUP_INTERVAL_MIN),
            CONFIG_BACKUP_DEFAULT_INTERVAL_MIN);
}

TEST(ConfigManagerTest, ResetKey) {
  ConfigManager& config = ConfigManager::instance();
  config.set(CONFIG_LOG_LEVEL, 3);  // 修改为error级别
  EXPECT_EQ(config.get<int>(CONFIG_LOG_LEVEL), 3);

  config.resetKeyToDefault(CONFIG_LOG_LEVEL);
  EXPECT_EQ(config.get<int>(CONFIG_LOG_LEVEL), CONFIG_LOG_DEFAULT_LEVEL);
}

TEST(ConfigManagerTest, ResetAll) {
  ConfigManager& config = ConfigManager::instance();
  // 修改多个配置
  config.set(CONFIG_WINDOW_WIDTH, 1920);
  config.set(CONFIG_LOG_LEVEL, 3);
  config.set(CONFIG_BACKUP_ENABLED, false);

  config.resetAllToDefault();

  // 验证所有恢复默认
  EXPECT_EQ(config.get<int>(CONFIG_WINDOW_WIDTH), 1280);
  EXPECT_EQ(config.get<int>(CONFIG_LOG_LEVEL), CONFIG_LOG_DEFAULT_LEVEL);
  EXPECT_EQ(config.get<bool>(CONFIG_BACKUP_ENABLED),
            CONFIG_BACKUP_DEFAULT_ENABLED);
}

TEST(ConfigManagerTest, ImportExportJson) {
  QTemporaryDir tempDir;
  QString tempFile = tempDir.filePath("test_config.json");

  ConfigManager& config = ConfigManager::instance();
  config.resetAllToDefault();

  // 修改配置
  config.set(CONFIG_WINDOW_WIDTH, 1920);
  config.set(CONFIG_WINDOW_HEIGHT, 1080);
  config.set(CONFIG_LOG_LEVEL, 1);
  config.set(CONFIG_BACKUP_INTERVAL_MIN, 10);

  // 导出配置
  EXPECT_TRUE(config.exportToJson(tempFile));
  EXPECT_TRUE(QFile::exists(tempFile));

  // 修改配置
  config.set(CONFIG_WINDOW_WIDTH, 1280);
  config.set(CONFIG_LOG_LEVEL, 3);

  // 导入配置
  EXPECT_TRUE(config.importFromJson(tempFile));

  // 验证导入生效
  EXPECT_EQ(config.get<int>(CONFIG_WINDOW_WIDTH), 1920);
  EXPECT_EQ(config.get<int>(CONFIG_WINDOW_HEIGHT), 1080);
  EXPECT_EQ(config.get<int>(CONFIG_LOG_LEVEL), 1);
  EXPECT_EQ(config.get<int>(CONFIG_BACKUP_INTERVAL_MIN), 10);

  // 测试导入不覆盖模式
  config.set(CONFIG_WINDOW_WIDTH, 800);
  EXPECT_TRUE(config.importFromJson(tempFile, false));
  EXPECT_EQ(config.get<int>(CONFIG_WINDOW_WIDTH), 800);  // 不覆盖，保留现有值
}

TEST(ConfigManagerTest, LogLevelIntegration) {
  ConfigManager& config = ConfigManager::instance();
  Logger::init();  // 初始化日志系统

  // 修改日志级别
  config.set(CONFIG_LOG_LEVEL, 0);  // debug级别
  // 日志级别已经通过监听自动更新，无需额外操作

  // 修改为warn级别
  config.set(CONFIG_LOG_LEVEL, 2);

  Logger::shutdown();
}

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
