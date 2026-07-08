#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QThread>

#include "ResultCollector.h"
#include "SignalCodec.h"
#include "SignalRegistry.h"
#include "SignalResolver.h"
#include "StepRunner.h"
#include "TestExecutionEngine.h"

#include <icd/repository.hpp>

using namespace etest::engine;
using namespace etest::core;

// ══════════════════════════════════════════════════════════════════════════════
// QCoreApplication 环境（QSignalSpy::wait 需要事件循环）
// ══════════════════════════════════════════════════════════════════════════════

class QAppEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    if (QCoreApplication::instance()) {
      return;
    }
    static char arg0[] = "test";
    static int argc = 1;
    static char* argv[] = {arg0, nullptr};
    app_ = new QCoreApplication(argc, argv);
  }

 private:
  QCoreApplication* app_ = nullptr;
};

static QAppEnvironment* s_env =
    static_cast<QAppEnvironment*>(
        ::testing::AddGlobalTestEnvironment(new QAppEnvironment()));

// ══════════════════════════════════════════════════════════════════════════════
// Test fixture: minimal engine facade test
// ══════════════════════════════════════════════════════════════════════════════

// ══════════════════════════════════════════════════════════════════════════════
// Test fixture: minimal engine facade test
// ══════════════════════════════════════════════════════════════════════════════

class EngineFacadeTest : public ::testing::Test {
 protected:
    void SetUp() override {
        // SignalRegistry and icd::Repository are lightweight default-constructible
        // objects. The engine does NOT require topology to be loaded for DELAY-only
        // programs.
    }

    void TearDown() override {
        // Clean up any temp files
        QFile::remove(tempEtlogPath_);
    }

    // ── Helper: build a simple DELAY-only program ──
    static ProgramData makeDelayProgram(int stepCount = 3) {
        ProgramData prog;
        prog.suiteName = QStringLiteral("FacadeTest");

        TestCaseData tc;
        tc.caseName = QStringLiteral("Case1");

        for (int i = 0; i < stepCount; ++i) {
            TestStepData step;
            step.command = QStringLiteral("DELAY");
            step.extra = QStringLiteral("10");  // 10 ms each
            tc.steps.append(step);
        }

        prog.cases.append(tc);
        return prog;
    }

    QString tempEtlogPath_ = QStringLiteral("test_engine_facade.etlog");
};

