#include <gtest/gtest.h>

#include <QApplication>
#include <QGraphicsView>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <QWheelEvent>

#include <icd/frame.hpp>
#include <icd/node.hpp>

#include <memory>

#include "IcdBitLayoutView.h"

Q_DECLARE_METATYPE(const icd::Node*)

using namespace etest::protocol;

namespace {

class IcdBitLayoutViewEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    if (QApplication::instance()) {
      return;
    }
    int argc = 1;
    char name[] = "test_icd_bit_layout_view";
    char* argv = name;
    app_ = std::make_unique<QApplication>(argc, &argv);
    qRegisterMetaType<const icd::Node*>("const icd::Node*");
  }

  void TearDown() override { app_.reset(); }

 private:
  std::unique_ptr<QApplication> app_;
};

const testing::Environment* const env =
    testing::AddGlobalTestEnvironment(new IcdBitLayoutViewEnv);

std::unique_ptr<icd::Node> MakeNode(const std::string& name, int offset,
                                    int start_bit, int bit_width,
                                    icd::ValueType value_type) {
  return std::make_unique<icd::Node>(name, std::string{}, offset, start_bit,
                                     bit_width, value_type, icd::Tag::none,
                                     icd::NodeAttrs{});
}

IcdBitLayoutScene* SceneFrom(IcdBitLayoutView* bit_layout) {
  auto* graphics_view = bit_layout->findChild<QGraphicsView*>();
  if (!graphics_view) {
    return nullptr;
  }
  return qobject_cast<IcdBitLayoutScene*>(graphics_view->scene());
}

QGraphicsView* GraphicsViewFrom(IcdBitLayoutView* bit_layout) {
  return bit_layout->findChild<QGraphicsView*>();
}

}  // namespace

TEST(IcdBitLayoutViewTest, OneByteSectionKeepsRoomForTypeBadge) {
  IcdBitLayoutScene scene;
  auto* item = scene.addBlock(QStringLiteral("5V导光板电源 设定电流值"),
                              QStringLiteral("word"), 4, 0, 8,
                              QColor(80, 160, 220));

  ASSERT_NE(item, nullptr);
  EXPECT_GE(item->sectionWidth(), 420);
  EXPECT_EQ(item->cellSize(), 38);
}

TEST(IcdBitLayoutViewTest, ShowsScrollBarsAndDoesNotZoomOnWheel) {
  IcdBitLayoutView bit_layout;
  auto* graphics_view = bit_layout.findChild<QGraphicsView*>();

  ASSERT_NE(graphics_view, nullptr);
  EXPECT_EQ(graphics_view->horizontalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
  EXPECT_EQ(graphics_view->verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);

  const auto before = graphics_view->transform();
  QPoint pos(10, 10);
  QWheelEvent wheel_event(pos, graphics_view->mapToGlobal(pos), QPoint(),
                          QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);

  QApplication::sendEvent(graphics_view->viewport(), &wheel_event);

  EXPECT_EQ(graphics_view->transform(), before);
}

TEST(IcdBitLayoutViewTest, PlainFrameShowsRootFieldsAsTopLevelItems) {
  icd::Frame frame(1, "PlainFrame", "", icd::FrameType::data,
                   icd::ByteOrder::little_endian);
  frame.add_root(MakeNode("byte0", 0, 0, 8, icd::ValueType::byte_));
  frame.add_root(MakeNode("current", 1, 0, 16, icd::ValueType::word));
  frame.add_root(MakeNode("crc", 3, 0, 16, icd::ValueType::word));

  IcdBitLayoutView bit_layout;
  bit_layout.loadFromFrame(frame);

  auto* scene = SceneFrom(&bit_layout);
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->topLevelLayoutItemCount(), 3);
  EXPECT_EQ(scene->fieldItemCount(), 3);
  EXPECT_EQ(scene->containerItemCount(), 0);
}

