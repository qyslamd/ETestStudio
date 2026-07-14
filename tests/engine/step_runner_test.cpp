#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QThread>

#include "HardwareManager.h"
#include "SignalCodec.h"
#include "SignalResolver.h"
#include "SignalRegistry.h"
#include "StepRunner.h"

#include <icd/frame.hpp>
#include <icd/node.hpp>
#include <icd/repository.hpp>

#include "plugin_sdk/ICANPlugin.h"

using namespace etest::engine;
using namespace etest::core;
using namespace etest::core::plugin;

// ══════════════════════════════════════════════════════════════════════════════
// Minimal mock CAN device for StepRunner tests
// ══════════════════════════════════════════════════════════════════════════════
class StepRunnerMockCAN : public ICANPlugin {
 public:
  // IPlugin
  bool initialize() override { return true; }
  bool start() override { return true; }
  void stop() override {}
  void uninitialize() override {}
  bool isRunning() const override { return true; }

  PluginMetaData metaData() const override {
    PluginMetaData m;
    m.id = "test.steprunner.can";
    m.name = "StepRunner Mock CAN";
    m.version = "1.0";
    return m;
  }

  // IDevicePlugin
  bool openDevice() override {
    opened_ = true;
    return true;
  }
  void closeDevice() override {
    opened_ = false;
    closed_ = true;
  }
  DeviceInfo deviceInfo() const override {
    DeviceInfo info;
    info.model = "MockCAN";
    return info;
  }
  etest::core::plugin::DeviceStatus deviceStatus() const override {
    return opened_ ? etest::core::plugin::DeviceStatus::Online
                   : etest::core::plugin::DeviceStatus::Offline;
  }

  // ICANPlugin
  bool sendMessage(quint32 id, const QByteArray& data,
                   bool /*extended*/) override {
    last_id_ = id;
    last_data_ = data;
    return true;
  }
  QByteArray receiveMessage(quint32 /*id*/) override {
    // Return known test data: bytes [0xAA, 0xBB, 0xCC, 0xDD]
    return QByteArray::fromHex("AABBCCDD");
  }
  bool setBitrate(int) override { return true; }
  int bitrate() const override { return 250000; }

  // Tracked state
  quint32 last_id_ = 0;
  QByteArray last_data_;
  bool opened_ = false;
  bool closed_ = false;
};

// ══════════════════════════════════════════════════════════════════════════════
// Friend-access helper for HardwareManager (uses existing friend declaration)
// HardwareManager declares: friend class HardwareManagerTestHelper;
// We define it in etest::engine namespace so it matches.
// ══════════════════════════════════════════════════════════════════════════════

namespace etest::engine {

class HardwareManagerTestHelper {
 public:
  static void injectDevice(HardwareManager& hm, const QString& deviceId,
                           IDevicePlugin* plugin,
                           etest::engine::DeviceStatus status) {
    HardwareManager::DeviceEntry entry;
    entry.plugin = plugin;
    entry.status = status;
    hm.device_pool_.insert(deviceId, entry);
  }

  static void injectDevice(HardwareManager& hm, const QString& deviceId,
                           IDevicePlugin* plugin) {
    injectDevice(hm, deviceId, plugin,
                 etest::engine::DeviceStatus::Online);
  }
};

}  // namespace etest::engine

// ══════════════════════════════════════════════════════════════════════════════
// Friend-access helper for StepRunner (uses our new friend declaration)
// ══════════════════════════════════════════════════════════════════════════════

namespace etest::engine {

class StepRunnerTestHelper {
 public:
  static StepResult executeSingleStep(StepRunner& runner,
                                       const TestStepData& step,
                                       int caseIndex,
                                       const QString& stepPath) {
    return runner.executeSingleStep(step, caseIndex, stepPath);
  }
};

}  // namespace etest::engine

using StepRunnerTestHelper = etest::engine::StepRunnerTestHelper;

// ══════════════════════════════════════════════════════════════════════════════
// Helper: build ICD repository with a CAN frame and node
// ══════════════════════════════════════════════════════════════════════════════

static icd::Repository buildTestIcdRepo(
    const QString& frameName = QStringLiteral("TestFrame"),
    int bitWidth = 8) {
  icd::Repository repo;

  icd::NodeAttrs attrs;
  attrs.is_scaled = true;
  attrs.scale_a = 1.0f;
  attrs.scale_b = 0.0f;

  auto node = std::make_unique<icd::Node>(
      "TestNode", "", 0, 0, bitWidth, icd::ValueType::byte_, icd::Tag::none,
      attrs);

  auto frame = std::make_unique<icd::Frame>(
      1, frameName.toStdString(), "", icd::FrameType::data,
      icd::ByteOrder::little_endian);
  frame->add_root(std::move(node));
  repo.add_frame(std::move(frame));

  return repo;
}

