#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/repository.hpp>
#include <icd/frame.hpp>
#include <icd/node.hpp>

#include "../src/format/xml_serializer.hpp"
#include "../src/format/json_serializer.hpp"
#include "../src/format/json_parser.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;
using namespace icd;
using namespace icd::format;

struct EprotoxTest : ::testing::Test {
    fs::path tempDir = fs::temp_directory_path() / "eprotox_test";

    void SetUp() override {
        fs::create_directories(tempDir);
    }

    void TearDown() override {
        fs::remove_all(tempDir);
    }

    // Helper: create a simple node
    static std::unique_ptr<Node> make_node(std::string name,
                                           int offset,
                                           int bit_offset,
                                           int bit_width,
                                           ValueType value_type = ValueType::byte_,
                                           Tag tag = Tag::none) {
        return std::make_unique<Node>(
            std::move(name), std::string{}, offset, bit_offset, bit_width,
            value_type, tag, NodeAttrs{});
    }
};

// ── Test 1: Empty Repository ───────────────────────────────────
TEST_F(EprotoxTest, EmptyRepository) {
    Repository repo;
    auto path = tempDir / "empty.eprotox";

    auto write_result = serialize_xml_repository(path, repo);
    ASSERT_TRUE(write_result.has_value()) << write_result.error().message;

    auto read_result = deserialize_xml_repository(path);
    ASSERT_TRUE(read_result.has_value()) << read_result.error().message;

    EXPECT_EQ(read_result->frames().size(), 0u);
}

// ── Test 2: Single Frame ───────────────────────────────────────
TEST_F(EprotoxTest, SingleFrame) {
    Repository repo;
    auto frame = std::make_unique<Frame>(42, "TestFrame", "A test frame",
                                         FrameType::cmd, ByteOrder::big_endian);

    NodeAttrs attrs;
    attrs.unit = "A";
    attrs.is_scaled = false;
    attrs.scale_a = 0.01f;

    auto node = std::make_unique<Node>("LABEL", "Label 110", 0, 0, 8,
                                       ValueType::byte_, Tag::head, std::move(attrs));
    frame->add_root(std::move(node));
    repo.add_frame(std::move(frame));

    auto path = tempDir / "single.eprotox";
    auto write_result = serialize_xml_repository(path, repo);
    ASSERT_TRUE(write_result.has_value()) << write_result.error().message;

    auto read_result = deserialize_xml_repository(path);
    ASSERT_TRUE(read_result.has_value()) << read_result.error().message;

    ASSERT_EQ(read_result->frames().size(), 1u);
    const auto& read_frame = *read_result->frames()[0];

    EXPECT_EQ(read_frame.id(), 42);
    EXPECT_EQ(read_frame.name(), "TestFrame");
    EXPECT_EQ(read_frame.description(), "A test frame");
    EXPECT_EQ(read_frame.type(), FrameType::cmd);
    EXPECT_EQ(read_frame.order(), ByteOrder::big_endian);

    ASSERT_EQ(read_frame.roots().size(), 1u);
    const auto& read_node = *read_frame.roots()[0];
    EXPECT_EQ(read_node.name(), "LABEL");
    EXPECT_EQ(read_node.description(), "Label 110");
    EXPECT_EQ(read_node.offset(), 0);
    EXPECT_EQ(read_node.bit_offset(), 0);
    EXPECT_EQ(read_node.bit_width(), 8);
    EXPECT_EQ(read_node.value_type(), ValueType::byte_);
    EXPECT_EQ(read_node.tag(), Tag::head);

    const auto& read_attrs = read_node.attrs();
    EXPECT_EQ(read_attrs.unit, "A");
    EXPECT_FALSE(read_attrs.is_scaled);
    ASSERT_TRUE(read_attrs.scale_a.has_value());
    EXPECT_FLOAT_EQ(*read_attrs.scale_a, 0.01f);
}

