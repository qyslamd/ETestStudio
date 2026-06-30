#include <gtest/gtest.h>

#include <QTemporaryDir>

#include "TestProgramData.h"

using namespace etest::app;

TEST(TestProgramDataTest, NewTestProgramFileIsLoadableWithDefaultName) {
  QTemporaryDir tempDir;
  ASSERT_TRUE(tempDir.isValid());

  const QString filePath = tempDir.filePath(QStringLiteral("新建测试用例.tcase"));
  ASSERT_TRUE(saveDefaultTestProgram(filePath));

  TestProgramData suite = loadTestProgram(filePath);

  EXPECT_EQ(QStringLiteral("新建测试用例"), suite.name);
  EXPECT_EQ(QStringLiteral("1.0"), suite.version);
  EXPECT_TRUE(suite.setup.isEmpty());
  EXPECT_TRUE(suite.teardown.isEmpty());
  EXPECT_TRUE(suite.cases.isEmpty());
}
