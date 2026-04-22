#ifndef ETEST_CORE_UTILS_TIME_UTIL_H_
#define ETEST_CORE_UTILS_TIME_UTIL_H_

#include <QString>

#include <chrono>

namespace etest {
namespace core {
namespace utils {

class TimeUtil {
 public:
  static qint64 currentTimestamp();
  static qint64 currentTimestampMillis();

  static QString timestampToString(qint64 timestamp, const QString& format);
  static QString timestampToISO8601(qint64 timestamp);

  static qint64 stringToTimestamp(const QString& str, const QString& format);
  static qint64 ISO8601ToTimestamp(const QString& str);

  static QString formatNow(const QString& format);
  static QString formatNowISO8601();

  static qint64 elapsed(qint64 start, qint64 end);
  static QString formatDuration(qint64 seconds);

  static const QString& defaultFormat();
  static const QString& iso8601Format();

  class ScopedTimer {
   public:
    explicit ScopedTimer(const QString& taskName);
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

   private:
    QString taskName_;
    std::chrono::steady_clock::time_point start_;
  };
};

}  // namespace utils
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_UTILS_TIME_UTIL_H_