#ifndef ETEST_CORE_UTILS_BYTE_UTIL_H_
#define ETEST_CORE_UTILS_BYTE_UTIL_H_

#include <QtGlobal>
#include <QByteArray>
#include <QString>

namespace etest {
namespace core {
namespace utils {

// 字节序枚举
enum class ByteOrder {
  kBigEndian,
  kLittleEndian
};

class ByteUtil {
 public:
  // 字节序翻转
  static quint16 swapEndian16(quint16 val);
  static quint32 swapEndian32(quint32 val);
  static quint64 swapEndian64(quint64 val);

  // 主机序 → 大端序
  static quint16 toBigEndian16(quint16 val);
  static quint32 toBigEndian32(quint32 val);
  static quint64 toBigEndian64(quint64 val);

  // 大端序 → 主机序
  static quint16 fromBigEndian16(quint16 val);
  static quint32 fromBigEndian32(quint32 val);
  static quint64 fromBigEndian64(quint64 val);

  // 主机序 → 小端序
  static quint16 toLittleEndian16(quint16 val);
  static quint32 toLittleEndian32(quint32 val);
  static quint64 toLittleEndian64(quint64 val);

  // 小端序 → 主机序
  static quint16 fromLittleEndian16(quint16 val);
  static quint32 fromLittleEndian32(quint32 val);
  static quint64 fromLittleEndian64(quint64 val);

  // 数值 → 字节数组（默认大端序）
  static QByteArray int16ToBytes(qint16 val, ByteOrder order = ByteOrder::kBigEndian);
  static QByteArray int32ToBytes(qint32 val, ByteOrder order = ByteOrder::kBigEndian);
  static QByteArray int64ToBytes(qint64 val, ByteOrder order = ByteOrder::kBigEndian);
  static QByteArray floatToBytes(float val, ByteOrder order = ByteOrder::kBigEndian);
  static QByteArray doubleToBytes(double val, ByteOrder order = ByteOrder::kBigEndian);

  // 字节数组 → 数值（默认大端序）
  static qint16 bytesToInt16(const QByteArray& ba, ByteOrder order = ByteOrder::kBigEndian);
  static qint32 bytesToInt32(const QByteArray& ba, ByteOrder order = ByteOrder::kBigEndian);
  static qint64 bytesToInt64(const QByteArray& ba, ByteOrder order = ByteOrder::kBigEndian);
  static float bytesToFloat(const QByteArray& ba, ByteOrder order = ByteOrder::kBigEndian);
  static double bytesToDouble(const QByteArray& ba, ByteOrder order = ByteOrder::kBigEndian);

  // 十六进制转换
  static QString toHex(const QByteArray& ba, const QString& separator = QString());
  static QByteArray fromHex(const QString& hexStr);

  // CRC 校验
  static quint16 crc16(const QByteArray& ba);
  static quint32 crc32(const QByteArray& ba);
};

}  // namespace utils
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_UTILS_BYTE_UTIL_H_