// ══════════════════════════════════════════════════════════════════════════════
// Helper: register a signal in SignalRegistry and return its UUID
// ══════════════════════════════════════════════════════════════════════════════

static QString registerSignal(SignalRegistry& registry,
                               const QString& deviceId,
                               const QString& portName,
                               const QString& frameName,
                               const QString& nodePath) {
  registry.registerDevice(deviceId, deviceId);
  registry.bindPortToFrames(deviceId, portName, {frameName});
  registry.registerSignals({{deviceId, portName, frameName, nodePath}});
  return SignalRegistry::computeUuid(deviceId, portName, frameName, nodePath);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test fixture that sets up common dependencies for CAN-based tests
// ══════════════════════════════════════════════════════════════════════════════

class StepRunnerCanTest : public ::testing::Test {
 protected:
  void SetUp() override {
    icdRepo_ = buildTestIcdRepo("TestFrame", 8);
    uuid_ = registerSignal(registry_, "dev-can", "port1", "TestFrame",
                           "TestNode");
    resolver_ = std::make_unique<SignalResolver>(&registry_, &icdRepo_);

    mockCAN_ = new StepRunnerMockCAN();
    mockCAN_->opened_ = true;
    HardwareManagerTestHelper::injectDevice(hm_, "dev-can", mockCAN_);

    runner_ = std::make_unique<StepRunner>(&hm_, &codec_, resolver_.get());
  }

  void TearDown() override {
    // runner_ must be destroyed before hm_ (runner_ holds pointers but not
    // ownership)
    runner_.reset();
    resolver_.reset();
    hm_.shutdown();
    // mockCAN_ was injected into hm_.device_pool_, which is cleared but the
    // pointer is not deleted by HardwareManager. We must delete it to avoid a
    // leak.
    delete mockCAN_;
  }

  // Dependencies
  SignalRegistry registry_;
  icd::Repository icdRepo_;
  HardwareManager hm_;
  SignalCodec codec_;
  std::unique_ptr<SignalResolver> resolver_;
  std::unique_ptr<StepRunner> runner_;
  StepRunnerMockCAN* mockCAN_ = nullptr;
  QString uuid_;
};

// ══════════════════════════════════════════════════════════════════════════════
// Tests
// ══════════════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────────────
// Test 1: executeSingleStep SET → hw writeFrame called, PASS returned
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, SetCommandReturnsPass) {
  TestStepData step;
  step.command = "SET";
  step.target = uuid_;
  step.value = 0x55;

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, step, 0, "1.1");

  EXPECT_EQ(result.status, PASS);
  EXPECT_EQ(result.command, "SET");
  // Frame-type SET: encodeToFrame with bitWidth=8, LE, value=0x55
  // → one byte [0x55]
  ASSERT_EQ(mockCAN_->last_data_.size(), 1);
  EXPECT_EQ(static_cast<uint8_t>(mockCAN_->last_data_[0]), 0x55);
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 2: executeSingleStep CHECK → PASS when value matches, FAIL when mismatch
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, CheckCommandPassOnMatch) {
  TestStepData step;
  step.command = "CHECK";
  step.target = uuid_;
  // mockCAN.receiveMessage returns 0xAA as the first byte (bitWidth=8, LE)
  // 0xAA = 170.0
  step.value = 170.0;
  step.tolerance = 0.0;

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, step, 0, "1.1");

  EXPECT_EQ(result.status, PASS);
  EXPECT_DOUBLE_EQ(result.actualValue, 170.0);
}

