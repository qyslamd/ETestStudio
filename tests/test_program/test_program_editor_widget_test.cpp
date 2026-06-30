#include <gtest/gtest.h>

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>

#include <memory>

#include "TestProgramEditorWidget.h"

using namespace etest::app;

namespace {

class TestProgramEditorWidgetEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    if (QApplication::instance()) {
      return;
    }
    int argc = 1;
    char name[] = "test_program_editor_widget_test";
    char* argv = name;
    app_ = std::make_unique<QApplication>(argc, &argv);
  }

  void TearDown() override { app_.reset(); }

 private:
  std::unique_ptr<QApplication> app_;
};

const testing::Environment* const env =
    testing::AddGlobalTestEnvironment(new TestProgramEditorWidgetEnv);

}  // namespace

TEST(TestProgramEditorWidgetTest, UsesMainWindowShellForEditorWidget) {
  TestProgramEditorWidget editor(QString{});

  EXPECT_NE(qobject_cast<QMainWindow*>(&editor), nullptr);
  EXPECT_EQ(editor.widget(), &editor);
  EXPECT_EQ(QStringLiteral("testprogram"), editor.editorType());
}

TEST(TestProgramEditorWidgetTest, EmbeddedModeHidesOnlyMainWindowMenuBar) {
  TestProgramEditorWidget editor(QString{});

  editor.setEmbeddedMode(true);
  EXPECT_TRUE(editor.menuBar()->isHidden());

  editor.setEmbeddedMode(false);
  EXPECT_FALSE(editor.menuBar()->isHidden());
}

TEST(TestProgramEditorWidgetTest, NewProgramResetsToCleanUntitledState) {
  TestProgramEditorWidget editor(QStringLiteral("unused.tcase"));

  editor.newProgram();

  EXPECT_FALSE(editor.isModified());
  EXPECT_TRUE(editor.filePath().isEmpty());
  EXPECT_EQ(QStringLiteral("未命名测试程序"), editor.displayName());
}