TEST(IcdBitLayoutViewTest, ChildFieldsStayInsideContainerRootItem) {
  icd::Frame frame(2, "A429Frame", "", icd::FrameType::cmd,
                   icd::ByteOrder::little_endian);
  auto parent = MakeNode("A429_IN1(001) 发送数据", 0, 0, 32,
                         icd::ValueType::longword);
  parent->add_child(MakeNode("Label", 0, 0, 8, icd::ValueType::byte_));
  const icd::Node* label = parent->children()[0].get();
  parent->add_child(MakeNode("SDI", 1, 0, 2, icd::ValueType::byte_));
  auto* data = MakeNode("Data", 1, 2, 19, icd::ValueType::integer).release();
  parent->add_child(std::unique_ptr<icd::Node>(data));
  parent->add_child(MakeNode("SSM", 3, 5, 2, icd::ValueType::byte_));
  parent->add_child(MakeNode("Parity", 3, 7, 1, icd::ValueType::byte_));
  frame.add_root(std::move(parent));

  IcdBitLayoutView bit_layout;
  bit_layout.loadFromFrame(frame);

  auto* scene = SceneFrom(&bit_layout);
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->topLevelLayoutItemCount(), 1);
  EXPECT_EQ(scene->fieldItemCount(), 0);
  EXPECT_EQ(scene->containerItemCount(), 1);

  auto containers = scene->containerItems();
  ASSERT_EQ(containers.size(), 1);
  EXPECT_EQ(containers[0]->childFieldCount(), 5);
  EXPECT_TRUE(containers[0]->containsChildNode(label));
  EXPECT_EQ(containers[0]->childRelativeRange(data), qMakePair(10, 28));
}

TEST(IcdBitLayoutViewTest, ChildClickEmitsExactNodePointerForDuplicateNames) {
  icd::Frame frame(3, "DuplicateNames", "", icd::FrameType::data,
                   icd::ByteOrder::little_endian);

  auto first_parent = MakeNode("word0", 0, 0, 32, icd::ValueType::longword);
  first_parent->add_child(MakeNode("DATA", 0, 0, 8, icd::ValueType::byte_));
  frame.add_root(std::move(first_parent));

  auto second_parent = MakeNode("word1", 4, 0, 32, icd::ValueType::longword);
  second_parent->add_child(MakeNode("DATA", 4, 0, 8, icd::ValueType::byte_));
  auto* expected_child = second_parent->children()[0].get();
  frame.add_root(std::move(second_parent));

  IcdBitLayoutView bit_layout;
  bit_layout.resize(1200, 600);
  bit_layout.show();
  bit_layout.loadFromFrame(frame);

  auto* scene = SceneFrom(&bit_layout);
  auto* graphics_view = GraphicsViewFrom(&bit_layout);
  ASSERT_NE(scene, nullptr);
  ASSERT_NE(graphics_view, nullptr);

  auto containers = scene->containerItems();
  ASSERT_EQ(containers.size(), 2);
  auto* child_item = containers[1]->childFieldItem(expected_child);
  ASSERT_NE(child_item, nullptr);

  QSignalSpy spy(&bit_layout, &IcdBitLayoutView::nodeClicked);
  ASSERT_TRUE(spy.isValid());

  const QPoint view_pos = graphics_view->mapFromScene(
      child_item->sceneBoundingRect().center());
  QTest::mouseClick(graphics_view->viewport(), Qt::LeftButton, Qt::NoModifier,
                    view_pos);

  ASSERT_EQ(spy.count(), 1);
  const auto emitted = qvariant_cast<const icd::Node*>(spy.takeFirst().at(0));
  EXPECT_EQ(emitted, expected_child);
}

// ============================================================
// 第二阶段：字段语义可视化
// ============================================================

namespace {

icd::NodeAttrs MakeAttrs() { return icd::NodeAttrs{}; }

std::unique_ptr<icd::Node> MakeSemanticNode(
    const std::string& name, const std::string& description, int offset,
    int bit_offset, int bit_width, icd::ValueType value_type, icd::Tag tag,
    icd::NodeAttrs attrs) {
  return std::make_unique<icd::Node>(name, description, offset, bit_offset,
                                     bit_width, value_type, tag,
                                     std::move(attrs));
}

}  // namespace

TEST(NodeSemanticTest, TooltipContainsDescriptionAndBasicRange) {
  auto node = MakeSemanticNode("设定电流", "仅此项是大端", 4, 0, 16,
                               icd::ValueType::word, icd::Tag::none,
                               MakeAttrs());
  const QString tooltip = etest::protocol::buildNodeTooltip(*node);

  EXPECT_TRUE(tooltip.contains(QStringLiteral("设定电流")));
  EXPECT_TRUE(tooltip.contains(QStringLiteral("仅此项是大端")));
  EXPECT_TRUE(tooltip.contains(QStringLiteral("uint16")));
  EXPECT_TRUE(tooltip.contains(QStringLiteral("Offset")));
}

TEST(NodeSemanticTest, TooltipContainsValueTextListEnum) {
  auto attrs = MakeAttrs();
  attrs.value_text_list = "不用=0,左单元=1,右单元=2,中间单元=3";
  auto node = MakeSemanticNode("SDI", "2bit有效", 1, 0, 2,
                               icd::ValueType::byte_, icd::Tag::none, attrs);
  const QString tooltip = etest::protocol::buildNodeTooltip(*node);

  EXPECT_TRUE(tooltip.contains(QStringLiteral("不用=0")));
  EXPECT_TRUE(tooltip.contains(QStringLiteral("枚举")));
}

