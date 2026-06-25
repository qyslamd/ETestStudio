#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/file_entry.hpp>
#include <icd/frame.hpp>
#include <icd/loader.hpp>
#include <icd/node.hpp>
#include <icd/repository.hpp>
#include <icd/types.hpp>

#include "../src/format/type_mapping.hpp"
#include "../src/format/xml_parser.hpp"
#include "../src/format/xml_serializer.hpp"
#include "../src/format/json_parser.hpp"
#include "../src/format/json_serializer.hpp"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace icd;
using namespace icd::format;

struct ConfigWritebackTest : ::testing::Test {
    fs::path tempDir = fs::temp_directory_path() / "config_writeback_test";

    void SetUp() override {
        fs::create_directories(tempDir);
    }

    void TearDown() override {
        fs::remove_all(tempDir);
    }

    // Helper: create a simple node
    static std::unique_ptr<Node> make_node(const std::string& name,
                                           int offset,
                                           int bit_offset,
                                           int bit_width,
                                           ValueType value_type = ValueType::byte_,
                                           Tag tag = Tag::none) {
        return std::make_unique<Node>(
            name, std::string{}, offset, bit_offset, bit_width,
            value_type, tag, NodeAttrs{});
    }

    // Helper: write a minimal ICDConfig.xml with one frame file
    void writeXmlConfig(const fs::path& config_path,
                        const std::string& frame_filename) {
        std::ofstream stream(config_path);
        ASSERT_TRUE(stream.is_open());
        stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               << "<ICDConfig>\n"
               << "  <Files>\n"
               << "    <FileInfo>\n"
               << "      <Name>TestFrame</Name>\n"
               << "      <Path>" << frame_filename << "</Path>\n"
               << "      <ID>1</ID>\n"
               << "      <Type>2</Type>\n"
               << "      <ByteOrder>1</ByteOrder>\n"
               << "    </FileInfo>\n"
               << "  </Files>\n"
               << "</ICDConfig>\n";
    }

    // Helper: write a minimal frame XML file
    void writeXmlFrame(const fs::path& frame_path) {
        std::ofstream stream(frame_path);
        ASSERT_TRUE(stream.is_open());
        stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               << "<ICDData>\n"
               << "  <Name>TestFrame</Name>\n"
               << "  <Data>\n"
               << "    <Item>\n"
               << "      <Name>Speed</Name>\n"
               << "      <Description>Speed value</Description>\n"
               << "      <Offset>0</Offset>\n"
               << "      <StartBit>0</StartBit>\n"
               << "      <BitWidth>16</BitWidth>\n"
               << "      <Type>word</Type>\n"
               << "      <Tag>" << tag_to_legacy_int(Tag::head) << "</Tag>\n"
               << "      <IsScaled>0</IsScaled>\n"
               << "    </Item>\n"
               << "  </Data>\n"
               << "</ICDData>\n";
    }
};

// ── Test 1: Empty XML Config ────────────────────────────────────
TEST_F(ConfigWritebackTest, EmptyXmlConfig) {
    auto config_path = tempDir / "empty.xml";
    {
        std::ofstream stream(config_path);
        stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               << "<ICDConfig>\n"
               << "  <Files>\n"
               << "  </Files>\n"
               << "</ICDConfig>\n";
    }

    auto config = parse_xml_config(config_path);
    ASSERT_TRUE(config.has_value()) << config.error().message;
    EXPECT_TRUE(config->files.empty());

    auto result = Loader::init_with_metadata(config_path);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->repository.frames().size(), 0u);
    EXPECT_TRUE(result->file_entries.empty());
}

// ── Test 2: Empty JSON Config ───────────────────────────────────
TEST_F(ConfigWritebackTest, EmptyJsonConfig) {
    auto config_path = tempDir / "empty.json";
    {
        std::ofstream stream(config_path);
        stream << "{\"files\": []}\n";
    }

    auto config = parse_json_config(config_path);
    ASSERT_TRUE(config.has_value()) << config.error().message;
    EXPECT_TRUE(config->files.empty());

    auto result = Loader::init_with_metadata(config_path);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result->repository.frames().size(), 0u);
    EXPECT_TRUE(result->file_entries.empty());
}

// ── Test 3: Load Full XML Config ─────────────────────────────────
TEST_F(ConfigWritebackTest, LoadXmlConfig) {
    auto config_path = tempDir / "ICDConfig.xml";
    auto frame_path = tempDir / "frame_001.xml";
    writeXmlFrame(frame_path);
    writeXmlConfig(config_path, "frame_001.xml");

    auto result = Loader::init_with_metadata(config_path);
    ASSERT_TRUE(result.has_value()) << result.error().message;

    // Verify LoadResult
    EXPECT_EQ(result->config_path, config_path);
    EXPECT_EQ(result->format, Format::xml);
    ASSERT_EQ(result->file_entries.size(), 1u);
    EXPECT_EQ(result->file_entries[0].id, 1);
    EXPECT_EQ(result->file_entries[0].name, "TestFrame");
    EXPECT_EQ(result->file_entries[0].path, "frame_001.xml");
    EXPECT_EQ(result->file_entries[0].type, FrameType::cmd);
    EXPECT_EQ(result->file_entries[0].order, ByteOrder::big_endian);

    // Verify Repository
    ASSERT_EQ(result->repository.frames().size(), 1u);
    EXPECT_EQ(result->repository.frames()[0]->id(), 1);
    EXPECT_EQ(result->repository.frames()[0]->name(), "TestFrame");
    EXPECT_EQ(result->repository.frames()[0]->type(), FrameType::cmd);
    EXPECT_EQ(result->repository.frames()[0]->order(), ByteOrder::big_endian);
}

