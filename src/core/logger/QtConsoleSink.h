#ifndef ETEST_CORE_LOGGER_QT_CONSOLE_SINK_H_
#define ETEST_CORE_LOGGER_QT_CONSOLE_SINK_H_

#include <QObject>
#include <mutex>
#include <spdlog/sinks/base_sink.h>

class QtConsoleSink : public QObject,
                       public spdlog::sinks::base_sink<std::mutex> {
  Q_OBJECT

 public:
  explicit QtConsoleSink(QObject* parent = nullptr);
  ~QtConsoleSink() override;

 signals:
  void logMessage(int level, const QString& formattedText);

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override;
};

#endif  // ETEST_CORE_LOGGER_QT_CONSOLE_SINK_H_
