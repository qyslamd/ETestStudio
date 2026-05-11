#include <gtest/gtest.h>
#include <icd/error.hpp>
#include "../src/format/json_parser.hpp"
#include "../src/format/xml_parser.hpp"
#include <filesystem>

#ifndef ICD_TEST_JSON_DIR
#error ICD_TEST_JSON_DIR is not defined
#endif
#ifndef ICD_TEST_XML_DIR
#error ICD_TEST_XML_DIR is not defined
#endif

namespace fs = std::filesystem;

struct JsonParserTest : ::testing::Test {
    fs::path jsonBase = fs::path(ICD_TEST_JSON_DIR);
    fs::path xmlBase = fs::path(ICD_TEST_XML_DIR);
};

TEST_F(JsonParserTest, ConfigParse) {
    auto config = icd::format::parse_json_config(jsonBase / "config-valid.json");
    ASSERT_TRUE(config.has_value()) << "error code=" << static_cast<int>(config.error().code);
    ASSERT_EQ(config->files.size(), 1u);
    EXPECT_EQ(config->files.front().logical_name, "FrameA");
}

TEST_F(JsonParserTest, FrameParseCrossCheck) {
    auto frame = icd::format::parse_json_frame(jsonBase / "frame-valid.json");
    auto xml = icd::format::parse_xml_frame(xmlBase / "frame-valid.xml");
    ASSERT_TRUE(frame.has_value());
    ASSERT_TRUE(xml.has_value());
    EXPECT_EQ(frame->name, "FrameA");
    EXPECT_EQ(frame->roots.size(), 1u);
    EXPECT_EQ(frame->roots.front().children.size(), 1u);
    EXPECT_EQ(frame->roots.front().name, xml->roots.front().name);
}

TEST_F(JsonParserTest, MalformedJson) {
    auto result = icd::format::parse_json_frame(jsonBase / "frame-malformed.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::parse_error);
    EXPECT_EQ(result.error().file.filename(), "frame-malformed.json");
}

TEST_F(JsonParserTest, MissingFieldJson) {
    auto result = icd::format::parse_json_frame(jsonBase / "frame-missing-field.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::schema_error);
    EXPECT_EQ(result.error().file.filename(), "frame-missing-field.json");
    EXPECT_FALSE(result.error().path_hint.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