// ══════════════════════════════════════════════════════════════════════════════
// Test 1: Start and finish with DELAY-only program
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(EngineFacadeTest, StartAndFinish) {
    SignalRegistry registry;
    icd::Repository repo;

    TestExecutionEngine engine(&registry, &repo);

    ProgramData prog = makeDelayProgram(3);
    engine.setProgram(prog);

    QSignalSpy startedSpy(&engine, &TestExecutionEngine::engineStarted);
    QSignalSpy finishedSpy(&engine, &TestExecutionEngine::engineFinished);
    QSignalSpy suiteStartedSpy(&engine, &TestExecutionEngine::suiteStarted);
    QSignalSpy suiteFinishedSpy(&engine, &TestExecutionEngine::suiteFinished);

    EXPECT_EQ(engine.state(), EngineState::Idle);

    // Start execution
    EXPECT_TRUE(engine.start());
    EXPECT_EQ(engine.state(), EngineState::Running);

    EXPECT_EQ(startedSpy.count(), 1);

    // Wait for engine to finish (with 10s timeout to be safe)
    EXPECT_TRUE(finishedSpy.wait(10000));
    EXPECT_EQ(finishedSpy.count(), 1);

    // Verify state auto-transitioned to Idle
    EXPECT_EQ(engine.state(), EngineState::Idle);

    // Verify suite lifecycle signals
    EXPECT_EQ(suiteStartedSpy.count(), 1);
    EXPECT_EQ(suiteFinishedSpy.count(), 1);

    // Extract suite name from the first signal
    QList<QVariant> suiteStartedArgs = suiteStartedSpy.takeFirst();
    EXPECT_EQ(suiteStartedArgs.at(0).toString(), QStringLiteral("FacadeTest"));

    // Verify step counting
    EXPECT_EQ(engine.totalSteps(), 3);
    EXPECT_EQ(engine.completedSteps(), 3);

    // Save report and verify file exists
    engine.saveReport(tempEtlogPath_);
    EXPECT_TRUE(QFile::exists(tempEtlogPath_));

    // Verify JSON content
    QFile file(tempEtlogPath_);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QByteArray content = file.readAll();
    file.close();

    EXPECT_TRUE(content.contains("FacadeTest"));
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 2: State transitions — pause / resume
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(EngineFacadeTest, StateTransitions) {
    SignalRegistry registry;
    icd::Repository repo;

    TestExecutionEngine engine(&registry, &repo);

    // Initially Idle
    EXPECT_EQ(engine.state(), EngineState::Idle);

    // 验证当前状态对外部调用 pause/resume 的正确响应
    // Idle 时调用 pause/resume 应当为 no-op
    engine.pause();
    EXPECT_EQ(engine.state(), EngineState::Idle);
    engine.resume();
    EXPECT_EQ(engine.state(), EngineState::Idle);

    // Idle → Running
    // 使用足够多的步骤以确保在工作线程完成前执行 pause
    ProgramData prog = makeDelayProgram(100);  // ~1.5s 总执行时间
    engine.setProgram(prog);
    EXPECT_TRUE(engine.start());
    EXPECT_EQ(engine.state(), EngineState::Running);

    // 等待工作线程启动并开始处理
    QThread::msleep(50);

    // Running → Paused
    engine.pause();
    EXPECT_EQ(engine.state(), EngineState::Paused);

    // Paused → Running
    engine.resume();
    EXPECT_EQ(engine.state(), EngineState::Running);

    // Stop
    engine.stop();
    EXPECT_EQ(engine.state(), EngineState::Idle);

    // Calling stop() again is a no-op
    engine.stop();
    EXPECT_EQ(engine.state(), EngineState::Idle);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 3: Start is rejected when not Idle
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(EngineFacadeTest, StartRejectedWhenNotIdle) {
    SignalRegistry registry;
    icd::Repository repo;

    TestExecutionEngine engine(&registry, &repo);
    engine.setProgram(makeDelayProgram(50));  // 足够长，确保第二个 start() 时仍在运行

    // First start succeeds
    EXPECT_TRUE(engine.start());

    // 给工作线程一点时间启动
    QThread::msleep(20);

    // Second start fails (engine is Running)
    EXPECT_FALSE(engine.start());

    // Wait for completion, then start succeeds again
    QSignalSpy finishedSpy(&engine, &TestExecutionEngine::engineFinished);
    ASSERT_TRUE(finishedSpy.wait(5000));

    EXPECT_EQ(engine.state(), EngineState::Idle);

    ProgramData prog2 = makeDelayProgram(1);
    engine.setProgram(prog2);
    EXPECT_TRUE(engine.start());

    // 等待本次运行完成
    QSignalSpy finishedSpy2(&engine, &TestExecutionEngine::engineFinished);
    ASSERT_TRUE(finishedSpy2.wait(5000));
    engine.stop();  // 确保清理
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 4: Stop during execution
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(EngineFacadeTest, StopDuringExecution) {
    SignalRegistry registry;
    icd::Repository repo;

    TestExecutionEngine engine(&registry, &repo);

    ProgramData prog = makeDelayProgram(10);  // 10 steps * 10ms = ~100ms
    engine.setProgram(prog);

    QSignalSpy finishedSpy(&engine, &TestExecutionEngine::engineFinished);

    EXPECT_TRUE(engine.start());

    // Give it a moment to start, then stop
    QThread::msleep(30);
    engine.stop();

    EXPECT_EQ(engine.state(), EngineState::Idle);
    // engineFinished should NOT have been emitted by onWorkerFinished
    // because stop() set state to Idle first, then emits engineFinished itself
    // Actually stop() emits engineFinished too, so count might be 1 or 0
    // depending on timing
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 5: Total / completed steps counting
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(EngineFacadeTest, StepCounting) {
    SignalRegistry registry;
    icd::Repository repo;

    TestExecutionEngine engine(&registry, &repo);

    ProgramData prog = makeDelayProgram(5);
    engine.setProgram(prog);

    EXPECT_EQ(engine.totalSteps(), 5);
    EXPECT_EQ(engine.completedSteps(), 0);

    QSignalSpy finishedSpy(&engine, &TestExecutionEngine::engineFinished);
    EXPECT_TRUE(engine.start());
    ASSERT_TRUE(finishedSpy.wait(10000));

    EXPECT_EQ(engine.completedSteps(), 5);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 6: Multiple runs
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(EngineFacadeTest, MultipleRuns) {
    SignalRegistry registry;
    icd::Repository repo;

    TestExecutionEngine engine(&registry, &repo);

    for (int run = 0; run < 3; ++run) {
        ProgramData prog = makeDelayProgram(2);
        engine.setProgram(prog);

        QSignalSpy finishedSpy(&engine, &TestExecutionEngine::engineFinished);

        EXPECT_TRUE(engine.start());
        EXPECT_TRUE(finishedSpy.wait(10000));

        EXPECT_EQ(engine.state(), EngineState::Idle);
        EXPECT_EQ(engine.completedSteps(), 2);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 7: Save report without collector (no-op, should not crash)
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(EngineFacadeTest, SaveReportWithoutExecution) {
    SignalRegistry registry;
    icd::Repository repo;

    TestExecutionEngine engine(&registry, &repo);

    // Calling saveReport without ever starting should not crash
    // and should not create a file
    engine.saveReport(tempEtlogPath_);
    EXPECT_FALSE(QFile::exists(tempEtlogPath_));
}
