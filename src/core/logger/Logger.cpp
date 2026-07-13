#include "Logger.h"
#include "LogHistoryBuffer.h"
#include "QtConsoleSink.h"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/wincolor_sink.h>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

using namespace etest::core::config;

namespace etest::core::logger {

bool Logger::s_initialized = false;
QMutex Logger::s_mutex;
QtConsoleSink* Logger::s_qtSink = nullptr;
LogHistoryBuffer* Logger::s_historyBuffer = nullptr;
std::unordered_map<std::string, spdlog::logger*> Logger::s_moduleLoggers;

void Logger::init() {
  if (s_initialized) {
    return;
  }

  // 创建日志存储目录：AppData/Local/ETestStudio/logs/
  QString localPath =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  QString logDir = localPath + "/logs";
  QDir().mkpath(logDir);

  // 从配置读取清理天数，未配置则使用默认值7天
  int keepDays = ConfigManager::instance().get<int>(
      CONFIG_LOG_KEEP_DAYS, CONFIG_LOG_DEFAULT_KEEP_DAYS);

  // 清理历史日志
  QDir dir(logDir);
  dir.setNameFilters({"*.log"});
  dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
  QFileInfoList fileList = dir.entryInfoList();
  QDateTime now = QDateTime::currentDateTime();
  for (const auto& fileInfo : fileList) {
    if (fileInfo.lastModified().daysTo(now) > keepDays) {
      QFile::remove(fileInfo.absoluteFilePath());
    }
  }

  // 异步日志配置
  spdlog::init_thread_pool(8192, 1);

  // 创建控制台彩色sink（仅 Debug 构建启用）
#ifdef _DEBUG
  auto consoleSink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
  consoleSink->set_level(spdlog::level::debug);
  consoleSink->set_pattern(
      "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%n] [%s:%#] %v");
#endif

  // 从配置读取文件大小和保留数量，未配置则使用默认值
  int maxFileSize = ConfigManager::instance().get<int>(
      CONFIG_LOG_MAX_FILE_SIZE, CONFIG_LOG_DEFAULT_MAX_FILE_SIZE);
  int maxFileCount = ConfigManager::instance().get<int>(
      CONFIG_LOG_MAX_FILE_COUNT, CONFIG_LOG_DEFAULT_MAX_FILE_COUNT);

  // 创建滚动文件sink
  QString logPath = logDir + "/etest.log";
  auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      logPath.toStdString(), maxFileSize, maxFileCount, true);
  fileSink->set_level(spdlog::level::debug);
  fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%n] [%s:%#] %v");

  // 创建启动期历史日志缓冲（环形 5000 条）。LogOutputPanel 出现后回放。
  // 必须先于 QtConsoleSink 创建，让 sink 在首条日志时就能 push 到 history。
  s_historyBuffer = new LogHistoryBuffer(5000);

  // 创建Qt控制台sink，输出到界面
  auto qtSink = std::make_shared<QtConsoleSink>(s_historyBuffer);
  qtSink->set_level(spdlog::level::debug);
  s_qtSink = qtSink.get();

  // 注册默认logger，所有模块共用
#ifdef _DEBUG
  std::vector<spdlog::sink_ptr> sinks = {consoleSink, fileSink, qtSink};
#else
  std::vector<spdlog::sink_ptr> sinks = {fileSink, qtSink};
#endif
  auto defaultLogger = std::make_shared<spdlog::async_logger>(
      "default", sinks.begin(), sinks.end(), spdlog::thread_pool(),
      spdlog::async_overflow_policy::block);
  defaultLogger->flush_on(spdlog::level::err);
  spdlog::register_logger(defaultLogger);
  spdlog::set_default_logger(defaultLogger);

  // 从配置读取日志级别
  auto updateLogLevel = []() {
    int level = ConfigManager::instance().get<int>(CONFIG_LOG_LEVEL,
                                                   CONFIG_LOG_DEFAULT_LEVEL);
    spdlog::level::level_enum spdLevel = spdlog::level::info;
    switch (level) {
      case 0:
        spdLevel = spdlog::level::debug;
        break;
      case 1:
        spdLevel = spdlog::level::info;
        break;
      case 2:
        spdLevel = spdlog::level::warn;
        break;
      case 3:
        spdLevel = spdlog::level::err;
        break;
      case 4:
        spdLevel = spdlog::level::critical;
        break;
      default:
        spdLevel = spdlog::level::info;
        break;
    }
    spdlog::set_level(spdLevel);
  };
  updateLogLevel();

  // 监听配置变更，日志级别实时生效
  QObject::connect(
      &ConfigManager::instance(), &ConfigManager::configChanged,
      [=](const QString& key) {
        if (key == CONFIG_LOG_LEVEL) {
          updateLogLevel();
          LOG_INFO("LOGGER", "日志级别已更新为: {}",
                   ConfigManager::instance().get<int>(CONFIG_LOG_LEVEL));
        }
      });

  // 自动每秒flush一次
  spdlog::flush_every(std::chrono::seconds(1));

  s_initialized = true;
  LOG_INFO("LOGGER", "日志系统初始化完成，当前日志级别: {}",
           ConfigManager::instance().get<int>(CONFIG_LOG_LEVEL));
}

