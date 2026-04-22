#include <gtest/gtest.h>
#include <QCoreApplication>

#include "utils/ByteUtil.h"
#include "common/ByteException.h"

using etest::core::utils::ByteUtil;
using etest::core::utils::ByteOrder;
using etest::core::common::ByteException;

class ByteUtilTest : public ::testing::Test {};

// ========== 字节序翻转 ==========

TEST_F(ByteUtilTest, SwapEndian16) {
  EXPECT_EQ(ByteUtil::swapEndian16(0x1234), 0x3412);
  EXPECT_EQ(ByteUtil::swapEndian16(0xABCD), 0xCDAB);
}

TEST_F(ByteUtilTest, SwapEndian32) {
  EXPECT_EQ(ByteUtil::swapEndian32(0x12345678), 0x78563412);
}

TEST_F(ByteUtilTest, SwapEndian64) {
  EXPECT_EQ(ByteUtil::swapEndian64(0x0123456789ABCDEFULL), 0xEFCDAB8967452301ULL);
}

TEST_F(ByteUtilTest, SwapEndianIdentity) {
  // 翻转两次应回到原值
  quint16 v16 = 0x1234;
  EXPECT_EQ(ByteUtil::swapEndian16(ByteUtil::swapEndian16(v16)), v16);
  quint32 v32 = 0x12345678;
  EXPECT_EQ(ByteUtil::swapEndian32(ByteUtil::swapEndian32(v32)), v32);
  quint64 v64 = 0x0123456789ABCDEFULL;
  EXPECT_EQ(ByteUtil::swapEndian64(ByteUtil::swapEndian64(v64)), v64);
}

// ========== 大端序转换 ==========

TEST_F(ByteUtilTest, BigEndianRoundTrip16) {
  quint16 original = 0x1234;
  quint16 big = ByteUtil::toBigEndian16(original);
  quint16 result = ByteUtil::fromBigEndian16(big);
  EXPECT_EQ(result, original);
}

TEST_F(ByteUtilTest, BigEndianRoundTrip32) {
  quint32 original = 0x12345678;
  quint32 big = ByteUtil::toBigEndian32(original);
  quint32 result = ByteUtil::fromBigEndian32(big);
  EXPECT_EQ(result, original);
}

TEST_F(ByteUtilTest, BigEndianRoundTrip64) {
  quint64 original = 0x0123456789ABCDEFULL;
  quint64 big = ByteUtil::toBigEndian64(original);
  quint64 result = ByteUtil::fromBigEndian64(big);
  EXPECT_EQ(result, original);
}

// ========== 小端序转换 ==========

TEST_F(ByteUtilTest, LittleEndianRoundTrip16) {
  quint16 original = 0x1234;
  quint16 little = ByteUtil::toLittleEndian16(original);
  quint16 result = ByteUtil::fromLittleEndian16(little);
  EXPECT_EQ(result, original);
}

TEST_F(ByteUtilTest, LittleEndianRoundTrip32) {
  quint32 original = 0x12345678;
  quint32 little = ByteUtil::toLittleEndian32(original);
  quint32 result = ByteUtil::fromLittleEndian32(little);
  EXPECT_EQ(result, original);
}

TEST_F(ByteUtilTest, LittleEndianRoundTrip64) {
  quint64 original = 0x0123456789ABCDEFULL;
  quint64 little = ByteUtil::toLittleEndian64(original);
  quint64 result = ByteUtil::fromLittleEndian64(little);
  EXPECT_EQ(result, original);
}

// ========== 数值 ↔ 字节数组（整数） ==========

TEST_F(ByteUtilTest, Int16ToBytesBigEndian) {
  QByteArray ba = ByteUtil::int16ToBytes(0x1234, ByteOrder::kBigEndian);
  EXPECT_EQ(ba.size(), 2);
  EXPECT_EQ(static_cast<unsigned char>(ba[0]), 0x12);
  EXPECT_EQ(static_cast<unsigned char>(ba[1]), 0x34);
}

TEST_F(ByteUtilTest, Int16ToBytesLittleEndian) {
  QByteArray ba = ByteUtil::int16ToBytes(0x1234, ByteOrder::kLittleEndian);
  EXPECT_EQ(ba.size(), 2);
  EXPECT_EQ(static_cast<unsigned char>(ba[0]), 0x34);
  EXPECT_EQ(static_cast<unsigned char>(ba[1]), 0x12);
}

TEST_F(ByteUtilTest, Int32ToBytesBigEndian) {
  QByteArray ba = ByteUtil::int32ToBytes(0x12345678, ByteOrder::kBigEndian);
  EXPECT_EQ(ba.size(), 4);
  EXPECT_EQ(static_cast<unsigned char>(ba[0]), 0x12);
  EXPECT_EQ(static_cast<unsigned char>(ba[3]), 0x78);
}

TEST_F(ByteUtilTest, Int64ToBytesBigEndian) {
  QByteArray ba = ByteUtil::int64ToBytes(0x0102030405060708LL, ByteOrder::kBigEndian);
  EXPECT_EQ(ba.size(), 8);
  EXPECT_EQ(static_cast<unsigned char>(ba[0]), 0x01);
  EXPECT_EQ(static_cast<unsigned char>(ba[7]), 0x08);
}

TEST_F(ByteUtilTest, BytesToInt16RoundTrip) {
  qint16 original = -12345;
  QByteArray ba = ByteUtil::int16ToBytes(original);
  qint16 result = ByteUtil::bytesToInt16(ba);
  EXPECT_EQ(result, original);
}