TEST(NodeSemanticTest, TooltipContainsUnitAndScale) {
  auto attrs = MakeAttrs();
  attrs.unit = "A";
  attrs.is_scaled = true;
  attrs.scale_a = 0.01f;
  attrs.scale_b = 0.0f;
  auto node = MakeSemanticNode("电流", "分辨率0.01", 4, 0, 16,
                               icd::ValueType::word, icd::Tag::none, attrs);
  const QString tooltip = etest::protocol::buildNodeTooltip(*node);

  EXPECT_TRUE(tooltip.contains(QStringLiteral("Unit: A")));
  EXPECT_TRUE(tooltip.contains(QStringLiteral("0.01")));
  EXPECT_TRUE(tooltip.contains(QStringLiteral("Scale")));
}

TEST(NodeSemanticTest, TooltipContainsLinkToAndSystemName) {
  auto attrs = MakeAttrs();
  attrs.link_to = "\\ComCHH50W_COM1\\ch1";
  attrs.system_name = "5V导光板电源";
  attrs.group_name = "5V导光板电源";
  auto node = MakeSemanticNode("电流", "", 4, 0, 16,
                               icd::ValueType::word, icd::Tag::none, attrs);
  const QString tooltip = etest::protocol::buildNodeTooltip(*node);

  EXPECT_TRUE(tooltip.contains(QStringLiteral("ComCHH50W_COM1")));
  EXPECT_TRUE(tooltip.contains(QStringLiteral("5V导光板电源")));
}

TEST(NodeSemanticTest, BadgesForChecksumTag) {
  auto node = MakeSemanticNode("CRC", "crc校验", 6, 0, 16,
                               icd::ValueType::word, icd::Tag::sum,
                               MakeAttrs());
  const QStringList badges = etest::protocol::buildNodeBadges(*node);
  EXPECT_TRUE(badges.contains(QStringLiteral("校验")));
}

TEST(NodeSemanticTest, BadgesForSignalValueTag) {
  auto node = MakeSemanticNode("电流", "", 4, 0, 16,
                               icd::ValueType::word, icd::Tag::signal_in_value,
                               MakeAttrs());
  const QStringList badges = etest::protocol::buildNodeBadges(*node);
  EXPECT_TRUE(badges.contains(QStringLiteral("信号值")));
}

TEST(NodeSemanticTest, BadgesForBigEndianTag) {
  auto node = MakeSemanticNode("电流", "大端字段", 4, 0, 16,
                               icd::ValueType::word,
                               icd::Tag::big_endian_value, MakeAttrs());
  const QStringList badges = etest::protocol::buildNodeBadges(*node);
  EXPECT_TRUE(badges.contains(QStringLiteral("大端")));
  EXPECT_TRUE(etest::protocol::isNodeBigEndian(*node));
}

TEST(NodeSemanticTest, BadgesForScaledField) {
  auto attrs = MakeAttrs();
  attrs.is_scaled = true;
  attrs.scale_a = 0.01f;
  auto node = MakeSemanticNode("电流", "", 4, 0, 16,
                               icd::ValueType::word, icd::Tag::none, attrs);
  const QStringList badges = etest::protocol::buildNodeBadges(*node);
  EXPECT_TRUE(badges.contains(QStringLiteral("缩放")));
}

TEST(NodeSemanticTest, BadgesForEnumField) {
  auto attrs = MakeAttrs();
  attrs.value_text_list = "不用=0,左单元=1,右单元=2,中间单元=3";
  auto node = MakeSemanticNode("SDI", "", 1, 0, 2, icd::ValueType::byte_,
                               icd::Tag::none, attrs);
  const QStringList badges = etest::protocol::buildNodeBadges(*node);
  EXPECT_TRUE(badges.contains(QStringLiteral("枚举")));
}

TEST(NodeSemanticTest, BadgesForHeaderTag) {
  auto node = MakeSemanticNode("byte0", "", 0, 0, 8, icd::ValueType::byte_,
                               icd::Tag::head, MakeAttrs());
  const QStringList badges = etest::protocol::buildNodeBadges(*node);
  EXPECT_TRUE(badges.contains(QStringLiteral("帧头")));
}

TEST(NodeSemanticTest, IsNodeBigEndianFalseForLittleEndianDefault) {
  auto node = MakeSemanticNode("电流", "", 4, 0, 16,
                               icd::ValueType::word, icd::Tag::none,
                               MakeAttrs());
  EXPECT_FALSE(etest::protocol::isNodeBigEndian(*node));
}