TEST_F(StepRunnerCanTest, CheckCommandFailOnMismatch) {
  TestStepData step;
  step.command = "CHECK";
  step.target = uuid_;
  step.value = 100.0;
  step.tolerance = 0.0;

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, step, 0, "1.1");

  EXPECT_EQ(result.status, FAIL);
  EXPECT_DOUBLE_EQ(result.actualValue, 170.0);
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 3: executeSingleStep DELAY → waits correct duration
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, DelayWaitsCorrectDuration) {
  TestStepData step;
  step.command = "DELAY";
  step.extra = "100";

  QElapsedTimer timer;
  timer.start();
  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, step, 0, "1.1");
  int elapsed = static_cast<int>(timer.elapsed());

  EXPECT_EQ(result.status, PASS);
  EXPECT_GE(elapsed, 90);
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 4: executeSingleStep LOOP → iterates correct count
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, LoopIteratesCorrectCount) {
  TestStepData delayStep;
  delayStep.command = "DELAY";
  delayStep.extra = "10";

  TestStepData loopStep;
  loopStep.command = "LOOP";
  loopStep.loopCount = 3;
  loopStep.subSteps.append(delayStep);

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, loopStep, 0, "1.1");

  EXPECT_EQ(result.status, PASS);
  ASSERT_EQ(result.iterations.size(), 3);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(result.iterations[i].iteration, i);
    ASSERT_EQ(result.iterations[i].subSteps.size(), 1);
    EXPECT_EQ(result.iterations[i].subSteps[0].status, PASS);
    EXPECT_EQ(result.iterations[i].subSteps[0].command, "DELAY");
  }
  EXPECT_TRUE(result.message.contains("3 iterations"));
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 5: executeSingleStep IF → then / else branch execution
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, IfThenBranchExecutedOnTrue) {
  // mockCAN returns 0xAA (=170), condition "> 100" → true
  TestStepData thenStep;
  thenStep.command = "DELAY";
  thenStep.extra = "10";

  TestStepData elseStep;
  elseStep.command = "DELAY";
  elseStep.extra = "20";

  TestStepData ifStep;
  ifStep.command = "IF";
  ifStep.target = uuid_;
  ifStep.condition = "> 100";
  ifStep.thenSteps.append(thenStep);
  ifStep.elseSteps.append(elseStep);

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, ifStep, 0, "1.1");

  EXPECT_EQ(result.status, PASS);
  EXPECT_TRUE(result.branches.conditionMet);
  EXPECT_EQ(result.branches.thenSteps.size(), 1);
  EXPECT_EQ(result.branches.elseSteps.size(), 0);
  EXPECT_EQ(result.branches.thenSteps[0].command, "DELAY");
}