// ── Test 4: Tag Bug Fix ─────────────────────────────────────────
TEST_F(ConfigWritebackTest, TagBugFix) {
    // Write XML with legacy Tag values 40, 41, 60
    auto xml_path = tempDir / "tag_test.xml";
    {
        std::ofstream stream(xml_path);
        stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               << "<ICDData>\n"
               << "  <Name>TagTest</Name>\n"
               << "  <Data>\n"
               << "    <Item>\n"
               << "      <Name>SumField</Name>\n"
               << "      <Description>Checksum</Description>\n"
               << "      <Offset>0</Offset>\n"
               << "      <StartBit>0</StartBit>\n"
               << "      <BitWidth>16</BitWidth>\n"
               << "      <Type>word</Type>\n"
               << "      <Tag>40</Tag>\n"
               << "      <IsScaled>0</IsScaled>\n"
               << "    </Item>\n"
               << "    <Item>\n"
               << "      <Name>SigField</Name>\n"
               << "      <Description>Signal in value</Description>\n"
               << "      <Offset>2</Offset>\n"
               << "      <StartBit>0</StartBit>\n"
               << "      <BitWidth>8</BitWidth>\n"
               << "      <Type>byte</Type>\n"
               << "      <Tag>41</Tag>\n"
               << "      <IsScaled>0</IsScaled>\n"
               << "    </Item>\n"
               << "    <Item>\n"
               << "      <Name>BigEndField</Name>\n"
               << "      <Description>Big endian value</Description>\n"
               << "      <Offset>3</Offset>\n"
               << "      <StartBit>0</StartBit>\n"
               << "      <BitWidth>32</BitWidth>\n"
               << "      <Type>dword</Type>\n"
               << "      <Tag>60</Tag>\n"
               << "      <IsScaled>0</IsScaled>\n"
               << "    </Item>\n"
               << "  </Data>\n"
               << "</ICDData>\n";
    }

    auto frame = parse_xml_frame(xml_path);
    ASSERT_TRUE(frame.has_value()) << frame.error().message;

    ASSERT_EQ(frame->roots.size(), 3u);
    EXPECT_EQ(frame->roots[0].tag, Tag::sum);
    EXPECT_EQ(frame->roots[1].tag, Tag::signal_in_value);
    EXPECT_EQ(frame->roots[2].tag, Tag::big_endian_value);
}

// ── Test 5: Tag Bug Fix JSON ────────────────────────────────────
TEST_F(ConfigWritebackTest, TagBugFixJson) {
    auto json_path = tempDir / "tag_test.json";
    {
        std::ofstream stream(json_path);
        stream << R"({
            "name": "TagTest",
            "data": [
                {"name": "SumField", "description": "Checksum", "offset": 0, "startBit": 0, "bitWidth": 16, "type": "word", "tag": 40, "isScaled": 0},
                {"name": "SigField", "description": "Signal", "offset": 2, "startBit": 0, "bitWidth": 8, "type": "byte", "tag": 41, "isScaled": 0},
                {"name": "BigEndField", "description": "Big endian", "offset": 3, "startBit": 0, "bitWidth": 32, "type": "dword", "tag": 60, "isScaled": 0}
            ]
        })";
    }

    auto frame = parse_json_frame(json_path);
    ASSERT_TRUE(frame.has_value()) << frame.error().message;

    ASSERT_EQ(frame->roots.size(), 3u);
    EXPECT_EQ(frame->roots[0].tag, Tag::sum);
    EXPECT_EQ(frame->roots[1].tag, Tag::signal_in_value);
    EXPECT_EQ(frame->roots[2].tag, Tag::big_endian_value);
}

// ── Test 6: XML Config Round-Trip ───────────────────────────────
TEST_F(ConfigWritebackTest, XmlConfigRoundTrip) {
    // Load, write back, reload
    auto config_path = tempDir / "ICDConfig.xml";
    auto frame_path = tempDir / "frame_001.xml";
    writeXmlFrame(frame_path);
    writeXmlConfig(config_path, "frame_001.xml");

    // Load
    auto result = Loader::init_with_metadata(config_path);
    ASSERT_TRUE(result.has_value());

    // Write back
    auto write_result = serialize_xml_config(config_path, result->file_entries);
    ASSERT_TRUE(write_result.has_value()) << write_result.error().message;

    // Also write frame file
    ASSERT_EQ(result->repository.frames().size(), 1u);
    auto frame_write_result = serialize_xml_frame_file(frame_path, *result->repository.frames()[0]);
    ASSERT_TRUE(frame_write_result.has_value()) << frame_write_result.error().message;

    // Reload
    auto result2 = Loader::init_with_metadata(config_path);
    ASSERT_TRUE(result2.has_value()) << result2.error().message;

    ASSERT_EQ(result2->repository.frames().size(), 1u);
    EXPECT_EQ(result2->repository.frames()[0]->name(), "TestFrame");
    EXPECT_EQ(result2->repository.frames()[0]->id(), 1);

    auto& entry = result2->file_entries[0];
    EXPECT_EQ(entry.id, 1);
    EXPECT_EQ(entry.name, "TestFrame");
    EXPECT_EQ(entry.path, "frame_001.xml");
}

// ── Test 7: FrameFileInfo Default Values ────────────────────────
TEST_F(ConfigWritebackTest, FrameFileInfoDefaults) {
    FrameFileInfo info;
    EXPECT_EQ(info.id, 0);
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.description.empty());
    EXPECT_TRUE(info.path.empty());
    EXPECT_EQ(info.type, FrameType::data);
    EXPECT_EQ(info.order, ByteOrder::little_endian);
    EXPECT_EQ(info.format, Format::xml);
}
