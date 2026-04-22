#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QMutex>
#include <spdlog/spdlog.h>
#include <unordered_map>

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

    // 内部日志输出接口，不建议直接调用，使用下面的宏
    static void log(const QString& module, LogLevel level, const char* file, int line, const char* format, ...);

private:
    // 按需创建模块logger，线程安全（调用方需持有s_mutex）
    static spdlog::logger* getOrCreateModuleLogger(const std::string& moduleName);
    static bool s_initialized;
    static QMutex s_mutex;
    static std::unordered_map<std::string, spdlog::logger*> s_moduleLoggers;
};

// 全局日志调用宏
#define LOG_DEBUG(module, format, ...) \
    Logger::log(module, LOG_LEVEL_DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LOG_INFO(module, format, ...) \
    Logger::log(module, LOG_LEVEL_INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LOG_WARN(module, format, ...) \
    Logger::log(module, LOG_LEVEL_WARN, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LOG_ERROR(module, format, ...) \
    Logger::log(module, LOG_LEVEL_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LOG_FATAL(module, format, ...) \
    Logger::log(module, LOG_LEVEL_FATAL, __FILE__, __LINE__, format, ##__VA_ARGS__)

#endif // LOGGER_H