TEST_F(StepRunnerCanTest, IfElseBranchExecutedOnFalse) {
  // mockCAN returns 0xAA (=170), condition "< 50" → false
  TestStepData thenStep;
  thenStep.command = "DELAY";
  thenStep.extra = "10";

  TestStepData elseStep;
  elseStep.command = "DELAY";
  elseStep.extra = "20";

  TestStepData ifStep;
  ifStep.command = "IF";
  ifStep.target = uuid_;
  ifStep.condition = "< 50";
  ifStep.thenSteps.append(thenStep);
  ifStep.elseSteps.append(elseStep);

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, ifStep, 0, "1.1");

  EXPECT_EQ(result.status, PASS);
  EXPECT_FALSE(result.branches.conditionMet);
  EXPECT_EQ(result.branches.thenSteps.size(), 0);
  EXPECT_EQ(result.branches.elseSteps.size(), 1);
  EXPECT_EQ(result.branches.elseSteps[0].command, "DELAY");
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 6: Cancel → steps marked SKIPPED
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, CancelMarksStepAsSkipped) {
  runner_->cancel();

  TestStepData step;
  step.command = "SET";
  step.target = uuid_;
  step.value = 1.0;

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, step, 0, "1.1");

  EXPECT_EQ(result.status, SKIPPED);
  EXPECT_TRUE(result.message.contains("Cancelled"));
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 7: UUID not found → ERROR status
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, UuidNotFoundReturnsError) {
  TestStepData step;
  step.command = "SET";
  step.target = "nonexistent-uuid-12345678";
  step.value = 1.0;

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, step, 0, "1.1");

  EXPECT_EQ(result.status, ERROR);
  EXPECT_TRUE(result.message.contains("Signal not found"));
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 8: execDelay with negative value → uses default (no crash, sleeps)
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, DelayNegativeUsesDefault) {
  TestStepData step;
  step.command = "DELAY";
  step.extra = "-1";

  QElapsedTimer timer;
  timer.start();
  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, step, 0, "1.1");
  int elapsed = static_cast<int>(timer.elapsed());

  EXPECT_EQ(result.status, PASS);
  // Default delay is 1000ms when extra is negative
  EXPECT_GE(elapsed, 900);
  EXPECT_TRUE(result.message.contains("Delayed"));
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 9: executeProgram emits lifecycle signals correctly
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, ExecuteProgramEmitsLifecycleSignals) {
  bool suiteStarted = false;
  bool suiteFinished = false;
  int caseStartedCount = 0;
  int caseFinishedCount = 0;
  int stepStartedCount = 0;
  int stepFinishedCount = 0;
  int progressCount = 0;

  QObject::connect(runner_.get(), &StepRunner::suiteStarted,
                   [&](const QString& name) {
                     suiteStarted = true;
                     EXPECT_EQ(name, "TestSuite");
                   });
  QObject::connect(runner_.get(), &StepRunner::suiteFinished,
                   [&](const QString& name, int pass, int fail) {
                     suiteFinished = true;
                     EXPECT_EQ(name, "TestSuite");
                     EXPECT_EQ(pass, 1);
                     EXPECT_EQ(fail, 0);
                   });
  QObject::connect(runner_.get(), &StepRunner::caseStarted,
                   [&](int /*idx*/, const QString& /*n*/) {
                     ++caseStartedCount;
                   });
  QObject::connect(runner_.get(), &StepRunner::caseFinished,
                   [&](int /*idx*/, const QString& /*n*/, int /*r*/) {
                     ++caseFinishedCount;
                   });
  QObject::connect(
      runner_.get(), &StepRunner::stepStarted,
      [&](int /*ci*/, const QString& /*sp*/, const QString& /*cmd*/,
          const QString& /*tgt*/) { ++stepStartedCount; });
  QObject::connect(
      runner_.get(), &StepRunner::stepFinished,
      [&](int /*ci*/, const QString& /*sp*/, const StepResult& /*r*/) {
        ++stepFinishedCount;
      });
  QObject::connect(runner_.get(), &StepRunner::progressUpdated,
                   [&](int /*cur*/, int /*tot*/) { ++progressCount; });

  TestStepData step;
  step.command = "DELAY";
  step.extra = "10";

  ProgramData program;
  program.suiteName = "TestSuite";
  TestCaseData tc;
  tc.caseName = "Case1";
  tc.steps.append(step);
  program.cases.append(tc);

  runner_->executeProgram(program);

  EXPECT_TRUE(suiteStarted);
  EXPECT_TRUE(suiteFinished);
  EXPECT_EQ(caseStartedCount, 1);
  EXPECT_EQ(caseFinishedCount, 1);
  EXPECT_EQ(stepStartedCount, 1);
  EXPECT_EQ(stepFinishedCount, 1);
  EXPECT_EQ(progressCount, 1);
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 10: Unknown command → ERROR status
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, UnknownCommandReturnsError) {
  TestStepData step;
  step.command = "INVALID_CMD";
  step.target = uuid_;

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, step, 0, "1.1");

  EXPECT_EQ(result.status, ERROR);
  EXPECT_TRUE(result.message.contains("Unknown command"));
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 11: Delays in LOOP sub-steps accumulate (timing integration)
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, LoopAccumulatesSubStepDelays) {
  TestStepData delayStep;
  delayStep.command = "DELAY";
  delayStep.extra = "30";

  TestStepData loopStep;
  loopStep.command = "LOOP";
  loopStep.loopCount = 3;
  loopStep.subSteps.append(delayStep);

  QElapsedTimer timer;
  timer.start();
  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, loopStep, 0, "1.1");
  int elapsed = static_cast<int>(timer.elapsed());

  EXPECT_EQ(result.status, PASS);
  // 3 iterations * 30ms ≈ 90ms
  EXPECT_GE(elapsed, 70);

  ASSERT_EQ(result.iterations.size(), 3);
  for (const auto& iter : result.iterations) {
    ASSERT_EQ(iter.subSteps.size(), 1);
    EXPECT_EQ(iter.subSteps[0].status, PASS);
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 12: CHECK with tolerance succeeds within range
// ──────────────────────────────────────────────────────────────────────────────
TEST_F(StepRunnerCanTest, CheckCommandPassWithinTolerance) {
  TestStepData step;
  step.command = "CHECK";
  step.target = uuid_;
  step.value = 168.0;  // actual=170, tolerance=5 → |170-168|=2 ≤ 5 → PASS
  step.tolerance = 5.0;

  StepResult result =
      StepRunnerTestHelper::executeSingleStep(*runner_, step, 0, "1.1");

  EXPECT_EQ(result.status, PASS);
  EXPECT_DOUBLE_EQ(result.actualValue, 170.0);
}
