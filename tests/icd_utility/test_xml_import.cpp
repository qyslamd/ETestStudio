#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <icd/error.hpp>
#include <icd/loader.hpp>
#include <icd/repository.hpp>
#include <icd/types.hpp>
#include "../src/format/json_serializer.hpp"
#include "../src/format/xml_parser.hpp"
#include "../src/schema/builder.hpp"
#include "../src/schema/schema.hpp"

#include <filesystem>
#include <fstream>
#include <cstdio>

#ifndef ICD_TEST_XML_DIR
#error ICD_TEST_XML_DIR is not defined
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

// ── 测试夹具：XML → .eproto 导入管线 ──
//
// 验证 ProtocolManagerWidget::onImportXml() 内部使用的 icd_utility 管线：
//   单帧(XML ICDData)：parse_xml_frame → build_repository → serialize_repository
//   多帧配置(XML ICDConfig)：Loader::init → serialize_repository
struct XmlImportTest : ::testing::Test {
  fs::path xmlBase = fs::path(ICD_TEST_XML_DIR);
  fs::path tmpDir  = fs::temp_directory_path() / "etest_xml_import_test";

  void SetUp() override { fs::create_directories(tmpDir); }
  void TearDown() override { fs::remove_all(tmpDir); }

  fs::path tempOutput(const char* name) const { return tmpDir / name; }
};

// ── 单帧 ICDData XML 导入 ──
TEST_F(XmlImportTest, SingleFramePipeline) {
  // 解析 ICDData XML → SchemaFrameDef
  auto frameResult = icd::format::parse_xml_frame(xmlBase / "frame-simple.xml");
  ASSERT_TRUE(frameResult.has_value()) << frameResult.error().message;

  // 包裹为 SchemaConfig → Repository
  icd::schema::SchemaConfig config;
  config.frames.push_back(std::move(*frameResult));
  auto repo = icd::schema::build_repository(config);
  ASSERT_TRUE(repo.has_value()) << repo.error().message;

  // 序列化为 .eproto
  fs::path outPath = tempOutput("frame-simple.eproto");
  auto sr = icd::format::serialize_repository(outPath, *repo);
  ASSERT_TRUE(sr.has_value()) << sr.error().message;
  ASSERT_TRUE(fs::exists(outPath));

  // ── 验证 .eproto JSON 结构 ──
  std::ifstream stream(outPath);
  ASSERT_TRUE(stream.is_open());
  json doc;
  stream >> doc;

  EXPECT_EQ(doc["version"], "1.0");
  ASSERT_TRUE(doc["frames"].is_array());
  ASSERT_EQ(doc["frames"].size(), 1);

  auto frame = doc["frames"][0];
  EXPECT_EQ(frame["id"], 0);
  EXPECT_EQ(frame["name"], "SimpleFrame");
  EXPECT_EQ(frame["description"], "");
  EXPECT_EQ(frame["type"], "data");
  EXPECT_EQ(frame["byteOrder"], "littleEndian");
  // Field1=8bit@0, Field2=16bit@1, Field3=32bit@3, Field4=16bit@7
  // 最大跨度 = max(8, 24, 56, 72) = 72bit → 9 字节
  EXPECT_EQ(frame["length"], 9);

  // 节点结构
  ASSERT_TRUE(frame["nodes"].is_array());
  ASSERT_EQ(frame["nodes"].size(), 4);

  EXPECT_EQ(frame["nodes"][0]["name"], "Field1");
  EXPECT_EQ(frame["nodes"][0]["description"], "First field");
  EXPECT_EQ(frame["nodes"][0]["offset"], 0);
  EXPECT_EQ(frame["nodes"][0]["startBit"], 0);
  EXPECT_EQ(frame["nodes"][0]["bitWidth"], 8);
  EXPECT_EQ(frame["nodes"][0]["valueType"], "uint8");

  EXPECT_EQ(frame["nodes"][1]["name"], "Field2");
  EXPECT_EQ(frame["nodes"][1]["description"], "Second field");
  EXPECT_EQ(frame["nodes"][1]["offset"], 1);
  EXPECT_EQ(frame["nodes"][1]["startBit"], 0);
  EXPECT_EQ(frame["nodes"][1]["bitWidth"], 16);
  EXPECT_EQ(frame["nodes"][1]["valueType"], "uint16");

  // dword → uint32
  EXPECT_EQ(frame["nodes"][2]["name"], "Field3");
  EXPECT_EQ(frame["nodes"][2]["description"], "Third field (dword)");
  EXPECT_EQ(frame["nodes"][2]["offset"], 3);
  EXPECT_EQ(frame["nodes"][2]["bitWidth"], 32);
  EXPECT_EQ(frame["nodes"][2]["valueType"], "uint32");

  // short → int16
  EXPECT_EQ(frame["nodes"][3]["name"], "Field4");
  EXPECT_EQ(frame["nodes"][3]["description"], "Fourth field (short)");
  EXPECT_EQ(frame["nodes"][3]["offset"], 7);
  EXPECT_EQ(frame["nodes"][3]["bitWidth"], 16);
  EXPECT_EQ(frame["nodes"][3]["valueType"], "int16");
}

// ── ICDConfig 多帧配置导入 ──
TEST_F(XmlImportTest, ConfigPipeline) {
  // Loader::init 自动解析 ICDConfig 并引用 frame-simple.xml
  auto repo = icd::Loader::init(xmlBase / "config-simple.xml");
  ASSERT_TRUE(repo.has_value()) << repo.error().message;

  // 验证帧可访问，配置中覆盖了 id=100
  const auto* frame = repo->find("SimpleFrame");
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->id(), 100);
  EXPECT_EQ(frame->type(), icd::FrameType::data);

  // 序列化为 .eproto
  fs::path outPath = tempOutput("config-simple.eproto");
  auto sr = icd::format::serialize_repository(outPath, *repo);
  ASSERT_TRUE(sr.has_value()) << sr.error().message;

  // ── 验证 .eproto JSON 结构 ──
  std::ifstream stream(outPath);
  ASSERT_TRUE(stream.is_open());
  json doc;
  stream >> doc;

  EXPECT_EQ(doc["version"], "1.0");
  ASSERT_TRUE(doc["frames"].is_array());
  ASSERT_EQ(doc["frames"].size(), 1);

  auto f = doc["frames"][0];
  EXPECT_EQ(f["id"], 100);           // 配置中指定的 ID
  EXPECT_EQ(f["name"], "SimpleFrame");
  EXPECT_EQ(f["type"], "data");
  EXPECT_EQ(f["length"], 9);
  ASSERT_TRUE(f["nodes"].is_array());
  EXPECT_EQ(f["nodes"].size(), 4);
}

// ── 无效 XML → 错误处理 ──
TEST_F(XmlImportTest, SingleFrameWithInvalidXml) {
  auto result = icd::format::parse_xml_frame(xmlBase / "frame-malformed.xml");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, icd::ErrorCode::parse_error);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