void Logger::shutdown() {
  if (!s_initialized) {
    return;
  }
  LOG_INFO("LOGGER", "日志系统关闭");
  s_moduleLoggers.clear();
  s_qtSink = nullptr;
  spdlog::shutdown();
  // spdlog::shutdown() 析构所有 sink（QtConsoleSink 在其中），
  // 之后 history_ 已无引用者，可以安全 delete。
  delete s_historyBuffer;
  s_historyBuffer = nullptr;
  s_initialized = false;
}

void Logger::setLevel(const QString& module, LogLevel level) {
  if (!s_initialized)
    return;
  QMutexLocker locker(&s_mutex);
  std::string key = module.toStdString();
  auto it = s_moduleLoggers.find(key);
  if (it != s_moduleLoggers.end()) {
    it->second->set_level(static_cast<spdlog::level::level_enum>(level));
  }
}

void Logger::setAllLevel(LogLevel level) {
  if (!s_initialized)
    return;
  spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
  // 同步更新所有模块logger的级别
  QMutexLocker locker(&s_mutex);
  for (auto& pair : s_moduleLoggers) {
    pair.second->set_level(static_cast<spdlog::level::level_enum>(level));
  }
}

spdlog::logger* Logger::getOrCreateModuleLogger(const std::string& moduleName) {
  auto it = s_moduleLoggers.find(moduleName);
  if (it != s_moduleLoggers.end()) {
    return it->second;
  }

  // 按需创建模块logger，复用default logger的sink
  auto defaultLogger = spdlog::default_logger();
  if (!defaultLogger) {
    return nullptr;
  }

  auto moduleLogger = std::make_shared<spdlog::async_logger>(
      moduleName, defaultLogger->sinks().begin(), defaultLogger->sinks().end(),
      spdlog::thread_pool(), spdlog::async_overflow_policy::block);
  moduleLogger->set_level(defaultLogger->level());
  moduleLogger->flush_on(spdlog::level::err);

  // 如果spdlog全局registry中已存在同名logger，先移除再注册
  if (spdlog::get(moduleName)) {
    spdlog::drop(moduleName);
  }
  spdlog::register_logger(moduleLogger);

  auto* rawPtr = moduleLogger.get();
  s_moduleLoggers[moduleName] = rawPtr;
  return rawPtr;
}

spdlog::logger* Logger::getLogger(const QString& module) {
  if (!s_initialized) return nullptr;
  QMutexLocker locker(&s_mutex);
  return getOrCreateModuleLogger(module.toStdString());
}

QtConsoleSink* Logger::qtConsoleSink() {
  return s_qtSink;
}

LogHistoryBuffer* Logger::qtHistoryBuffer() {
  return s_historyBuffer;
}

}  // namespace etest::core::logger