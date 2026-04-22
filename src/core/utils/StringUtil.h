#ifndef ETEST_CORE_UTILS_STRING_UTIL_H_
#define ETEST_CORE_UTILS_STRING_UTIL_H_

#include <QString>
#include <QStringList>
#include <QByteArray>

namespace etest {
namespace core {
namespace utils {

class StringUtil {
 public:
  // 编码转换：QString ↔ QByteArray
  static QByteArray toUtf8(const QString& str);
  static QString fromUtf8(const QByteArray& ba);

  static QByteArray toGBK(const QString& str);
  static QString fromGBK(const QByteArray& ba);

  static QByteArray toLatin1(const QString& str);
  static QString fromLatin1(const QByteArray& ba);

  // 字符串分割与拼接
  static QStringList split(const QString& str, const QString& sep, bool skipEmpty = true);
  static QString join(const QStringList& list, const QString& sep);

  // 数值格式化
  static QString formatNumber(double value, int precision = 2, bool groupSeparator = true);
  static QString formatNumber(qint64 value, bool groupSeparator = true);

  // 去除空白
  static QString trim(const QString& str);

  // 填充
  static QString padLeft(const QString& str, int width, QChar fill = QLatin1Char(' '));
  static QString padRight(const QString& str, int width, QChar fill = QLatin1Char(' '));

  // 大小写转换
  static QString toUpper(const QString& str);
  static QString toLower(const QString& str);

  // 字符串校验
  static bool isNumber(const QString& str);
  static bool isInteger(const QString& str);
};

}  // namespace utils
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_UTILS_STRING_UTIL_H_
