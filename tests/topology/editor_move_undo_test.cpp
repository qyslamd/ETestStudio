#include <gtest/gtest.h>
#include <QApplication>
#include <memory>

#define private public
#include "topology/TopologyEditorWidget.h"
#undef private
#include "topology/TopologyDocument.h"
#include "topology/TopologyScene.h"
#include "topology/UndoCommands.h"

using namespace etest::topology;

namespace {

class TestEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    if (QApplication::instance())
      return;
    int argc = 1;
    char name[] = "test";
    char* argv = name;
    app_ = std::make_unique<QApplication>(argc, &argv);
  }

  void TearDown() override { app_.reset(); }

 private:
  std::unique_ptr<QApplication> app_;
};

testing::Environment* const env =
    testing::AddGlobalTestEnvironment(new TestEnv);

}  // namespace

TEST(TopologyEditorMoveUndoTest, RedoMoveKeepsCommandPosition) {
  TopologyEditorWidget editor;
  TopologyProduct product;
  product.name = QStringLiteral("UUT");
  product.position = QPointF(10, 20);
  editor.document()->addProduct(product);
  editor.reloadScene();

  auto* item = editor.scene_->findUutItem(0);
  ASSERT_NE(item, nullptr);
  item->setPos(QPointF(30, 40));

  editor.document()->undoStack()->push(new MoveProductCommand(
      editor.document(), 0, QPointF(10, 20), QPointF(30, 40)));
  ASSERT_EQ(editor.document()->product(0)->position, QPointF(30, 40));

  editor.document()->undoStack()->undo();
  ASSERT_EQ(editor.document()->product(0)->position, QPointF(10, 20));

  editor.document()->undoStack()->redo();

  EXPECT_EQ(editor.document()->product(0)->position, QPointF(30, 40));
}
