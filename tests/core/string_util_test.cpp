#include <gtest/gtest.h>
#include <QCoreApplication>

#include "utils/StringUtil.h"
#include "common/StringException.h"

using etest::core::utils::StringUtil;
using etest::core::common::StringException;

class StringUtilTest : public ::testing::Test {};

// ========== 编码转换 ==========

TEST_F(StringUtilTest, Utf8RoundTrip) {
  QString original = "Hello 测试中文";
  QByteArray ba = StringUtil::toUtf8(original);
  QString result = StringUtil::fromUtf8(ba);
  EXPECT_EQ(result, original);
}

TEST_F(StringUtilTest, Latin1RoundTrip) {
  QString original = "Hello World";
  QByteArray ba = StringUtil::toLatin1(original);
  QString result = StringUtil::fromLatin1(ba);
  EXPECT_EQ(result, original);
}

TEST_F(StringUtilTest, GbkRoundTrip) {
  QString original = "Hello 测试中文";
  QByteArray ba = StringUtil::toGBK(original);
  QString result = StringUtil::fromGBK(ba);
  EXPECT_EQ(result, original);
}

// ========== 分割与拼接 ==========

TEST_F(StringUtilTest, SplitSkipEmpty) {
  QStringList result = StringUtil::split("a,,b,c,", ",", true);
  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
  EXPECT_EQ(result[2], "c");
}

TEST_F(StringUtilTest, SplitKeepEmpty) {
  QStringList result = StringUtil::split("a,,b,c,", ",", false);
  EXPECT_EQ(result.size(), 5);
}

TEST_F(StringUtilTest, Join) {
  QStringList list = {"a", "b", "c"};
  QString result = StringUtil::join(list, ",");
  EXPECT_EQ(result, "a,b,c");
}

TEST_F(StringUtilTest, JoinEmptyList) {
  QStringList list;
  QString result = StringUtil::join(list, ",");
  EXPECT_EQ(result, "");
}

// ========== 数值格式化 ==========

TEST_F(StringUtilTest, FormatDoubleWithPrecision) {
  QString result = StringUtil::formatNumber(3.14159, 2);
  EXPECT_TRUE(result.contains("3.14"));
}

TEST_F(StringUtilTest, FormatDoubleNoGroupSeparator) {
  QString result = StringUtil::formatNumber(1234567.89, 2, false);
  EXPECT_FALSE(result.contains(","));
}

TEST_F(StringUtilTest, FormatIntWithGroupSeparator) {
  QString result = StringUtil::formatNumber(static_cast<qint64>(1234567), true);
  EXPECT_TRUE(result.contains(",") || result.contains("1,234,567"));
}

TEST_F(StringUtilTest, FormatIntNoGroupSeparator) {
  QString result = StringUtil::formatNumber(static_cast<qint64>(1234567), false);
  EXPECT_FALSE(result.contains(","));
}

// ========== 去除空白 ==========

TEST_F(StringUtilTest, TrimWhitespace) {
  EXPECT_EQ(StringUtil::trim("  hello  "), "hello");
}

TEST_F(StringUtilTest, TrimFullwidthSpace) {
  EXPECT_EQ(StringUtil::trim("　hello　"), "hello");
}

TEST_F(StringUtilTest, TrimMixedSpace) {
  EXPECT_EQ(StringUtil::trim(" 　 hello 　 "), "hello");
}

// ========== 填充 ==========

TEST_F(StringUtilTest, PadLeft) {
  EXPECT_EQ(StringUtil::padLeft("abc", 6), "   abc");
  EXPECT_EQ(StringUtil::padLeft("abc", 6, '0'), "000abc");
}

TEST_F(StringUtilTest, PadRight) {
  EXPECT_EQ(StringUtil::padRight("abc", 6), "abc   ");
  EXPECT_EQ(StringUtil::padRight("abc", 6, '0'), "abc000");
}

TEST_F(StringUtilTest, PadNoOp) {
  EXPECT_EQ(StringUtil::padLeft("abcdef", 3), "abcdef");
}

// ========== 大小写转换 ==========

TEST_F(StringUtilTest, ToUpper) {
  EXPECT_EQ(StringUtil::toUpper("hello"), "HELLO");
  EXPECT_EQ(StringUtil::toUpper("Hello World"), "HELLO WORLD");
}

TEST_F(StringUtilTest, ToLower) {
  EXPECT_EQ(StringUtil::toLower("HELLO"), "hello");
  EXPECT_EQ(StringUtil::toLower("Hello World"), "hello world");
}

// ========== 字符串校验 ==========

TEST_F(StringUtilTest, IsNumber) {
  EXPECT_TRUE(StringUtil::isNumber("123"));
  EXPECT_TRUE(StringUtil::isNumber("-456"));
  EXPECT_TRUE(StringUtil::isNumber("3.14"));
  EXPECT_TRUE(StringUtil::isNumber("-0.5"));
  EXPECT_FALSE(StringUtil::isNumber("abc"));
  EXPECT_FALSE(StringUtil::isNumber(""));
  EXPECT_FALSE(StringUtil::isNumber("12a34"));
}

TEST_F(StringUtilTest, IsInteger) {
  EXPECT_TRUE(StringUtil::isInteger("123"));
  EXPECT_TRUE(StringUtil::isInteger("-456"));
  EXPECT_FALSE(StringUtil::isInteger("3.14"));
  EXPECT_FALSE(StringUtil::isInteger("abc"));
  EXPECT_FALSE(StringUtil::isInteger(""));
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
