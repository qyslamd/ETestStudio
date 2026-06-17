#include <gtest/gtest.h>
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QFile>
#include <QTemporaryDir>

#include "topology/TopologySceneRenderer.h"

using namespace etest::topology;

namespace {

/// 所有测试共享的 QApplication（Qt GUI 类需要）
class TestEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    int argc = 1;
    char name[] = "test";
    char* argv = name;
    app_ = std::make_unique<QApplication>(argc, &argv);
  }
  void TearDown() override { app_.reset(); }

 private:
  std::unique_ptr<QApplication> app_;
};

// 注册全局环境（在所有 test 之前 SetUp）
testing::Environment* const env =
    testing::AddGlobalTestEnvironment(new TestEnv);

/// 创建一个带内容的简易场景用于测试
static QGraphicsScene* makeTestScene() {
  auto* scene = new QGraphicsScene();
  scene->setSceneRect(0, 0, 200, 100);
  auto* rect = scene->addRect(10, 10, 180, 80);
  rect->setBrush(QColor(100, 180, 255));
  rect->setPen(QPen(Qt::darkBlue, 2));
  return scene;
}

}  // namespace

TEST(SceneRendererTest, RenderPng) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath("test.png");

  auto* scene = makeTestScene();
  bool ok = renderSceneToFile(scene, path);
  delete scene;

  ASSERT_TRUE(ok);
  QFile f(path);
  ASSERT_TRUE(f.exists());
  EXPECT_GT(f.size(), 500);
}

TEST(SceneRendererTest, RenderSvg) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath("test.svg");

  auto* scene = makeTestScene();
  bool ok = renderSceneToFile(scene, path);
  delete scene;

  ASSERT_TRUE(ok);
  QFile f(path);
  ASSERT_TRUE(f.open(QIODevice::ReadOnly));
  QByteArray content = f.readAll();
  f.close();
  EXPECT_TRUE(content.contains("<svg"));
  EXPECT_TRUE(content.contains("</svg>"));
  EXPECT_GT(f.size(), 200);
}

// QPdfWriter 直接写 PDF 文件，不依赖打印机驱动
TEST(SceneRendererTest, RenderPdf) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath("test.pdf");

  auto* scene = makeTestScene();
  bool ok = renderSceneToFile(scene, path);
  delete scene;

  ASSERT_TRUE(ok);
  QFile f(path);
  ASSERT_TRUE(f.exists());
  EXPECT_GT(f.size(), 200);
  ASSERT_TRUE(f.open(QIODevice::ReadOnly));
  QByteArray header = f.read(5);
  f.close();
  EXPECT_EQ(header, QByteArray("%PDF-"));
}

TEST(SceneRendererTest, EmptyScene) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath("out.png");

  auto* scene = new QGraphicsScene();
  bool ok = renderSceneToFile(scene, path);
  delete scene;

  EXPECT_FALSE(ok);
}

TEST(SceneRendererTest, NullScene) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath("out.png");
  EXPECT_FALSE(renderSceneToFile(nullptr, path));
}

TEST(SceneRendererTest, UnknownExtension) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath("test.bin");

  auto* scene = makeTestScene();
  bool ok = renderSceneToFile(scene, path);
  delete scene;

  EXPECT_FALSE(ok);
}

TEST(SceneRendererTest, RenderSvgReturnsFalseWhenFileCannotBeWritten) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath("missing-dir/test.svg");

  auto* scene = makeTestScene();
  bool ok = renderSceneToFile(scene, path);
  delete scene;

  EXPECT_FALSE(ok);
  EXPECT_FALSE(QFile::exists(path));
}

TEST(SceneRendererTest, RenderPdfReturnsFalseWhenFileCannotBeWritten) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath("missing-dir/test.pdf");

  auto* scene = makeTestScene();
  bool ok = renderSceneToFile(scene, path);
  delete scene;

  EXPECT_FALSE(ok);
  EXPECT_FALSE(QFile::exists(path));
}
