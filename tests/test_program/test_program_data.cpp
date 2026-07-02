#include <gtest/gtest.h>

#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>

#include "TestProgramData.h"

using namespace etest::app;

TEST(TestProgramDataTest, NewTestProgramFileIsLoadableWithDefaultName) {
  QTemporaryDir tempDir;
  ASSERT_TRUE(tempDir.isValid());

  const QString filePath = tempDir.filePath(QStringLiteral("新建测试用例.tcase"));
  ASSERT_TRUE(saveDefaultTestProgram(filePath));

  TestProgramData suite = loadTestProgram(filePath);

  EXPECT_EQ(QStringLiteral("新建测试用例"), suite.name);
  EXPECT_EQ(QStringLiteral("1.1"), suite.version);
  EXPECT_TRUE(suite.setup.isEmpty());
  EXPECT_TRUE(suite.teardown.isEmpty());
  EXPECT_TRUE(suite.cases.isEmpty());
}

TEST(TestProgramDataTest, SaveAndLoadAllCommandTypes) {
  QTemporaryDir tempDir;
  ASSERT_TRUE(tempDir.isValid());

  // ── 构建包含所有指令类型的测试数据 ──
  TestProgramData suite;
  suite.name = QStringLiteral("全指令测试");
  suite.version = QStringLiteral("1.1");

  // Setup: 多种命令
  TestStepData setStep;
  setStep.cmd = QStringLiteral("SET");
  setStep.target = QStringLiteral("温度传感器");
  setStep.value = 37.5;
  setStep.delayMs = 100;
  setStep.description = QStringLiteral("设置温度");
  suite.setup.append(setStep);

  TestStepData verifyStep;
  verifyStep.cmd = QStringLiteral("VERIFY");
  verifyStep.target = QStringLiteral("温度传感器");
  verifyStep.value = 37.5;
  verifyStep.tolerance.enabled = true;
  verifyStep.tolerance.min = -0.5;
  verifyStep.tolerance.max = 0.5;
  verifyStep.timeoutMs = 3000;
  verifyStep.description = QStringLiteral("验证温度");
  suite.setup.append(verifyStep);

  TestStepData waitStep;
  waitStep.cmd = QStringLiteral("WAIT");
  waitStep.condition.target = QStringLiteral("压力传感器");
  waitStep.condition.op = QStringLiteral(">=");
  waitStep.condition.value = 10.0;
  waitStep.timeoutMs = 5000;
  waitStep.delayMs = 200;
  waitStep.description = QStringLiteral("等待压力达标");
  suite.setup.append(waitStep);

  TestStepData delayStep;
  delayStep.cmd = QStringLiteral("DELAY");
  delayStep.delayMs = 1000;
  delayStep.description = QStringLiteral("延时1秒");
  suite.setup.append(delayStep);

  TestStepData actionStep;
  actionStep.cmd = QStringLiteral("ACTION");
  actionStep.description = QStringLiteral("请观察指示灯是否亮起");
  suite.setup.append(actionStep);

  TestStepData logStep;
  logStep.cmd = QStringLiteral("LOG");
  logStep.description = QStringLiteral("当前温度正常");
  suite.setup.append(logStep);

  // 测试用例：控制流
  TestCaseData tc;
  tc.name = QStringLiteral("循环测试");

  // LOOP 步骤
  TestStepData loopStep;
  loopStep.cmd = QStringLiteral("LOOP");
  loopStep.loopCount = 5;

  TestStepData loopBodyStep;
  loopBodyStep.cmd = QStringLiteral("SET");
  loopBodyStep.target = QStringLiteral("加热器");
  loopBodyStep.value = 1;
  loopBodyStep.delayMs = 500;
  loopStep.subSteps.append(loopBodyStep);

  tc.steps.append(loopStep);

  // WHILE 步骤
  TestStepData whileStep;
  whileStep.cmd = QStringLiteral("WHILE");
  whileStep.condition.target = QStringLiteral("温度");
  whileStep.condition.op = QStringLiteral("<");
  whileStep.condition.value = 30.0;
  whileStep.loopIntervalMs = 1000;
  whileStep.timeoutMs = 30000;

  TestStepData whileBodyStep;
  whileBodyStep.cmd = QStringLiteral("DELAY");
  whileBodyStep.delayMs = 500;
  whileStep.subSteps.append(whileBodyStep);

  tc.steps.append(whileStep);

  // IF 步骤
  TestStepData ifStep;
  ifStep.cmd = QStringLiteral("IF");
  ifStep.condition.target = QStringLiteral("温度");
  ifStep.condition.op = QStringLiteral(">=");
  ifStep.condition.value = 30.0;

  TestStepData thenStep;
  thenStep.cmd = QStringLiteral("LOG");
  thenStep.description = QStringLiteral("温度达标");
  ifStep.subSteps.append(thenStep);

  TestStepData elseStep;
  elseStep.cmd = QStringLiteral("SET");
  elseStep.target = QStringLiteral("加热器");
  elseStep.value = 1;
  ifStep.elseSubSteps.append(elseStep);

  tc.steps.append(ifStep);

  // 故障注入步骤
  TestStepData injectStep;
  injectStep.cmd = QStringLiteral("INJECT_FAULT");
  injectStep.target = QStringLiteral("温度");
  injectStep.fault.type = QStringLiteral("stuck_at");
  injectStep.fault.value = 999;
  injectStep.description = QStringLiteral("注入故障");
  tc.steps.append(injectStep);

  TestStepData clearStep;
  clearStep.cmd = QStringLiteral("CLEAR_FAULT");
  clearStep.target = QStringLiteral("温度");
  tc.steps.append(clearStep);

  suite.cases.append(tc);

  // PHOTO / RECORD 在 Teardown
  TestStepData photoStep;
  photoStep.cmd = QStringLiteral("PHOTO");
  photoStep.description = QStringLiteral("拍照记录");
  suite.teardown.append(photoStep);

  TestStepData recordStep;
  recordStep.cmd = QStringLiteral("RECORD");
  recordStep.value = false;
  recordStep.description = QStringLiteral("停止录制");
  suite.teardown.append(recordStep);

  // ── 保存 ──
  const QString filePath = tempDir.filePath(QStringLiteral("full_test.tcase"));
  ASSERT_TRUE(saveTestProgram(filePath, suite));

  // ── 重新加载 ──
  TestProgramData loaded = loadTestProgram(filePath);

  // ── 验证 ──
  EXPECT_EQ(suite.name, loaded.name);
  EXPECT_EQ(QStringLiteral("1.1"), loaded.version);
  ASSERT_EQ(suite.setup.size(), loaded.setup.size());

  // SET
  EXPECT_EQ(QStringLiteral("SET"), loaded.setup[0].cmd);
  EXPECT_EQ(QStringLiteral("温度传感器"), loaded.setup[0].target);
  EXPECT_EQ(37.5, loaded.setup[0].value.toDouble());
  EXPECT_EQ(100, loaded.setup[0].delayMs);

  // VERIFY with tolerance
  EXPECT_EQ(QStringLiteral("VERIFY"), loaded.setup[1].cmd);
  EXPECT_TRUE(loaded.setup[1].tolerance.enabled);
  EXPECT_DOUBLE_EQ(-0.5, loaded.setup[1].tolerance.min);
  EXPECT_DOUBLE_EQ(0.5, loaded.setup[1].tolerance.max);

  // WAIT with condition
  EXPECT_EQ(QStringLiteral("WAIT"), loaded.setup[2].cmd);
  EXPECT_EQ(QStringLiteral("压力传感器"), loaded.setup[2].condition.target);
  EXPECT_EQ(QStringLiteral(">="), loaded.setup[2].condition.op);
  EXPECT_EQ(10.0, loaded.setup[2].condition.value.toDouble());

  // DELAY
  EXPECT_EQ(QStringLiteral("DELAY"), loaded.setup[3].cmd);
  EXPECT_EQ(1000, loaded.setup[3].delayMs);

  // ACTION
  EXPECT_EQ(QStringLiteral("ACTION"), loaded.setup[4].cmd);
  EXPECT_EQ(QStringLiteral("请观察指示灯是否亮起"), loaded.setup[4].description);

  // LOG
  EXPECT_EQ(QStringLiteral("LOG"), loaded.setup[5].cmd);

  // 控制流
  ASSERT_EQ(1, loaded.cases.size());
  ASSERT_EQ(5, loaded.cases[0].steps.size());

  // LOOP
  EXPECT_EQ(QStringLiteral("LOOP"), loaded.cases[0].steps[0].cmd);
  EXPECT_EQ(5, loaded.cases[0].steps[0].loopCount);
  ASSERT_EQ(1, loaded.cases[0].steps[0].subSteps.size());
  EXPECT_EQ(QStringLiteral("SET"), loaded.cases[0].steps[0].subSteps[0].cmd);

  // WHILE
  EXPECT_EQ(QStringLiteral("WHILE"), loaded.cases[0].steps[1].cmd);
  EXPECT_EQ(1000, loaded.cases[0].steps[1].loopIntervalMs);
  EXPECT_EQ(30000, loaded.cases[0].steps[1].timeoutMs);
  EXPECT_EQ(QStringLiteral("<"), loaded.cases[0].steps[1].condition.op);

  // IF
  EXPECT_EQ(QStringLiteral("IF"), loaded.cases[0].steps[2].cmd);
  ASSERT_EQ(1, loaded.cases[0].steps[2].subSteps.size());
  EXPECT_EQ(QStringLiteral("LOG"), loaded.cases[0].steps[2].subSteps[0].cmd);
  ASSERT_EQ(1, loaded.cases[0].steps[2].elseSubSteps.size());
  EXPECT_EQ(QStringLiteral("SET"), loaded.cases[0].steps[2].elseSubSteps[0].cmd);

  // INJECT_FAULT
  EXPECT_EQ(QStringLiteral("INJECT_FAULT"), loaded.cases[0].steps[3].cmd);
  EXPECT_EQ(QStringLiteral("stuck_at"), loaded.cases[0].steps[3].fault.type);
  EXPECT_EQ(999, loaded.cases[0].steps[3].fault.value.toInt());

  // CLEAR_FAULT
  EXPECT_EQ(QStringLiteral("CLEAR_FAULT"), loaded.cases[0].steps[4].cmd);
  EXPECT_EQ(QStringLiteral("温度"), loaded.cases[0].steps[4].target);

  // PHOTO / RECORD
  ASSERT_EQ(2, loaded.teardown.size());
  EXPECT_EQ(QStringLiteral("PHOTO"), loaded.teardown[0].cmd);
  EXPECT_EQ(QStringLiteral("RECORD"), loaded.teardown[1].cmd);
}

