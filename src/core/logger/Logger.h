#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/spdlog.h>
#include <QMutex>
#include <QString>
#include <unordered_map>

class QtConsoleSink;

namespace etest::core::logger {
class LogHistoryBuffer;

// 日志级别枚举
enum LogLevel {
  LOG_LEVEL_DEBUG = spdlog::level::debug,
  LOG_LEVEL_INFO = spdlog::level::info,
  LOG_LEVEL_WARN = spdlog::level::warn,
  LOG_LEVEL_ERROR = spdlog::level::err,
  LOG_LEVEL_FATAL = spdlog::level::critical
};

// 日志系统对外接口
class Logger {
 public:
  // 初始化日志系统，程序启动时调用一次
  static void init();

  // 关闭日志系统，程序退出时调用一次
  static void shutdown();

  // 设置指定模块的日志级别
  static void setLevel(const QString& module, LogLevel level);

  // 设置所有模块的日志级别
  static void setAllLevel(LogLevel level);

  // 获取Qt控制台sink，用于连接UI信号
  static QtConsoleSink* qtConsoleSink();

  // 获取启动期历史日志缓冲。Logger 未 init 时返回 nullptr。
  static LogHistoryBuffer* qtHistoryBuffer();

  // 获取模块logger指针，供LOG宏使用
  static spdlog::logger* getLogger(const QString& module);

 private:
  // 按需创建模块logger，线程安全（调用方需持有s_mutex）
  static spdlog::logger* getOrCreateModuleLogger(const std::string& moduleName);
  static bool s_initialized;
  static QMutex s_mutex;
  static QtConsoleSink* s_qtSink;
  static LogHistoryBuffer* s_historyBuffer;
  static std::unordered_map<std::string, spdlog::logger*> s_moduleLoggers;
};
}  // namespace etest::core::logger

// 全局日志调用宏 - 使用fmt格式化语法 ({})
#define LOG_DEBUG(module, ...)                                           \
  do {                                                                   \
    auto* _l = etest::core::logger::Logger::getLogger(module);           \
    if (_l) _l->log(spdlog::source_loc{__FILE__, __LINE__, ""},        \
                     spdlog::level::debug, __VA_ARGS__);                 \
  } while (0)

#define LOG_INFO(module, ...)                                            \
  do {                                                                   \
    auto* _l = etest::core::logger::Logger::getLogger(module);           \
    if (_l) _l->log(spdlog::source_loc{__FILE__, __LINE__, ""},        \
                     spdlog::level::info, __VA_ARGS__);                  \
  } while (0)

#define LOG_WARN(module, ...)                                            \
  do {                                                                   \
    auto* _l = etest::core::logger::Logger::getLogger(module);           \
    if (_l) _l->log(spdlog::source_loc{__FILE__, __LINE__, ""},        \
                     spdlog::level::warn, __VA_ARGS__);                  \
  } while (0)

#define LOG_ERROR(module, ...)                                           \
  do {                                                                   \
    auto* _l = etest::core::logger::Logger::getLogger(module);           \
    if (_l) _l->log(spdlog::source_loc{__FILE__, __LINE__, ""},        \
                     spdlog::level::err, __VA_ARGS__);                   \
  } while (0)

#define LOG_FATAL(module, ...)                                           \
  do {                                                                   \
    auto* _l = etest::core::logger::Logger::getLogger(module);           \
    if (_l) _l->log(spdlog::source_loc{__FILE__, __LINE__, ""},        \
                     spdlog::level::critical, __VA_ARGS__);              \
  } while (0)

#endif  // LOGGER_H
