#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/loader.hpp>
#include <icd/types.hpp>
#include <filesystem>

#ifndef ICD_TEST_XML_DIR
#error ICD_TEST_XML_DIR is not defined
#endif
#ifndef ICD_TEST_JSON_DIR
#error ICD_TEST_JSON_DIR is not defined
#endif

namespace fs = std::filesystem;

struct LoaderTest : ::testing::Test {
    fs::path xmlBase = fs::path(ICD_TEST_XML_DIR);
    fs::path jsonBase = fs::path(ICD_TEST_JSON_DIR);
};

TEST_F(LoaderTest, XmlAutoDetect) {
    auto repo = icd::Loader::init(xmlBase / "config-valid.xml");
    ASSERT_TRUE(repo.has_value()) << repo.error().message;
    EXPECT_NE(repo->find("FrameA"), nullptr);
}

TEST_F(LoaderTest, JsonAutoDetect) {
    auto repo = icd::Loader::init(jsonBase / "config-valid.json");
    ASSERT_TRUE(repo.has_value()) << repo.error().message;
    EXPECT_NE(repo->find("FrameA"), nullptr);
}

TEST_F(LoaderTest, ExplicitXmlFormat) {
    auto repo = icd::Loader::init(xmlBase / "config-valid.xml", icd::Format::xml);
    ASSERT_TRUE(repo.has_value());
}

TEST_F(LoaderTest, UnsupportedFormat) {
    auto repo = icd::Loader::init(jsonBase / "config-valid.unsupported", icd::Format::auto_detect);
    ASSERT_FALSE(repo.has_value());
    EXPECT_EQ(repo.error().code, icd::ErrorCode::unsupported_format);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