// ── Test 3: Multiple Frames ────────────────────────────────────
TEST_F(EprotoxTest, MultipleFrames) {
    Repository repo;

    auto frame1 = std::make_unique<Frame>(1, "Frame_A", "First frame",
                                          FrameType::data, ByteOrder::little_endian);
    frame1->add_root(make_node("Node_A", 0, 0, 8, ValueType::byte_));
    repo.add_frame(std::move(frame1));

    auto frame2 = std::make_unique<Frame>(2, "Frame_B", "Second frame",
                                          FrameType::cmd, ByteOrder::big_endian);
    frame2->add_root(make_node("Node_B", 0, 0, 16, ValueType::word));
    frame2->add_root(make_node("Node_C", 2, 0, 32, ValueType::longword));
    repo.add_frame(std::move(frame2));

    auto frame3 = std::make_unique<Frame>(3, "Frame_C", "Third frame",
                                          FrameType::data_cmd, ByteOrder::little_endian);
    frame3->add_root(make_node("Node_D", 0, 0, 32, ValueType::integer));
    repo.add_frame(std::move(frame3));

    auto path = tempDir / "multi.eprotox";
    auto write_result = serialize_xml_repository(path, repo);
    ASSERT_TRUE(write_result.has_value()) << write_result.error().message;

    auto read_result = deserialize_xml_repository(path);
    ASSERT_TRUE(read_result.has_value()) << read_result.error().message;

    ASSERT_EQ(read_result->frames().size(), 3u);
    EXPECT_EQ(read_result->frames()[0]->id(), 1);
    EXPECT_EQ(read_result->frames()[0]->type(), FrameType::data);
    EXPECT_EQ(read_result->frames()[1]->id(), 2);
    EXPECT_EQ(read_result->frames()[1]->order(), ByteOrder::big_endian);
    EXPECT_EQ(read_result->frames()[2]->id(), 3);
    EXPECT_EQ(read_result->frames()[2]->type(), FrameType::data_cmd);

    // Verify node count for frame 2
    ASSERT_EQ(read_result->frames()[1]->roots().size(), 2u);
    EXPECT_EQ(read_result->frames()[1]->roots()[0]->name(), "Node_B");
    EXPECT_EQ(read_result->frames()[1]->roots()[1]->name(), "Node_C");
}

// ── Test 4: Nested Nodes ───────────────────────────────────────
TEST_F(EprotoxTest, NestedNodes) {
    Repository repo;
    auto frame = std::make_unique<Frame>(10, "NestedFrame", "Frame with nested nodes",
                                         FrameType::data, ByteOrder::little_endian);

    // Parent node
    auto parent = std::make_unique<Node>("Parent", "Parent node", 0, 0, 32,
                                         ValueType::longword, Tag::none, NodeAttrs{});

    // Child 1
    auto child1 = std::make_unique<Node>("Child1", "First child", 0, 0, 8,
                                         ValueType::byte_, Tag::head, NodeAttrs{});
    // Grandchild of Child1
    auto grandchild1 = std::make_unique<Node>("GrandChild1", "Grandchild", 0, 0, 4,
                                              ValueType::byte_, Tag::none, NodeAttrs{});
    child1->add_child(std::move(grandchild1));

    // Child 2
    auto child2 = std::make_unique<Node>("Child2", "Second child", 0, 8, 16,
                                         ValueType::word, Tag::length, NodeAttrs{});

    parent->add_child(std::move(child1));
    parent->add_child(std::move(child2));
    frame->add_root(std::move(parent));
    repo.add_frame(std::move(frame));

    auto path = tempDir / "nested.eprotox";
    auto write_result = serialize_xml_repository(path, repo);
    ASSERT_TRUE(write_result.has_value()) << write_result.error().message;

    auto read_result = deserialize_xml_repository(path);
    ASSERT_TRUE(read_result.has_value()) << read_result.error().message;

    ASSERT_EQ(read_result->frames().size(), 1u);
    const auto& read_frame = *read_result->frames()[0];
    ASSERT_EQ(read_frame.roots().size(), 1u);

    const auto& read_parent = *read_frame.roots()[0];
    EXPECT_EQ(read_parent.name(), "Parent");
    EXPECT_EQ(read_parent.children().size(), 2u);

    const auto& read_child1 = *read_parent.children()[0];
    EXPECT_EQ(read_child1.name(), "Child1");
    EXPECT_EQ(read_child1.tag(), Tag::head);
    EXPECT_EQ(read_child1.children().size(), 1u);

    const auto& read_grandchild = *read_child1.children()[0];
    EXPECT_EQ(read_grandchild.name(), "GrandChild1");
    EXPECT_EQ(read_grandchild.bit_width(), 4);

    const auto& read_child2 = *read_parent.children()[1];
    EXPECT_EQ(read_child2.name(), "Child2");
    EXPECT_EQ(read_child2.bit_offset(), 8);
    EXPECT_EQ(read_child2.bit_width(), 16);
    EXPECT_EQ(read_child2.tag(), Tag::length);
}

