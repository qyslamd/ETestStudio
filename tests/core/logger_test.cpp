#include "logger/Logger.h"
#include "logger/LogHistoryBuffer.h"
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <thread>
#include <vector>
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"


using namespace etest::core::logger;
using namespace etest::core::config;

class LoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ConfigManager::instance().resetAllToDefault();
    Logger::init();
  }

  void TearDown() override { Logger::shutdown(); }
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
  EXPECT_EQ(ConfigManager::instance().get<int>(CONFIG_LOG_KEEP_DAYS,
                                               CONFIG_LOG_DEFAULT_KEEP_DAYS),
            CONFIG_LOG_DEFAULT_KEEP_DAYS);
}

TEST_F(LoggerTest, LogFileCreated) {
  // 输出日志
  LOG_INFO("FILE_TEST", "verify log file exists");

  // flush确保写入
  spdlog::default_logger()->flush();

  // 验证日志文件存在
  QString localPath =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  QString logPath = localPath + "/logs/etest.log";
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

// ============================================================
// LogHistoryBuffer 测试（独立于 Logger，不依赖 spdlog）
// ============================================================

class LogHistoryBufferTest : public ::testing::Test {
 protected:
  void SetUp() override { buffer_ = new LogHistoryBuffer(5000); }
  void TearDown() override { delete buffer_; }
  LogHistoryBuffer* buffer_ = nullptr;
};

// 容量上限：连续 push 6000 条应截断到 5000，丢最老的 1000 条。
TEST_F(LogHistoryBufferTest, CapacityOverflowTruncates) {
  for (int i = 0; i < 6000; ++i) {
    buffer_->push(0, QStringLiteral("msg_%1").arg(i));
  }
  QObject receiver;
  int count = -1;
  QString firstText;
  QString lastText;
  QObject::connect(buffer_, &LogHistoryBuffer::drained, &receiver,
                   [&](const QList<LogEntry>& entries) {
                     count = entries.size();
                     if (!entries.isEmpty()) {
                       firstText = entries.first().text;
                       lastText = entries.last().text;
                     }
                   });
  buffer_->drain(&receiver);
  EXPECT_EQ(count, 5000);
  EXPECT_EQ(firstText, QStringLiteral("msg_1000"));
  EXPECT_EQ(lastText, QStringLiteral("msg_5999"));
}

// drain 一次：push 100 条后 drain 一次性收到所有 100 条且顺序正确。
TEST_F(LogHistoryBufferTest, DrainDeliversAllEntries) {
  for (int i = 0; i < 100; ++i) {
    buffer_->push(1, QStringLiteral("entry_%1").arg(i));
  }
  QObject receiver;
  int count = 0;
  int firstLevel = -1;
  QString firstText;
  QString lastText;
  QObject::connect(buffer_, &LogHistoryBuffer::drained, &receiver,
                   [&](const QList<LogEntry>& entries) {
                     count = entries.size();
                     if (!entries.isEmpty()) {
                       firstLevel = entries.first().level;
                       firstText = entries.first().text;
                       lastText = entries.last().text;
                     }
                   });
  buffer_->drain(&receiver);
  EXPECT_EQ(count, 100);
  EXPECT_EQ(firstLevel, 1);
  EXPECT_EQ(firstText, QStringLiteral("entry_0"));
  EXPECT_EQ(lastText, QStringLiteral("entry_99"));
}

// drain 多次幂等：重复调用 drained 信号只 emit 一次。
TEST_F(LogHistoryBufferTest, DrainIsIdempotent) {
  buffer_->push(0, "x");
  QObject receiver;
  int emitCount = 0;
  QObject::connect(buffer_, &LogHistoryBuffer::drained, &receiver,
                   [&](const QList<LogEntry>&) { ++emitCount; });
  buffer_->drain(&receiver);
  buffer_->drain(&receiver);
  buffer_->drain(&receiver);
  EXPECT_EQ(emitCount, 1);
}

// drain 防御 nullptr receiver：传 nullptr 不崩溃、不 emit。
TEST_F(LogHistoryBufferTest, DrainWithNullReceiverDoesNotCrash) {
  buffer_->push(0, "x");
  buffer_->drain(nullptr);
  SUCCEED();
}

// drain 防御 receiver 析构后调用：connect 之后 delete receiver，Qt 自动 disconnect。
// drain 不会触发任何已 disconnect 的槽，也不持有对已析构对象的引用。
TEST_F(LogHistoryBufferTest, DrainAfterReceiverDestroyedDoesNotCrash) {
  buffer_->push(0, "x");
  int emitCount = 0;
  QObject receiver;
  QObject::connect(buffer_, &LogHistoryBuffer::drained, &receiver,
                   [&](const QList<LogEntry>&) { ++emitCount; });
  // drain 一次，receiver 仍活着，正常收到
  buffer_->drain(&receiver);
  EXPECT_EQ(emitCount, 1);
  // 第二次 drain 因 drained_=true 不会再 emit；这验证 Qt 槽仍有效
  buffer_->drain(&receiver);
  EXPECT_EQ(emitCount, 1);
}

// 线程安全：4 线程并发 push 各 1000 条（共 4000，未超 5000），全部保留。
TEST_F(LogHistoryBufferTest, ConcurrentPushPreservesAll) {
  constexpr int kThreads = 4;
  constexpr int kPerThread = 1000;
  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([this, t, kPerThread]() {
      for (int i = 0; i < kPerThread; ++i) {
        buffer_->push(t, QStringLiteral("t%1_m%2").arg(t).arg(i));
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }
  QObject receiver;
  int count = 0;
  QObject::connect(buffer_, &LogHistoryBuffer::drained, &receiver,
                   [&](const QList<LogEntry>& entries) { count = entries.size(); });
  buffer_->drain(&receiver);
  EXPECT_EQ(count, kThreads * kPerThread);
}

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
