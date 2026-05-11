#include <gtest/gtest.h>
#include <icd/frame.hpp>
#include <icd/node.hpp>
#include <icd/repository.hpp>
#include <memory>

TEST(RepositoryTest, EmptyRepoReturnsNull) {
    icd::Repository repo;
    EXPECT_TRUE(repo.frames().empty());
    EXPECT_EQ(repo.find(42), nullptr);
    EXPECT_EQ(repo.find("missing-frame"), nullptr);
}

TEST(RepositoryTest, NodeTreeFind) {
    auto root = std::make_unique<icd::Node>("root", "root desc", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    auto target = std::make_unique<icd::Node>("target", "target desc", 1, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    auto nested = std::make_unique<icd::Node>("nested", "nested desc", 2, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    nested->add_child(std::make_unique<icd::Node>("deep-target", "deep desc", 3, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    target->add_child(std::move(nested));
    auto sibling = std::make_unique<icd::Node>("sibling", "sibling desc", 4, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    root->add_child(std::move(target));
    root->add_child(std::move(sibling));
    EXPECT_NE(root->find("target"), nullptr);
    EXPECT_NE(root->find("deep-target"), nullptr);
}

TEST(RepositoryTest, FrameFind) {
    auto root = std::make_unique<icd::Node>("root", "root desc", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    root->add_child(std::make_unique<icd::Node>("target", "target desc", 1, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    root->add_child(std::make_unique<icd::Node>("deep-target", "deep desc", 2, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    auto frame = std::make_unique<icd::Frame>(1, "frame-a", "frame desc", icd::FrameType::data, icd::ByteOrder::little_endian);
    frame->add_root(std::move(root));
    EXPECT_EQ(frame->find("missing-node"), nullptr);
    EXPECT_NE(frame->find("target"), nullptr);
    EXPECT_NE(frame->find("deep-target"), nullptr);
}

TEST(RepositoryTest, MultiFrameFind) {
    icd::Repository repo;
    auto frame_a = std::make_unique<icd::Frame>(1, "frame-a", "frame desc", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto root_a = std::make_unique<icd::Node>("root-a", "root a", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    root_a->add_child(std::make_unique<icd::Node>("target", "target in a", 1, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    frame_a->add_root(std::move(root_a));
    auto frame_b = std::make_unique<icd::Frame>(2, "frame-b", "frame desc", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto root_b = std::make_unique<icd::Node>("root-b", "root b", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    root_b->add_child(std::make_unique<icd::Node>("target", "duplicate name in other frame", 1, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    frame_b->add_root(std::move(root_b));
    repo.add_frame(std::move(frame_a));
    repo.add_frame(std::move(frame_b));
    EXPECT_NE(repo.find(1), nullptr);
    EXPECT_NE(repo.find("frame-a"), nullptr);
    EXPECT_NE(repo.find("frame-a", "target"), nullptr);
    EXPECT_NE(repo.find("frame-b", "target"), nullptr);
    EXPECT_NE(repo.find("frame-a", "target"), repo.find("frame-b", "target"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
