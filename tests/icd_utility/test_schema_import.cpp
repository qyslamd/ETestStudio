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

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// 全局 schema 目录路径（由 main() 从 --schema_dir 或 SCHEMA_DIR 设置）
std::string g_schema_dir;

// ── 测试夹具：批量导入 Schema XML → .eproto ──
//
// 用法：
//   test_schema_import --schema_dir=D:\path\to\schema
//   或设置环境变量 SCHEMA_DIR
struct SchemaImportTest : ::testing::Test {
  static fs::path tmpDir;

  static void SetUpTestSuite() {
    if (!g_schema_dir.empty()) {
      tmpDir = fs::temp_directory_path() / "etest_schema_import";
      fs::remove_all(tmpDir);
      fs::create_directories(tmpDir);
    }
  }

  static void TearDownTestSuite() {
    if (!tmpDir.empty()) {
      fs::remove_all(tmpDir);
    }
  }
};

fs::path SchemaImportTest::tmpDir;

// ── 读取文件头部并检测 XML 类型 ──
enum class XmlType { kConfig, kFrame, kUnknown };
static XmlType detectXmlType(const fs::path& path) {
  std::ifstream stream(path);
  if (!stream.is_open()) return XmlType::kUnknown;
  std::string head(4096, '\0');
  stream.read(head.data(), 4096);
  stream.close();

  if (head.find("<ICDConfig") != std::string::npos) return XmlType::kConfig;
  if (head.find("<ICDData") != std::string::npos)   return XmlType::kFrame;
  return XmlType::kUnknown;
}

// ── 运行导入管线：XML → .eproto ──
// 返回空字符串表示成功，否则返回错误消息
static std::string runImportPipeline(const fs::path& xmlPath, const fs::path& outPath) {
  auto type = detectXmlType(xmlPath);
  if (type == XmlType::kUnknown)
    return "unknown XML type (neither <ICDConfig nor <ICDData)";

  tl::expected<icd::Repository, icd::Error> result;

  if (type == XmlType::kConfig) {
    result = icd::Loader::init(xmlPath);
    if (!result)
      return "Loader::init failed: " + result.error().message;
  } else {
    auto frameResult = icd::format::parse_xml_frame(xmlPath);
    if (!frameResult)
      return "parse_xml_frame failed: " + frameResult.error().message;
    icd::schema::SchemaConfig config;
    config.frames.push_back(std::move(*frameResult));
    result = icd::schema::build_repository(config);
    if (!result)
      return "build_repository failed: " + result.error().message;
  }

  auto sr = icd::format::serialize_repository(outPath, *result);
  if (!sr)
    return "serialize_repository failed: " + sr.error().message;

  // 验证 .eproto 是合法 JSON
  std::ifstream jsonStream(outPath);
  if (!jsonStream.is_open())
    return "cannot open output .eproto for verification";
  try {
    nlohmann::json doc;
    jsonStream >> doc;
    if (doc["version"] != "1.0")
      return "bad version in .eproto: " + doc["version"].get<std::string>();
    if (!doc["frames"].is_array() || doc["frames"].empty())
      return ".eproto has no frames array";
  } catch (const std::exception& e) {
    return "invalid .eproto JSON: " + std::string(e.what());
  }

  return {};  // 成功
}

TEST_F(SchemaImportTest, ImportAllXmls) {
  if (g_schema_dir.empty()) GTEST_SKIP() << "未设置 SCHEMA_DIR 或 --schema_dir，跳过批量导入测试";

  fs::path schemaDir(g_schema_dir);
  ASSERT_TRUE(fs::exists(schemaDir)) << "目录不存在: " << g_schema_dir;

  // 扫描 XML 文件
  std::vector<fs::path> xmlFiles;
  for (const auto& entry : fs::directory_iterator(schemaDir)) {
    if (entry.path().extension() == ".xml")
      xmlFiles.push_back(entry.path());
  }
  ASSERT_FALSE(xmlFiles.empty()) << "目录中没有 XML 文件: " << g_schema_dir;

  // 执行批量导入
  int total       = 0;
  int configCount = 0;
  int frameCount  = 0;
  int skipped     = 0;
  std::vector<std::string> failures;

  for (const auto& xmlPath : xmlFiles) {
    SCOPED_TRACE(xmlPath.string());
    ++total;

    auto type = detectXmlType(xmlPath);
    if (type == XmlType::kUnknown) { ++skipped; continue; }
    if (type == XmlType::kConfig) ++configCount;
    else ++frameCount;

    fs::path outPath = tmpDir / (xmlPath.stem().string() + ".eproto");
    auto err = runImportPipeline(xmlPath, outPath);
    if (!err.empty()) {
      failures.push_back(xmlPath.filename().string() + ": " + err);
    }
  }

  // 输出汇总
  std::cout << "\n=== Schema Import Summary ==="
            << "\n  Total XML : " << total
            << "\n  ICDConfig : " << configCount
            << "\n  ICDData   : " << frameCount
            << "\n  Skipped   : " << skipped
            << "\n  Failed    : " << failures.size()
            << "\n  Passed    : " << (total - skipped - static_cast<int>(failures.size()))
            << "\n============================\n"
            << std::endl;

  for (const auto& f : failures) {
    std::cout << "  FAIL: " << f << std::endl;
  }

  EXPECT_TRUE(failures.empty()) << failures.size() << " 个文件导入失败";
}

// ── 去除首尾空白 ──
static std::string trim(std::string s) {
  const auto* ws = " \t\r\n";
  s.erase(0, s.find_first_not_of(ws));
  s.erase(s.find_last_not_of(ws) + 1);
  return s;
}

int main(int argc, char** argv) {
  // 在 InitGoogleTest 之前提取自定义参数 --schema_dir
  std::vector<char*> gtest_argv;
  gtest_argv.push_back(argv[0]);
  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], "--schema_dir=", 13) == 0) {
      g_schema_dir = trim(argv[i] + 13);
    } else {
      gtest_argv.push_back(argv[i]);
    }
  }
  int gtest_argc = static_cast<int>(gtest_argv.size());
  ::testing::InitGoogleTest(&gtest_argc, gtest_argv.data());

  // 备选：环境变量（可能带尾部空格，需要 trim）
  if (g_schema_dir.empty()) {
    const char* env = std::getenv("SCHEMA_DIR");
    if (env) g_schema_dir = trim(env);
  }

  return RUN_ALL_TESTS();
}
