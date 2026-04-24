#include "Logger.h"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/wincolor_sink.h>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <cstdarg>
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

using namespace etest::core::config;

namespace etest::core::logger {

bool Logger::s_initialized = false;
QMutex Logger::s_mutex;
std::unordered_map<std::string, spdlog::logger*> Logger::s_moduleLoggers;

void Logger::init() {
  if (s_initialized) {
    return;
  }

  // 创建日志存储目录
  QString docPath =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  QString logDir = docPath + "/etest/logs";
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

  // 创建控制台彩色sink
  auto consoleSink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
  consoleSink->set_level(spdlog::level::debug);
  consoleSink->set_pattern(
      "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%n] [%s:%#] %v");

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

  // 注册默认logger，所有模块共用
  std::vector<spdlog::sink_ptr> sinks = {consoleSink, fileSink};
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
  spdlog::shutdown();
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
  spdlog::register_logger(moduleLogger);

  auto* rawPtr = moduleLogger.get();
  s_moduleLoggers[moduleName] = rawPtr;
  return rawPtr;
}

void Logger::log(const QString& module,
                 LogLevel level,
                 const char* file,
                 int line,
                 const char* format,
                 ...) {
  if (!s_initialized)
    return;

  std::string moduleName = module.toStdString();
  spdlog::logger* logger = nullptr;

  {
    QMutexLocker locker(&s_mutex);
    logger = getOrCreateModuleLogger(moduleName);
  }

  if (!logger) {
    logger = spdlog::default_logger().get();
  }

  va_list args;
  va_start(args, format);
  char buf[1024] = {0};
  vsnprintf(buf, sizeof(buf) - 1, format, args);
  va_end(args);

  spdlog::source_loc loc(file, line, "");
  logger->log(loc, static_cast<spdlog::level::level_enum>(level), buf);
}

}  // namespace etest::core::logger