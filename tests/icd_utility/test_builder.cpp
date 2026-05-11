#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/frame.hpp>
#include <icd/repository.hpp>
#include <icd/types.hpp>
#include "../src/schema/builder.hpp"
#include "../src/schema/schema.hpp"

namespace {

icd::schema::SchemaNodeDef make_node(std::string name, int offset) {
    icd::schema::SchemaNodeDef node;
    node.name = std::move(name);
    node.description = "node";
    node.offset = offset;
    node.bit_offset = 0;
    node.bit_width = 8;
    node.value_type = icd::ValueType::byte_;
    node.tag = icd::Tag::none;
    return node;
}

icd::schema::SchemaFrameDef make_frame(int id, std::string name) {
    icd::schema::SchemaFrameDef frame;
    frame.id = id;
    frame.name = std::move(name);
    frame.description = "frame";
    frame.type = icd::FrameType::data;
    frame.order = icd::ByteOrder::little_endian;
    frame.roots.push_back(make_node("root", 0));
    return frame;
}

} // namespace

TEST(BuilderTest, DuplicateFrameId) {
    icd::schema::SchemaConfig config;
    config.frames.push_back(make_frame(1, "frame-a"));
    config.frames.push_back(make_frame(1, "frame-b"));
    auto result = icd::schema::build_repository(config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::duplicate_frame_id);
}

TEST(BuilderTest, DuplicateFrameName) {
    icd::schema::SchemaConfig config;
    config.frames.push_back(make_frame(1, "frame-a"));
    config.frames.push_back(make_frame(2, "frame-a"));
    auto result = icd::schema::build_repository(config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::duplicate_frame_name);
}

TEST(BuilderTest, NormalBuildWithAttrs) {
    icd::schema::SchemaConfig config;
    auto frame = make_frame(10, "frame-ok");
    auto child = make_node("child", 1);
    child.attrs.system_name = "SystemA";
    child.attrs.group_name = "GroupA";
    child.attrs.unit = "V";
    child.attrs.min = 1.5f;
    child.attrs.max = 9.5f;
    child.attrs.scale_a = 2.0f;
    child.attrs.scale_b = 3.0f;
    frame.roots.front().children.push_back(std::move(child));
    config.frames.push_back(std::move(frame));
    auto result = icd::schema::build_repository(config);
    ASSERT_TRUE(result.has_value());
    const auto& repo = result.value();
    EXPECT_EQ(repo.frames().size(), 1u);
    EXPECT_NE(repo.find(10), nullptr);
    EXPECT_NE(repo.find("frame-ok"), nullptr);
    const auto* child_node = repo.find("frame-ok", "child");
    ASSERT_NE(child_node, nullptr);
    EXPECT_EQ(child_node->attrs().system_name, "SystemA");
    EXPECT_EQ(child_node->attrs().group_name, "GroupA");
    EXPECT_EQ(child_node->attrs().unit, "V");
    ASSERT_TRUE(child_node->attrs().min.has_value());
    EXPECT_EQ(*child_node->attrs().min, 1.5f);
    ASSERT_TRUE(child_node->attrs().max.has_value());
    EXPECT_EQ(*child_node->attrs().max, 9.5f);
    ASSERT_TRUE(child_node->attrs().scale_a.has_value());
    EXPECT_EQ(*child_node->attrs().scale_a, 2.0f);
    ASSERT_TRUE(child_node->attrs().scale_b.has_value());
    EXPECT_EQ(*child_node->attrs().scale_b, 3.0f);
}

TEST(BuilderTest, EmptyNodeNameRejected) {
    icd::schema::SchemaConfig config;
    auto frame = make_frame(20, "frame-empty-node");
    frame.roots.front().name.clear();
    config.frames.push_back(std::move(frame));
    auto result = icd::schema::build_repository(config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::invalid_node);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
