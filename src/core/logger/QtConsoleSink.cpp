#include "QtConsoleSink.h"

QtConsoleSink::QtConsoleSink(QObject* parent) : QObject(parent) {
  set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
}

QtConsoleSink::~QtConsoleSink() = default;

void QtConsoleSink::sink_it_(const spdlog::details::log_msg& msg) {
  spdlog::memory_buf_t formatted;
  formatter_->format(msg, formatted);
  emit logMessage(static_cast<int>(msg.level),
                  QString::fromUtf8(formatted.data(),
                                    static_cast<int>(formatted.size())));
}

void QtConsoleSink::flush_() {}
