#include <gtest/gtest.h>
#include "logger/Logger.h"
#include "config/ConfigManager.h"
#include "config/ConfigDefs.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ConfigManager::instance().resetAllToDefault();
        Logger::init();
    }

    void TearDown() override {
        Logger::shutdown();
    }
};

TEST_F(LoggerTest, InitAndShutdown) {
    // 初始化已在 SetUp 中完成，验证可以正常关闭
    Logger::shutdown();
    // 再次初始化验证可重复初始化
    Logger::init();
}

TEST_F(LoggerTest, AllLogLevelOutput) {
    // 验证所有日志级别宏可正常调用，不崩溃
    LOG_DEBUG("TEST", "debug message {}", 1);
    LOG_INFO("TEST", "info message {}", 2);
    LOG_WARN("TEST", "warn message {}", 3);
    LOG_ERROR("TEST", "error message {}", 4);
    LOG_FATAL("TEST", "fatal message {}", 5);
}

TEST_F(LoggerTest, ModuleLevelSwitch) {
    // 设置TEST模块为error级别
    Logger::setLevel("TEST", LOG_LEVEL_ERROR);

    // 设置OTHER模块为debug级别
    Logger::setLevel("OTHER", LOG_LEVEL_DEBUG);

    // 验证不会崩溃
    LOG_DEBUG("OTHER", "other debug");
    LOG_ERROR("TEST", "test error");
}

TEST_F(LoggerTest, SetAllLevel) {
    // 设置全局日志级别
    Logger::setAllLevel(LOG_LEVEL_DEBUG);
    LOG_DEBUG("GLOBAL", "global debug after setAllLevel");

    Logger::setAllLevel(LOG_LEVEL_ERROR);
    LOG_ERROR("GLOBAL", "global error after setAllLevel");
}

TEST_F(LoggerTest, ConfigDrivenLogLevel) {
    // 通过配置修改日志级别
    ConfigManager::instance().set(CONFIG_LOG_LEVEL, 0);
    LOG_DEBUG("CONFIG_TEST", "debug after config change to 0");

    ConfigManager::instance().set(CONFIG_LOG_LEVEL, 3);
    LOG_ERROR("CONFIG_TEST", "error after config change to 3");
}

TEST_F(LoggerTest, ConfigDrivenFileParams) {
    // 验证配置键存在且有默认值
    EXPECT_EQ(ConfigManager::instance().get<int>(
        CONFIG_LOG_MAX_FILE_SIZE, CONFIG_LOG_DEFAULT_MAX_FILE_SIZE),
        CONFIG_LOG_DEFAULT_MAX_FILE_SIZE);
    EXPECT_EQ(ConfigManager::instance().get<int>(
        CONFIG_LOG_MAX_FILE_COUNT, CONFIG_LOG_DEFAULT_MAX_FILE_COUNT),
        CONFIG_LOG_DEFAULT_MAX_FILE_COUNT);
    EXPECT_EQ(ConfigManager::instance().get<int>(
        CONFIG_LOG_KEEP_DAYS, CONFIG_LOG_DEFAULT_KEEP_DAYS),
        CONFIG_LOG_DEFAULT_KEEP_DAYS);
}

TEST_F(LoggerTest, LogFileCreated) {
    // 输出日志
    LOG_INFO("FILE_TEST", "verify log file exists");

    // flush确保写入
    spdlog::default_logger()->flush();

    // 验证日志文件存在
    QString docPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString logPath = docPath + "/etest/logs/etest.log";
    EXPECT_TRUE(QFile::exists(logPath));
}

TEST_F(LoggerTest, MultipleModulesLog) {
    // 多个模块同时输出日志，验证按需创建模块logger不崩溃
    LOG_INFO("MODULE_A", "message from module A");
    LOG_INFO("MODULE_B", "message from module B");
    LOG_INFO("MODULE_C", "message from module C");

    // 同一模块再次输出
    LOG_INFO("MODULE_A", "second message from module A");
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
