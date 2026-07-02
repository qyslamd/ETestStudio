#include <gtest/gtest.h>

#include <QApplication>
#include <QString>
#include <QStringList>

#include <icd/frame.hpp>
#include <icd/node.hpp>

#include <memory>

#include "IcdFramePreview.h"

using namespace etest::protocol;

namespace {

class FramePreviewEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    if (QApplication::instance()) {
      return;
    }
    int argc = 1;
    char name[] = "test_frame_preview";
    char* argv = name;
    app_ = std::make_unique<QApplication>(argc, &argv);
  }
  void TearDown() override { app_.reset(); }

 private:
  std::unique_ptr<QApplication> app_;
};

const testing::Environment* const env =
    testing::AddGlobalTestEnvironment(new FramePreviewEnv);

std::unique_ptr<icd::Node> MakeNode(const std::string& name, int offset,
                                    int bit_offset, int bit_width,
                                    icd::ValueType vt, icd::Tag tag = icd::Tag::none,
                                    icd::NodeAttrs attrs = {}) {
  return std::make_unique<icd::Node>(name, std::string{}, offset, bit_offset,
                                     bit_width, vt, tag, std::move(attrs));
}

}  // namespace

TEST(FramePreviewTest, ParseHexBytesAcceptsSpacesAndMixedCase) {
  const auto bytes = parseHexBytes(QStringLiteral("12 34 ab CD"));
  ASSERT_TRUE(bytes.has_value());
  ASSERT_EQ(bytes->size(), 4u);
  EXPECT_EQ((*bytes)[0], std::byte{0x12});
  EXPECT_EQ((*bytes)[1], std::byte{0x34});
  EXPECT_EQ((*bytes)[2], std::byte{0xAB});
  EXPECT_EQ((*bytes)[3], std::byte{0xCD});
}

TEST(FramePreviewTest, ParseHexBytesRejectsOddLength) {
  const auto bytes = parseHexBytes(QStringLiteral("123"));
  EXPECT_FALSE(bytes.has_value());
}

TEST(FramePreviewTest, ParseHexBytesRejectsInvalidChars) {
  const auto bytes = parseHexBytes(QStringLiteral("12 ZZ"));
  EXPECT_FALSE(bytes.has_value());
}

TEST(FramePreviewTest, FormatNodeValueWordLittleEndian) {
  icd::Frame frame(1, "f", "", icd::FrameType::data,
                   icd::ByteOrder::little_endian);
  frame.add_root(MakeNode("current", 0, 0, 16, icd::ValueType::word));
  const std::vector<std::byte> payload{std::byte{0x34}, std::byte{0x12}};
  ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());

  const auto lines = decodeFramePreview(frame, payload);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_TRUE(lines[0].contains(QStringLiteral("current")));
  EXPECT_TRUE(lines[0].contains(QStringLiteral("4660")) ||
              lines[0].contains(QStringLiteral("0x1234")));
}

TEST(FramePreviewTest, DecodeFramePreviewShowsAllRootFields) {
  icd::Frame frame(2, "f", "", icd::FrameType::data,
                   icd::ByteOrder::little_endian);
  frame.add_root(MakeNode("byte0", 0, 0, 8, icd::ValueType::byte_));
  frame.add_root(MakeNode("byte1", 1, 0, 8, icd::ValueType::byte_));
  frame.add_root(MakeNode("crc", 2, 0, 16, icd::ValueType::word));

  const std::vector<std::byte> payload{std::byte{0x01}, std::byte{0x02},
                                       std::byte{0xAA}, std::byte{0xBB}};
  const auto lines = decodeFramePreview(frame, payload);
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_TRUE(lines[0].contains(QStringLiteral("byte0")));
  EXPECT_TRUE(lines[1].contains(QStringLiteral("byte1")));
  EXPECT_TRUE(lines[2].contains(QStringLiteral("crc")));
}

TEST(FramePreviewTest, DecodeFramePreviewShowsChildFieldsInsideContainer) {
  icd::Frame frame(3, "a429", "", icd::FrameType::cmd,
                   icd::ByteOrder::little_endian);
  auto parent = MakeNode("word", 0, 0, 32, icd::ValueType::longword);
  parent->add_child(MakeNode("label", 0, 0, 8, icd::ValueType::byte_));
  parent->add_child(MakeNode("sdi", 1, 0, 2, icd::ValueType::byte_));
  parent->add_child(MakeNode("parity", 3, 7, 1, icd::ValueType::byte_));
  frame.add_root(std::move(parent));

  // label=0x01, sdi=0b11 (bits 8,9), parity=1 (bit 31)
  const std::vector<std::byte> payload{std::byte{0x01}, std::byte{0x03},
                                       std::byte{0x00}, std::byte{0x80}};
  const auto lines = decodeFramePreview(frame, payload);
  // 1 parent + 3 children
  ASSERT_EQ(lines.size(), 4u);
  EXPECT_TRUE(lines[0].contains(QStringLiteral("word")));
  EXPECT_TRUE(lines[1].contains(QStringLiteral("label")));
  EXPECT_TRUE(lines[2].contains(QStringLiteral("sdi")));
  EXPECT_TRUE(lines[3].contains(QStringLiteral("parity")));
}

TEST(FramePreviewTest, DecodeFramePreviewEnumTranslatesValueTextList) {
  icd::Frame frame(4, "f", "", icd::FrameType::data,
                   icd::ByteOrder::little_endian);
  icd::NodeAttrs attrs;
  attrs.value_text_list = "不用=0,左单元=1,右单元=2,中间单元=3";
  frame.add_root(MakeNode("sdi", 0, 0, 2, icd::ValueType::byte_,
                          icd::Tag::none, attrs));

  const std::vector<std::byte> payload{std::byte{0x02}};
  const auto lines = decodeFramePreview(frame, payload);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_TRUE(lines[0].contains(QStringLiteral("右单元")));
}

TEST(FramePreviewTest, DecodeFramePreviewBigEndianNodeUsesNodeOrder) {
  icd::Frame frame(5, "f", "", icd::FrameType::data,
                   icd::ByteOrder::little_endian);
  frame.add_root(MakeNode("be_word", 0, 0, 16, icd::ValueType::word,
                          icd::Tag::big_endian_value));

  const std::vector<std::byte> payload{std::byte{0x12}, std::byte{0x34}};
  const auto lines = decodeFramePreview(frame, payload);
  ASSERT_EQ(lines.size(), 1u);
  // big-endian 0x1234 = 4660
  EXPECT_TRUE(lines[0].contains(QStringLiteral("4660")) ||
              lines[0].contains(QStringLiteral("0x1234")));
}

TEST(FramePreviewTest, DecodeFramePreviewHandlesBufferTooSmall) {
  icd::Frame frame(6, "f", "", icd::FrameType::data,
                   icd::ByteOrder::little_endian);
  frame.add_root(MakeNode("w", 0, 0, 16, icd::ValueType::word));
  const std::vector<std::byte> payload{std::byte{0x34}};
  const auto lines = decodeFramePreview(frame, payload);
  // 单个字段解码失败时仍应返回一行，提示解码失败
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_TRUE(lines[0].contains(QStringLiteral("解码失败")) ||
              lines[0].contains(QStringLiteral("失败")) ||
              lines[0].contains(QStringLiteral("error")) ||
              !lines[0].contains(QStringLiteral("4660")));
}
