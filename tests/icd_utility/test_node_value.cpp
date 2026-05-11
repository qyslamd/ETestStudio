#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/frame.hpp>
#include <icd/node.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace {

template <typename T>
bool expect_value(const tl::expected<icd::NodeValue, icd::Error>& value, const T& expected) {
    return value.has_value() && std::holds_alternative<T>(*value) && std::get<T>(*value) == expected;
}

std::unique_ptr<icd::Node> make_node(std::string name, int offset, int bit_offset, int bit_width, icd::ValueType vt) {
    return std::make_unique<icd::Node>(std::move(name), "", offset, bit_offset, bit_width, vt, icd::Tag::none, icd::NodeAttrs{});
}

} // namespace

TEST(NodeValueTest, EagerDecodeWord) {
    auto root = make_node("root", 0, 0, 16, icd::ValueType::word);
    auto child = make_node("child", 2, 0, 8, icd::ValueType::byte_);
    auto* child_ptr = child.get();
    root->add_child(std::move(child));
    icd::Frame frame(1, "frame", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto* root_ptr = root.get();
    frame.add_root(std::move(root));
    const std::array<std::byte, 3> payload{std::byte{0x34}, std::byte{0x12}, std::byte{0xAB}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    auto root_value = root_ptr->get_value();
    ASSERT_TRUE(root_value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::uint16_t>(**root_value));
    EXPECT_EQ(std::get<std::uint16_t>(**root_value), static_cast<std::uint16_t>(0x1234));
    auto child_value = child_ptr->get_value();
    ASSERT_TRUE(child_value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::uint64_t>(**child_value));
    EXPECT_EQ(std::get<std::uint64_t>(**child_value), static_cast<std::uint64_t>(0xAB));
}

TEST(NodeValueTest, LazyDecodeWord) {
    auto root = make_node("root", 0, 0, 16, icd::ValueType::word);
    auto* root_ptr = root.get();
    icd::Frame frame(2, "frame-lazy", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    frame.add_root(std::move(root));
    const std::array<std::byte, 2> payload{std::byte{0x78}, std::byte{0x56}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::lazy).has_value());
    EXPECT_TRUE(root_ptr->modified());
    auto value = root_ptr->get_value();
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::uint16_t>(**value));
    EXPECT_EQ(std::get<std::uint16_t>(**value), static_cast<std::uint16_t>(0x5678));
    EXPECT_FALSE(root_ptr->modified());
}

TEST(NodeValueTest, SetValueNoFrame) {
    icd::Node node("manual", "", 0, 0, 16, icd::ValueType::word, icd::Tag::none, {});
    auto result = node.set_value(icd::NodeValue{static_cast<std::uint16_t>(0x2222)});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, SetValueAndPropagate) {
    auto root = make_node("parent", 0, 0, 16, icd::ValueType::word);
    auto child = make_node("child", 0, 0, 8, icd::ValueType::byte_);
    auto* child_ptr = child.get();
    root->add_child(std::move(child));
    icd::Frame frame(3, "frame-set", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto* root_ptr = root.get();
    frame.add_root(std::move(root));
    const std::array<std::byte, 2> payload{std::byte{0x11}, std::byte{0x22}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    EXPECT_FALSE(child_ptr->modified());
    ASSERT_TRUE(root_ptr->set_value(icd::NodeValue{static_cast<std::uint16_t>(0x3344)}).has_value());
    auto root_value = root_ptr->get_value();
    ASSERT_TRUE(root_value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::uint16_t>(**root_value));
    EXPECT_EQ(std::get<std::uint16_t>(**root_value), static_cast<std::uint16_t>(0x3344));
    EXPECT_TRUE(child_ptr->modified());
    auto child_value = child_ptr->get_value();
    ASSERT_TRUE(child_value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::uint64_t>(**child_value));
    EXPECT_EQ(std::get<std::uint64_t>(**child_value), static_cast<std::uint64_t>(0x44));
    auto root_decoded = root_ptr->decode(icd::span<const std::byte>(payload), icd::ByteOrder::little_endian);
    ASSERT_TRUE(root_decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<std::uint16_t>(*root_decoded));
    EXPECT_EQ(std::get<std::uint16_t>(*root_decoded), static_cast<std::uint16_t>(0x2211));
}

TEST(NodeValueTest, BytesType) {
    icd::Frame frame(4, "frame-bytes", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto node = make_node("bytes", 1, 0, 16, icd::ValueType::bytes);
    auto* node_ptr = node.get();
    frame.add_root(std::move(node));
    const std::array<std::byte, 4> payload{std::byte{0x00}, std::byte{0x11}, std::byte{0x22}, std::byte{0x00}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    ASSERT_TRUE(node_ptr->set_value(icd::NodeValue{std::vector<std::byte>{std::byte{0xAA}, std::byte{0xBB}}}).has_value());
    auto value = node_ptr->get_value();
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<std::byte>>(**value));
    const auto& bytes = std::get<std::vector<std::byte>>(**value);
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], std::byte{0xAA});
    EXPECT_EQ(bytes[1], std::byte{0xBB});
    auto decoded = node_ptr->decode(icd::span<const std::byte>(payload), icd::ByteOrder::little_endian);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<std::byte>>(*decoded));
    const auto& stale = std::get<std::vector<std::byte>>(*decoded);
    ASSERT_EQ(stale.size(), 2u);
    EXPECT_EQ(stale[0], std::byte{0x11});
    EXPECT_EQ(stale[1], std::byte{0x22});
}

TEST(NodeValueTest, StringType) {
    icd::Frame frame(5, "frame-string", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto node = make_node("str", 0, 0, 24, icd::ValueType::string_);
    auto* node_ptr = node.get();
    frame.add_root(std::move(node));
    const std::array<std::byte, 3> payload{std::byte{'X'}, std::byte{'Y'}, std::byte{'Z'}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    ASSERT_TRUE(node_ptr->set_value(icd::NodeValue{std::string{"ABC"}}).has_value());
    auto value = node_ptr->get_value();
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(**value));
    EXPECT_EQ(std::get<std::string>(**value), "ABC");
}

TEST(NodeValueTest, LengthMismatchBytes) {
    icd::Frame frame(6, "frame-bytes-mismatch", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto node = make_node("bytes", 0, 0, 16, icd::ValueType::bytes);
    auto* node_ptr = node.get();
    frame.add_root(std::move(node));
    const std::array<std::byte, 2> payload{std::byte{0x11}, std::byte{0x22}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    auto result = node_ptr->set_value(icd::NodeValue{std::vector<std::byte>{std::byte{0xAA}}});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, LengthMismatchString) {
    icd::Frame frame(7, "frame-string-mismatch", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto node = make_node("str", 0, 0, 24, icd::ValueType::string_);
    auto* node_ptr = node.get();
    frame.add_root(std::move(node));
    const std::array<std::byte, 3> payload{std::byte{'X'}, std::byte{'Y'}, std::byte{'Z'}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    auto result = node_ptr->set_value(icd::NodeValue{std::string{"AB"}});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, DecodeWord) {
    icd::Node node("word", "", 0, 0, 16, icd::ValueType::word, icd::Tag::none, {});
    const std::array<std::byte, 2> frame{std::byte{0x34}, std::byte{0x12}};
    EXPECT_TRUE(expect_value<std::uint16_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::uint16_t>(0x1234)));
}

TEST(NodeValueTest, DecodeSmallint) {
    icd::Node node("smallint", "", 0, 0, 16, icd::ValueType::smallint, icd::Tag::none, {});
    const std::array<std::byte, 2> frame{std::byte{0xFE}, std::byte{0xFF}};
    EXPECT_TRUE(expect_value<std::int16_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::int16_t>(-2)));
}

TEST(NodeValueTest, DecodeLongword) {
    icd::Node node("longword", "", 0, 0, 32, icd::ValueType::longword, icd::Tag::none, {});
    const std::array<std::byte, 4> frame{std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78}};
    EXPECT_TRUE(expect_value<std::uint32_t>(node.decode(frame, icd::ByteOrder::big_endian), static_cast<std::uint32_t>(0x12345678)));
}

TEST(NodeValueTest, DecodeInteger) {
    icd::Node node("integer", "", 0, 0, 32, icd::ValueType::integer, icd::Tag::none, {});
    const std::array<std::byte, 4> frame{std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x80}};
    EXPECT_TRUE(expect_value<std::int32_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::int32_t>(-2130706433)));
}

TEST(NodeValueTest, DecodeByte) {
    icd::Node node("byte", "", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0xAB}};
    EXPECT_TRUE(expect_value<std::uint64_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::uint64_t>(0xAB)));
}