TEST_F(ByteUtilTest, BytesToInt32RoundTrip) {
  qint32 original = -123456789;
  QByteArray ba = ByteUtil::int32ToBytes(original);
  qint32 result = ByteUtil::bytesToInt32(ba);
  EXPECT_EQ(result, original);
}

TEST_F(ByteUtilTest, BytesToInt64RoundTrip) {
  qint64 original = -12345678901234LL;
  QByteArray ba = ByteUtil::int64ToBytes(original);
  qint64 result = ByteUtil::bytesToInt64(ba);
  EXPECT_EQ(result, original);
}

TEST_F(ByteUtilTest, BytesToIntLittleEndianRoundTrip) {
  qint32 original = 0x12345678;
  QByteArray ba = ByteUtil::int32ToBytes(original, ByteOrder::kLittleEndian);
  qint32 result = ByteUtil::bytesToInt32(ba, ByteOrder::kLittleEndian);
  EXPECT_EQ(result, original);
}

// ========== 数值 ↔ 字节数组（浮点） ==========

TEST_F(ByteUtilTest, FloatToBytesRoundTrip) {
  float original = 3.14f;
  QByteArray ba = ByteUtil::floatToBytes(original);
  float result = ByteUtil::bytesToFloat(ba);
  EXPECT_FLOAT_EQ(result, original);
}

TEST_F(ByteUtilTest, DoubleToBytesRoundTrip) {
  double original = 3.141592653589793;
  QByteArray ba = ByteUtil::doubleToBytes(original);
  double result = ByteUtil::bytesToDouble(ba);
  EXPECT_DOUBLE_EQ(result, original);
}

TEST_F(ByteUtilTest, FloatLittleEndianRoundTrip) {
  float original = -2.718f;
  QByteArray ba = ByteUtil::floatToBytes(original, ByteOrder::kLittleEndian);
  float result = ByteUtil::bytesToFloat(ba, ByteOrder::kLittleEndian);
  EXPECT_FLOAT_EQ(result, original);
}

TEST_F(ByteUtilTest, DoubleLittleEndianRoundTrip) {
  double original = -2.718281828;
  QByteArray ba = ByteUtil::doubleToBytes(original, ByteOrder::kLittleEndian);
  double result = ByteUtil::bytesToDouble(ba, ByteOrder::kLittleEndian);
  EXPECT_DOUBLE_EQ(result, original);
}

// ========== 大小不匹配异常 ==========

TEST_F(ByteUtilTest, BytesToInt16SizeMismatch) {
  QByteArray ba(3, '\0');
  EXPECT_THROW(ByteUtil::bytesToInt16(ba), ByteException);
}

TEST_F(ByteUtilTest, BytesToInt32SizeMismatch) {
  QByteArray ba(2, '\0');
  EXPECT_THROW(ByteUtil::bytesToInt32(ba), ByteException);
}

TEST_F(ByteUtilTest, BytesToFloatSizeMismatch) {
  QByteArray ba(3, '\0');
  EXPECT_THROW(ByteUtil::bytesToFloat(ba), ByteException);
}

// ========== 十六进制转换 ==========

TEST_F(ByteUtilTest, ToHexNoSeparator) {
  QByteArray ba("\x12\x34\xAB\xCD", 4);
  QString hex = ByteUtil::toHex(ba);
  EXPECT_EQ(hex, "1234abcd");
}

TEST_F(ByteUtilTest, ToHexWithSeparator) {
  QByteArray ba("\x12\x34\xAB", 3);
  QString hex = ByteUtil::toHex(ba, " ");
  EXPECT_EQ(hex, "12 34 ab");
}

TEST_F(ByteUtilTest, FromHexClean) {
  QString hex = "1234abcd";
  QByteArray ba = ByteUtil::fromHex(hex);
  EXPECT_EQ(ba.size(), 4);
  EXPECT_EQ(static_cast<unsigned char>(ba[0]), 0x12);
  EXPECT_EQ(static_cast<unsigned char>(ba[3]), 0xCD);
}

TEST_F(ByteUtilTest, FromHexWithSeparators) {
  QByteArray ba = ByteUtil::fromHex("12 34:ab-cd");
  EXPECT_EQ(ba.size(), 4);
}

TEST_F(ByteUtilTest, HexRoundTrip) {
  QByteArray original = QByteArray::fromHex("deadbeef0102030405");
  QString hex = ByteUtil::toHex(original);
  QByteArray result = ByteUtil::fromHex(hex);
  EXPECT_EQ(result, original);
}

// ========== CRC 校验 ==========

TEST_F(ByteUtilTest, Crc16KnownValue) {
  // CRC-16/MODBUS of "123456789" should be 0x4B37
  QByteArray data("123456789");
  quint16 crc = ByteUtil::crc16(data);
  EXPECT_EQ(crc, 0x4B37);
}

TEST_F(ByteUtilTest, Crc16Empty) {
  QByteArray data;
  quint16 crc = ByteUtil::crc16(data);
  EXPECT_EQ(crc, 0xFFFF);
}

TEST_F(ByteUtilTest, Crc32KnownValue) {
  // CRC-32 of "123456789" should be 0xCBF43926
  QByteArray data("123456789");
  quint32 crc = ByteUtil::crc32(data);
  EXPECT_EQ(crc, static_cast<quint32>(0xCBF43926));
}

TEST_F(ByteUtilTest, Crc32Empty) {
  QByteArray data;
  quint32 crc = ByteUtil::crc32(data);
  EXPECT_EQ(crc, static_cast<quint32>(0x00000000));
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
