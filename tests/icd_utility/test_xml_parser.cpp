#include <gtest/gtest.h>
#include <icd/error.hpp>
#include "../src/format/xml_parser.hpp"
#include "../src/format/json_parser.hpp"
#include <filesystem>

#ifndef ICD_TEST_XML_DIR
#error ICD_TEST_XML_DIR is not defined
#endif
#ifndef ICD_TEST_JSON_DIR
#error ICD_TEST_JSON_DIR is not defined
#endif

namespace fs = std::filesystem;

struct XmlParserTest : ::testing::Test {
    fs::path xmlBase = fs::path(ICD_TEST_XML_DIR);
    fs::path jsonBase = fs::path(ICD_TEST_JSON_DIR);
};

TEST_F(XmlParserTest, CrossFormatConsistency) {
    auto xml = icd::format::parse_xml_frame(xmlBase / "frame-valid.xml");
    auto json = icd::format::parse_json_frame(jsonBase / "frame-valid.json");
    ASSERT_TRUE(xml.has_value());
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ(xml->name, json->name);
    EXPECT_EQ(xml->roots.size(), json->roots.size());
    EXPECT_EQ(xml->roots.front().name, json->roots.front().name);
    EXPECT_EQ(xml->roots.front().offset, json->roots.front().offset);
    EXPECT_EQ(xml->roots.front().children.size(), json->roots.front().children.size());
    EXPECT_EQ(xml->roots.front().children.front().name, json->roots.front().children.front().name);
    EXPECT_EQ(xml->roots.front().attrs.system_name, json->roots.front().attrs.system_name);
    EXPECT_EQ(xml->roots.front().attrs.unit, json->roots.front().attrs.unit);
}

TEST_F(XmlParserTest, MalformedXml) {
    auto result = icd::format::parse_xml_frame(xmlBase / "frame-malformed.xml");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::parse_error);
    EXPECT_EQ(result.error().file.filename(), "frame-malformed.xml");
}

TEST_F(XmlParserTest, MissingFieldXml) {
    auto result = icd::format::parse_xml_frame(xmlBase / "frame-missing-field.xml");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::schema_error);
    EXPECT_EQ(result.error().file.filename(), "frame-missing-field.xml");
    EXPECT_FALSE(result.error().path_hint.empty());
}

TEST_F(XmlParserTest, MalformedJson) {
    auto result = icd::format::parse_json_frame(jsonBase / "frame-malformed.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::parse_error);
    EXPECT_EQ(result.error().file.filename(), "frame-malformed.json");
}

TEST_F(XmlParserTest, MissingFieldJson) {
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
