#include "Logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <cstdarg>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/wincolor_sink.h>
#include <spdlog/async.h>

bool Logger::s_initialized = false;

void Logger::init() {
    if (s_initialized) {
        return;
    }

    // 创建日志存储目录
    QString docPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString logDir = docPath + "/etest/logs";
    QDir().mkpath(logDir);

    // 清理7天以上的历史日志
    QDir dir(logDir);
    dir.setNameFilters({"*.log"});
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    QFileInfoList fileList = dir.entryInfoList();
    QDateTime now = QDateTime::currentDateTime();
    for (const auto& fileInfo : fileList) {
        if (fileInfo.lastModified().daysTo(now) > 7) {
            QFile::remove(fileInfo.absoluteFilePath());
        }
    }

    // 异步日志配置
    spdlog::init_thread_pool(8192, 1);

    // 创建控制台彩色sink（Windows平台专用，兼容性更好）
    auto consoleSink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
    consoleSink->set_level(spdlog::level::debug);
    consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%n] [%s:%#] %v");

    // 创建滚动文件sink：10MB分割，保留20个文件
    QString logPath = logDir + "/etest.log";
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logPath.toStdString(),
        10 * 1024 * 1024, // 10MB
        20,
        true
    );
    fileSink->set_level(spdlog::level::debug);
    fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%n] [%s:%#] %v");

    // 注册默认logger，所有模块共用
    std::vector<spdlog::sink_ptr> sinks = {consoleSink, fileSink};
    auto defaultLogger = std::make_shared<spdlog::async_logger>(
        "default",
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );
    defaultLogger->set_level(spdlog::level::debug);
    defaultLogger->flush_on(spdlog::level::err);
    spdlog::register_logger(defaultLogger);
    spdlog::set_default_logger(defaultLogger);

    // 自动每秒flush一次
    spdlog::flush_every(std::chrono::seconds(1));

    s_initialized = true;
    LOG_INFO("LOGGER", "日志系统初始化完成");
}

void Logger::shutdown() {
    if (!s_initialized) {
        return;
    }
    LOG_INFO("LOGGER", "日志系统关闭");
    spdlog::shutdown();
    s_initialized = false;
}

void Logger::setLevel(const QString& module, LogLevel level) {
    if (!s_initialized) return;
    auto logger = spdlog::get(module.toStdString());
    if (logger) {
        logger->set_level(static_cast<spdlog::level::level_enum>(level));
    }
}

void Logger::setAllLevel(LogLevel level) {
    if (!s_initialized) return;
    spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
}

void Logger::log(const QString& module, LogLevel level, const char* file, int line, const char* format, ...) {
    if (!s_initialized) return;

    auto logger = spdlog::get(module.toStdString());
    if (!logger) {
        logger = spdlog::default_logger();
    }

    va_list args;
    va_start(args, format);
    char buf[1024] = {0};
    vsnprintf(buf, sizeof(buf) - 1, format, args);
    va_end(args);

    spdlog::source_loc loc(file, line, "");
    logger->log(loc, static_cast<spdlog::level::level_enum>(level), buf);
}