TEST(TestProgramDataTest, BackwardCompatOldV1Format) {
  // v1.0 格式的 JSON 字符串（没有新字段）
  const QByteArray oldJson = R"({
    "version": "1.0",
    "name": "旧版测试",
    "description": "",
    "setup": [
      {
        "cmd": "SET",
        "target": "温度",
        "value": 25,
        "delayMs": 0,
        "timeoutMs": 5000,
        "description": ""
      }
    ],
    "teardown": [],
    "cases": []
  })";

  QJsonDocument doc = QJsonDocument::fromJson(oldJson);
  ASSERT_TRUE(doc.isObject());

  TestProgramData suite = testProgramFromJson(doc.object());

  EXPECT_EQ(QStringLiteral("旧版测试"), suite.name);
  EXPECT_EQ(QStringLiteral("1.0"), suite.version);
  ASSERT_EQ(1, suite.setup.size());
  EXPECT_EQ(QStringLiteral("SET"), suite.setup[0].cmd);
  EXPECT_EQ(QStringLiteral("温度"), suite.setup[0].target);
  EXPECT_EQ(25, suite.setup[0].value.toInt());

  // 新字段应为默认值
  EXPECT_TRUE(suite.setup[0].condition.target.isEmpty());
  EXPECT_FALSE(suite.setup[0].tolerance.enabled);
  EXPECT_TRUE(suite.setup[0].fault.type.isEmpty());
  EXPECT_EQ(1, suite.setup[0].loopCount);
  EXPECT_TRUE(suite.setup[0].subSteps.isEmpty());
  EXPECT_TRUE(suite.setup[0].elseSubSteps.isEmpty());
}
