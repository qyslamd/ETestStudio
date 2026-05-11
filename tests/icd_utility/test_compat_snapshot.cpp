#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/loader.hpp>
#include "../src/format/xml_parser.hpp"
#include "../src/schema/builder.hpp"
#include <filesystem>

#ifndef ICD_TEST_XML_COMPAT_DIR
#error ICD_TEST_XML_COMPAT_DIR is not defined
#endif

namespace fs = std::filesystem;

struct CompatSnapshotTest : ::testing::Test {
    fs::path base = fs::path(ICD_TEST_XML_COMPAT_DIR);
};

TEST_F(CompatSnapshotTest, LoadFullConfig) {
    auto repo = icd::Loader::init(base / "ICDConfig.xml", icd::Format::xml);
    ASSERT_TRUE(repo.has_value()) << repo.error().message;
    EXPECT_EQ(repo->frames().size(), 7u);
    EXPECT_NE(repo->find("FDR0"), nullptr);
    EXPECT_NE(repo->find("DA0_1"), nullptr);
    EXPECT_NE(repo->find("AD0"), nullptr);
}

TEST_F(CompatSnapshotTest, Fdr0FrameStructure) {
    auto frame = icd::format::parse_xml_frame(base / "fdr0.xml");
    ASSERT_TRUE(frame.has_value()) << frame.error().message;
    EXPECT_EQ(frame->roots.size(), 3u);
    EXPECT_EQ(frame->roots[0].name, "\xE5\xB8\xA7\xE5\xA4\xB4");
    EXPECT_EQ(frame->roots[1].name, "\xE7\x87\x83\xE6\xB2\xB9\xE9\x98\x80\xE9\x97\xA8" "1");
    EXPECT_EQ(frame->roots[2].attrs.unit, "\xE5\xBA\xA6");
}

TEST_F(CompatSnapshotTest, IoFrameStructure) {
    auto frame = icd::format::parse_xml_frame(base / "io.xml");
    ASSERT_TRUE(frame.has_value()) << frame.error().message;
    EXPECT_EQ(frame->roots.size(), 7u);
    EXPECT_EQ(frame->roots[4].name, "100mv1");
    EXPECT_EQ(frame->roots[6].children.size(), 2u);
    EXPECT_EQ(frame->roots[6].children[0].name, "\xE7\xA8\x8B\xE6\x8E\xA7\xE7\x94\xB5\xE6\xBA\x90" "4");
}

TEST_F(CompatSnapshotTest, Io2FrameStructure) {
    auto frame = icd::format::parse_xml_frame(base / "io2.xml");
    ASSERT_TRUE(frame.has_value()) << frame.error().message;
    EXPECT_EQ(frame->roots.size(), 3u);
    EXPECT_EQ(frame->roots[0].name, "K1");
    EXPECT_EQ(frame->roots[1].attrs.system_name, "\xE9\xA3\x9E\xE5\x8F\x82");
    EXPECT_EQ(frame->roots[2].description, "\xE7\x94\xB5\xE5\x8E\x8B" "1");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