// ── Test 5: All ValueTypes ─────────────────────────────────────
TEST_F(EprotoxTest, AllValueTypes) {
    Repository repo;
    auto frame = std::make_unique<Frame>(99, "AllTypes", "All value types",
                                         FrameType::data, ByteOrder::little_endian);

    struct TypeEntry {
        std::string name;
        ValueType type;
    };

    TypeEntry types[] = {
        {"TypeBoolean", ValueType::boolean},
        {"TypeUint8",   ValueType::byte_},
        {"TypeBytes",   ValueType::bytes},
        {"TypeUint16",  ValueType::word},
        {"TypeInt16",   ValueType::shortint},
        {"TypeSmallint", ValueType::smallint},
        {"TypeUint32",  ValueType::longword},
        {"TypeInt32",   ValueType::integer},
        {"TypeUint64",  ValueType::ulong_},
        {"TypeFloat",   ValueType::single},
        {"TypeDouble",  ValueType::double_},
        {"TypeString",  ValueType::string_},
        {"TypeUnknown", ValueType::unknown},
    };

    int i = 0;
    for (const auto& entry : types) {
        frame->add_root(make_node(entry.name, i, 0, 8, entry.type));
        ++i;
    }
    repo.add_frame(std::move(frame));

    auto path = tempDir / "all_types.eprotox";
    auto write_result = serialize_xml_repository(path, repo);
    ASSERT_TRUE(write_result.has_value()) << write_result.error().message;

    auto read_result = deserialize_xml_repository(path);
    ASSERT_TRUE(read_result.has_value()) << read_result.error().message;

    ASSERT_EQ(read_result->frames().size(), 1u);
    ASSERT_EQ(read_result->frames()[0]->roots().size(), 13u);

    for (size_t j = 0; j < 13; ++j) {
        EXPECT_EQ(read_result->frames()[0]->roots()[j]->value_type(), types[j].type)
            << "Mismatch for type at index " << j << " (" << types[j].name << ")";
    }
}

// ── Test 6: CJK Path ───────────────────────────────────────────
TEST_F(EprotoxTest, CjkPath) {
    Repository repo;
    auto frame = std::make_unique<Frame>(1, "CJKFrame", "CJK test",
                                         FrameType::data, ByteOrder::little_endian);
    frame->add_root(make_node("Node1", 0, 0, 8));
    repo.add_frame(std::move(frame));

    auto path = tempDir / "测试文件.eprotox";

    auto write_result = serialize_xml_repository(path, repo);
    ASSERT_TRUE(write_result.has_value()) << write_result.error().message;
    ASSERT_TRUE(fs::exists(path));

    auto read_result = deserialize_xml_repository(path);
    ASSERT_TRUE(read_result.has_value()) << read_result.error().message;

    ASSERT_EQ(read_result->frames().size(), 1u);
    EXPECT_EQ(read_result->frames()[0]->name(), "CJKFrame");
}

// ── Test 7: IsScaled Parsing (true/false + 0/1) ────────────────
TEST_F(EprotoxTest, IsScaledParsing) {
    // Manually create an .eprotox content with various IsScaled formats
    auto path = tempDir / "scaled_test.eprotox";

    {
        std::ofstream stream(path);
        ASSERT_TRUE(stream.is_open());
        stream << R"(<?xml version="1.0" encoding="UTF-8"?>)"
               << "\n<ICDProtocol version=\"1.0\">\n"
               << "  <Frame>\n"
               << "    <ID>1</ID>\n"
               << "    <Name>ScaledTest</Name>\n"
               << "    <Description>Test</Description>\n"
               << "    <Type>data</Type>\n"
               << "    <ByteOrder>littleEndian</ByteOrder>\n"
               << "    <Nodes>\n"
               << "      <Item>\n"
               << "        <Name>ScaledTrue</Name>\n"
               << "        <Description>Scaled true</Description>\n"
               << "        <Offset>0</Offset>\n"
               << "        <StartBit>0</StartBit>\n"
               << "        <BitWidth>8</BitWidth>\n"
               << "        <ValueType>uint8</ValueType>\n"
               << "        <Tag>0</Tag>\n"
               << "        <Attrs>\n"
               << "          <IsScaled>true</IsScaled>\n"
               << "        </Attrs>\n"
               << "      </Item>\n"
               << "      <Item>\n"
               << "        <Name>ScaledFalse</Name>\n"
               << "        <Description>Scaled false</Description>\n"
               << "        <Offset>1</Offset>\n"
               << "        <StartBit>0</StartBit>\n"
               << "        <BitWidth>8</BitWidth>\n"
               << "        <ValueType>uint8</ValueType>\n"
               << "        <Tag>0</Tag>\n"
               << "        <Attrs>\n"
               << "          <IsScaled>false</IsScaled>\n"
               << "        </Attrs>\n"
               << "      </Item>\n"
               << "      <Item>\n"
               << "        <Name>ScaledOne</Name>\n"
               << "        <Description>Scaled 1</Description>\n"
               << "        <Offset>2</Offset>\n"
               << "        <StartBit>0</StartBit>\n"
               << "        <BitWidth>8</BitWidth>\n"
               << "        <ValueType>uint8</ValueType>\n"
               << "        <Tag>0</Tag>\n"
               << "        <Attrs>\n"
               << "          <IsScaled>1</IsScaled>\n"
               << "        </Attrs>\n"
               << "      </Item>\n"
               << "      <Item>\n"
               << "        <Name>ScaledZero</Name>\n"
               << "        <Description>Scaled 0</Description>\n"
               << "        <Offset>3</Offset>\n"
               << "        <StartBit>0</StartBit>\n"
               << "        <BitWidth>8</BitWidth>\n"
               << "        <ValueType>uint8</ValueType>\n"
               << "        <Tag>0</Tag>\n"
               << "        <Attrs>\n"
               << "          <IsScaled>0</IsScaled>\n"
               << "        </Attrs>\n"
               << "      </Item>\n"
               << "    </Nodes>\n"
               << "  </Frame>\n"
               << "</ICDProtocol>\n";
    }

    auto read_result = deserialize_xml_repository(path);
    ASSERT_TRUE(read_result.has_value()) << read_result.error().message;

    ASSERT_EQ(read_result->frames().size(), 1u);
    ASSERT_EQ(read_result->frames()[0]->roots().size(), 4u);

    EXPECT_TRUE(read_result->frames()[0]->roots()[0]->attrs().is_scaled)   << "true should be scaled";
    EXPECT_FALSE(read_result->frames()[0]->roots()[1]->attrs().is_scaled)  << "false should not be scaled";
    EXPECT_TRUE(read_result->frames()[0]->roots()[2]->attrs().is_scaled)   << "1 should be scaled";
    EXPECT_FALSE(read_result->frames()[0]->roots()[3]->attrs().is_scaled)  << "0 should not be scaled";
}