TEST(NodeValueTest, DecodeShortint) {
    icd::Node node("shortint", "", 0, 0, 8, icd::ValueType::shortint, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0xFE}};
    EXPECT_TRUE(expect_value<std::int64_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::int64_t>(-2)));
}

TEST(NodeValueTest, DecodeBoolean) {
    icd::Node node("flag", "", 0, 2, 1, icd::ValueType::boolean, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0b00000100}};
    EXPECT_TRUE(expect_value<bool>(node.decode(frame, icd::ByteOrder::little_endian), true));
}

TEST(NodeValueTest, DecodeSingle) {
    icd::Node node("single", "", 0, 0, 32, icd::ValueType::single, icd::Tag::none, {});
    const std::array<std::byte, 4> frame{std::byte{0x00}, std::byte{0x00}, std::byte{0x80}, std::byte{0x3F}};
    EXPECT_TRUE(expect_value<double>(node.decode(frame, icd::ByteOrder::little_endian), 1.0));
}

TEST(NodeValueTest, DecodeDouble) {
    icd::Node node("double", "", 0, 0, 64, icd::ValueType::double_, icd::Tag::none, {});
    const std::array<std::byte, 8> frame{std::byte{0x3F}, std::byte{0xF0}, std::byte{0x00}, std::byte{0x00},
                                         std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(expect_value<double>(node.decode(frame, icd::ByteOrder::big_endian), 1.0));
}

TEST(NodeValueTest, DecodeBytes) {
    icd::Node node("bytes", "", 1, 0, 16, icd::ValueType::bytes, icd::Tag::none, {});
    const std::array<std::byte, 4> frame{std::byte{0x00}, std::byte{0xAA}, std::byte{0xBB}, std::byte{0x00}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<std::byte>>(*value));
    const auto& bytes = std::get<std::vector<std::byte>>(*value);
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], std::byte{0xAA});
    EXPECT_EQ(bytes[1], std::byte{0xBB});
}

TEST(NodeValueTest, DecodeString) {
    icd::Node node("string", "", 0, 0, 24, icd::ValueType::string_, icd::Tag::none, {});
    const std::array<std::byte, 3> frame{std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    EXPECT_TRUE(expect_value<std::string>(node.decode(frame, icd::ByteOrder::little_endian), std::string{"ABC"}));
}

TEST(NodeValueTest, DecodeOutOfRange) {
    icd::Node node("oor", "", 1, 0, 16, icd::ValueType::word, icd::Tag::none, {});
    const std::array<std::byte, 2> frame{std::byte{0x00}, std::byte{0x01}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, DecodeNonAlignedBytes) {
    icd::Node node("bad-bytes", "", 0, 1, 16, icd::ValueType::bytes, icd::Tag::none, {});
    const std::array<std::byte, 3> frame{std::byte{0x00}, std::byte{0x01}, std::byte{0x02}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, DecodeUnknownType) {
    icd::Node node("unknown", "", 0, 0, 8, icd::ValueType::unknown, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0x00}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, DecodeWordWrongWidth) {
    icd::Node node("bad-word", "", 0, 0, 8, icd::ValueType::word, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0x01}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error().code, icd::ErrorCode::invalid_argument);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
