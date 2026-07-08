#include <gtest/gtest.h>
#include <QStringList>
#include <QVector>

#include "SignalRegistry.h"
#include "icd/frame.hpp"
#include "icd/node.hpp"
#include "icd/repository.hpp"
#include "utils/SignalSyncHelper.h"

using namespace etest::core;
using namespace etest::app;

// ══════════════════════════════════════════════════════════════════════════════
//  buildNodePath
// ══════════════════════════════════════════════════════════════════════════════

TEST(SignalSyncHelperTest, BuildNodePathOneLevel) {
  icd::NodeAttrs attrs;
  icd::Node root("root", "desc", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, attrs);
  QString path = buildNodePath(&root);
  EXPECT_EQ(path, "root");
}

TEST(SignalSyncHelperTest, BuildNodePathNested) {
  icd::NodeAttrs attrs;
  auto root = std::make_unique<icd::Node>(
      "frame", "desc", 0, 0, 32, icd::ValueType::integer, icd::Tag::none, attrs);
  auto child = std::make_unique<icd::Node>(
      "field1", "desc", 0, 0, 16, icd::ValueType::word, icd::Tag::none, attrs);
  auto grandchild = std::make_unique<icd::Node>(
      "subfield", "desc", 0, 8, 8, icd::ValueType::byte_, icd::Tag::none, attrs);

  // 构建树：root → field1 → subfield
  icd::Node* rawChild = child.get();
  icd::Node* rawGrandchild = grandchild.get();
  rawChild->add_child(std::move(grandchild));
  root->add_child(std::move(child));

  QString path = buildNodePath(rawGrandchild);
  EXPECT_EQ(path, "frame/field1/subfield");
}

// ══════════════════════════════════════════════════════════════════════════════
//  synchronizeRegistry
// ══════════════════════════════════════════════════════════════════════════════

TEST(SignalSyncHelperTest, SynchronizeRegistry) {
  // 准备 icd::Repository 数据
  icd::NodeAttrs attrs;

  auto frame = std::make_unique<icd::Frame>(
      1, "A429_发送", "test frame", icd::FrameType::data, icd::ByteOrder::little_endian);
  auto root = std::make_unique<icd::Node>(
      "root", "", 0, 0, 32, icd::ValueType::integer, icd::Tag::none, attrs);
  auto sig = std::make_unique<icd::Node>(
      "燃油阀门1", "", 0, 0, 16, icd::ValueType::word, icd::Tag::none, attrs);
  icd::Node* rawSig = sig.get();
  root->add_child(std::move(sig));
  frame->add_root(std::move(root));

  icd::Repository repo;
  repo.add_frame(std::move(frame));

  // 准备 SignalRegistry
  SignalRegistry registry;
  registry.registerDevice("dev-001", "EPH5272-1");
  registry.bindPortToFrames("dev-001", "ch0", QStringList() << "A429_发送");

  // 执行同步
  synchronizeRegistry(registry, &repo);

  // 验证：应能 resolve 刚注册的信号
  QString uuid = SignalRegistry::computeUuid(
      "dev-001", "ch0", "A429_发送", "root/燃油阀门1");
  auto resolved = registry.resolve(uuid);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->deviceName, "EPH5272-1");
  EXPECT_EQ(resolved->nodePath, "root/燃油阀门1");
}

TEST(SignalSyncHelperTest, SynchronizeRegistryMultipleFrames) {
  icd::NodeAttrs attrs;

  // 帧 A
  auto frameA = std::make_unique<icd::Frame>(
      1, "A429_发送", "", icd::FrameType::data, icd::ByteOrder::little_endian);
  auto rootA = std::make_unique<icd::Node>(
      "root", "", 0, 0, 32, icd::ValueType::integer, icd::Tag::none, attrs);
  rootA->add_child(std::make_unique<icd::Node>(
      "temp", "", 0, 0, 16, icd::ValueType::word, icd::Tag::none, attrs));
  frameA->add_root(std::move(rootA));

  // 帧 B
  auto frameB = std::make_unique<icd::Frame>(
      2, "A429_接收", "", icd::FrameType::data, icd::ByteOrder::little_endian);
  auto rootB = std::make_unique<icd::Node>(
      "root", "", 0, 0, 32, icd::ValueType::integer, icd::Tag::none, attrs);
  rootB->add_child(std::make_unique<icd::Node>(
      "pressure", "", 0, 0, 16, icd::ValueType::word, icd::Tag::none, attrs));
  frameB->add_root(std::move(rootB));

  icd::Repository repo;
  repo.add_frame(std::move(frameA));
  repo.add_frame(std::move(frameB));

  SignalRegistry registry;
  registry.registerDevice("dev-001", "EPH5272-1");
  registry.bindPortToFrames("dev-001", "ch0",
                             QStringList() << "A429_发送" << "A429_接收");

  synchronizeRegistry(registry, &repo);

  // 两个信号都应能 resolve
  QString u1 = SignalRegistry::computeUuid(
      "dev-001", "ch0", "A429_发送", "root/temp");
  QString u2 = SignalRegistry::computeUuid(
      "dev-001", "ch0", "A429_接收", "root/pressure");
  EXPECT_TRUE(registry.resolve(u1).has_value());
  EXPECT_TRUE(registry.resolve(u2).has_value());
}

TEST(SignalSyncHelperTest, SynchronizeRegistryEmptyRepo) {
  etest::core::SignalRegistry registry;
  registry.registerDevice("dev-001", "EPH5272-1");
  registry.bindPortToFrames("dev-001", "ch0", QStringList() << "nonexistent");

  // repo 中没有该帧 → 应静默跳过
  icd::Repository repo;
  synchronizeRegistry(registry, &repo);

  // 端口绑定仍在，但非存在的帧不会被注册
  QString uuid = SignalRegistry::computeUuid(
      "dev-001", "ch0", "nonexistent", "root");
  EXPECT_FALSE(registry.resolve(uuid).has_value());
}
