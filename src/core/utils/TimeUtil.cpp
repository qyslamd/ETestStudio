#include "utils/TimeUtil.h"

#include <QDateTime>

#include "logger/Logger.h"
#include "common/TimeException.h"

namespace etest {
namespace core {
namespace utils {

namespace {

const QString kDefaultFormat = "yyyy-MM-dd hh:mm:ss";
const QString kISO8601Format = Qt::ISODate;

void throwTimeException(::etest::core::common::TimeException::Code code,
                        const QString& message,
                        const QString& input = QString()) {
  if (input.isEmpty()) {
    throw ::etest::core::common::TimeException(code, message);
  } else {
    throw ::etest::core::common::TimeException(code, message, input);
  }
}

}  // namespace

qint64 TimeUtil::currentTimestamp() {
  return QDateTime::currentSecsSinceEpoch();
}

qint64 TimeUtil::currentTimestampMillis() {
  return QDateTime::currentMSecsSinceEpoch();
}

QString TimeUtil::timestampToString(qint64 timestamp, const QString& format) {
  QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp);
  return dateTime.toString(format.isEmpty() ? kDefaultFormat : format);
}

QString TimeUtil::timestampToISO8601(qint64 timestamp) {
  QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp);
  return dateTime.toString(Qt::ISODate);
}

qint64 TimeUtil::stringToTimestamp(const QString& str, const QString& format) {
  QDateTime dateTime = QDateTime::fromString(str, format.isEmpty() ? kDefaultFormat : format);
  if (!dateTime.isValid()) {
    throwTimeException(::etest::core::common::TimeException::Code::kParseError,
                       "Invalid date time string",
                       str);
    return -1;
  }
  return dateTime.toSecsSinceEpoch();
}

qint64 TimeUtil::ISO8601ToTimestamp(const QString& str) {
  QDateTime dateTime = QDateTime::fromString(str, Qt::ISODate);
  if (!dateTime.isValid()) {
    throwTimeException(::etest::core::common::TimeException::Code::kParseError,
                       "Invalid ISO8601 string",
                       str);
    return -1;
  }
  return dateTime.toSecsSinceEpoch();
}

QString TimeUtil::formatNow(const QString& format) {
  return timestampToString(currentTimestamp(), format);
}

QString TimeUtil::formatNowISO8601() {
  return timestampToISO8601(currentTimestamp());
}

qint64 TimeUtil::elapsed(qint64 start, qint64 end) {
  return end - start;
}

QString TimeUtil::formatDuration(qint64 seconds) {
  if (seconds < 0) {
    return "0s";
  }

  qint64 days = seconds / 86400;
  qint64 hours = (seconds % 86400) / 3600;
  qint64 minutes = (seconds % 3600) / 60;
  qint64 secs = seconds % 60;

  QStringList parts;
  if (days > 0) parts.append(QString("%1d").arg(days));
  if (hours > 0) parts.append(QString("%1h").arg(hours));
  if (minutes > 0) parts.append(QString("%1m").arg(minutes));
  if (secs > 0 || parts.isEmpty()) parts.append(QString("%1s").arg(secs));

  return parts.join(" ");
}

const QString& TimeUtil::defaultFormat() {
  return kDefaultFormat;
}

const QString& TimeUtil::iso8601Format() {
  return kISO8601Format;
}

TimeUtil::ScopedTimer::ScopedTimer(const QString& taskName)
    : taskName_(taskName), start_(std::chrono::steady_clock::now()) {}

TimeUtil::ScopedTimer::~ScopedTimer() {
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
  LOG_INFO("TIME", "%s took %lld ms", qUtf8Printable(taskName_), duration.count());
}

}  // namespace utils
}  // namespace core
}  // namespace etest