#include "utils/StringUtil.h"
#include "common/StringException.h"

#include <QTextCodec>
#include <QLocale>

namespace etest {
namespace core {
namespace utils {

QByteArray StringUtil::toUtf8(const QString& str) {
  return str.toUtf8();
}

QString StringUtil::fromUtf8(const QByteArray& ba) {
  return QString::fromUtf8(ba);
}

QByteArray StringUtil::toGBK(const QString& str) {
  auto* codec = QTextCodec::codecForName("GBK");
  if (!codec) {
    throw common::StringException(common::StringException::Code::kEncodingError,
                                  "GBK codec not available");
  }
  return codec->fromUnicode(str);
}

QString StringUtil::fromGBK(const QByteArray& ba) {
  auto* codec = QTextCodec::codecForName("GBK");
  if (!codec) {
    throw common::StringException(common::StringException::Code::kEncodingError,
                                  "GBK codec not available");
  }
  return codec->toUnicode(ba);
}

QByteArray StringUtil::toLatin1(const QString& str) {
  return str.toLatin1();
}

QString StringUtil::fromLatin1(const QByteArray& ba) {
  return QString::fromLatin1(ba);
}

QStringList StringUtil::split(const QString& str, const QString& sep, bool skipEmpty) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  auto behavior = skipEmpty ? Qt::SkipEmptyParts : Qt::KeepEmptyParts;
#else
  auto behavior = skipEmpty ? QString::SkipEmptyParts : QString::KeepEmptyParts;
#endif
  return str.split(sep, behavior);
}

QString StringUtil::join(const QStringList& list, const QString& sep) {
  return list.join(sep);
}

QString StringUtil::formatNumber(double value, int precision, bool groupSeparator) {
  QLocale locale;
  locale.setNumberOptions(groupSeparator ? QLocale::DefaultNumberOptions
                                         : QLocale::OmitGroupSeparator);
  return locale.toString(value, 'f', precision);
}

QString StringUtil::formatNumber(qint64 value, bool groupSeparator) {
  QLocale locale;
  locale.setNumberOptions(groupSeparator ? QLocale::DefaultNumberOptions
                                         : QLocale::OmitGroupSeparator);
  return locale.toString(value);
}

QString StringUtil::trim(const QString& str) {
  QString result = str.trimmed();
  // 去除中文全角空格 (U+3000)
  while (result.startsWith(QChar(0x3000))) {
    result.remove(0, 1);
  }
  while (result.endsWith(QChar(0x3000))) {
    result.chop(1);
  }
  return result;
}

QString StringUtil::padLeft(const QString& str, int width, QChar fill) {
  return str.rightJustified(width, fill);
}

QString StringUtil::padRight(const QString& str, int width, QChar fill) {
  return str.leftJustified(width, fill);
}

QString StringUtil::toUpper(const QString& str) {
  return str.toUpper();
}

QString StringUtil::toLower(const QString& str) {
  return str.toLower();
}

bool StringUtil::isNumber(const QString& str) {
  if (str.isEmpty()) {
    return false;
  }
  bool ok = false;
  str.toDouble(&ok);
  return ok;
}

bool StringUtil::isInteger(const QString& str) {
  if (str.isEmpty()) {
    return false;
  }
  bool ok = false;
  str.toLongLong(&ok);
  return ok;
}

}  // namespace utils
}  // namespace core
}  // namespace etest
