#include <gtest/gtest.h>

#include <QApplication>
#include <QGraphicsView>
#include <QScrollBar>
#include <QWheelEvent>

#include <memory>

#include "IcdBitLayoutView.h"

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
  }

  void TearDown() override { app_.reset(); }

 private:
  std::unique_ptr<QApplication> app_;
};

const testing::Environment* const env =
    testing::AddGlobalTestEnvironment(new IcdBitLayoutViewEnv);

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