// ── Test 8: Cross-Format Round-Trip ─────────────────────────────
TEST_F(EprotoxTest, CrossFormatRoundTrip) {
    // Build a repo manually
    Repository repo;
    auto frame = std::make_unique<Frame>(5, "CrossFrame", "Cross-format frame",
                                         FrameType::cmd, ByteOrder::big_endian);

    auto node = std::make_unique<Node>("Speed", "Vehicle speed", 0, 0, 16,
                                       ValueType::word, Tag::none, NodeAttrs{});
    frame->add_root(std::move(node));
    repo.add_frame(std::move(frame));

    // Write as .eprotox
    auto eprotox_path = tempDir / "cross.eprotox";
    auto write_eprotox = serialize_xml_repository(eprotox_path, repo);
    ASSERT_TRUE(write_eprotox.has_value()) << write_eprotox.error().message;

    // Read back from .eprotox
    auto from_eprotox = deserialize_xml_repository(eprotox_path);
    ASSERT_TRUE(from_eprotox.has_value()) << from_eprotox.error().message;

    // Write as .eproto JSON
    auto json_path = tempDir / "cross.eproto";
    auto write_json = serialize_repository(json_path, *from_eprotox);
    ASSERT_TRUE(write_json.has_value()) << write_json.error().message;

    // Read back from .eproto JSON
    auto from_json = deserialize_repository(json_path);
    ASSERT_TRUE(from_json.has_value()) << from_json.error().message;

    // Compare contents
    ASSERT_EQ(from_json->frames().size(), 1u);
    const auto& json_frame = *from_json->frames()[0];
    EXPECT_EQ(json_frame.id(), 5);
    EXPECT_EQ(json_frame.name(), "CrossFrame");
    EXPECT_EQ(json_frame.type(), FrameType::cmd);
    EXPECT_EQ(json_frame.order(), ByteOrder::big_endian);

    ASSERT_EQ(json_frame.roots().size(), 1u);
    EXPECT_EQ(json_frame.roots()[0]->name(), "Speed");
    EXPECT_EQ(json_frame.roots()[0]->bit_width(), 16);
    EXPECT_EQ(json_frame.roots()[0]->value_type(), ValueType::word);
}

// ── Test 9: Malformed XML ──────────────────────────────────────
TEST_F(EprotoxTest, MalformedXml) {
    auto path = tempDir / "malformed.eprotox";
    {
        std::ofstream stream(path);
        ASSERT_TRUE(stream.is_open());
        stream << "this is not xml <<<>>>";
    }

    auto result = deserialize_xml_repository(path);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::parse_error);
}

// ── Test 10: Invalid Root Element ──────────────────────────────
TEST_F(EprotoxTest, InvalidRootElement) {
    auto path = tempDir / "bad_root.eprotox";
    {
        std::ofstream stream(path);
        ASSERT_TRUE(stream.is_open());
        stream << R"(<?xml version="1.0" encoding="UTF-8"?>)"
               << "\n<WrongRoot version=\"1.0\">\n"
               << "  <Frame><ID>1</ID><Name>X</Name><Description>X</Description></Frame>\n"
               << "</WrongRoot>\n";
    }

    auto result = deserialize_xml_repository(path);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::schema_error);
    EXPECT_EQ(result.error().path_hint, "ICDProtocol");
}
