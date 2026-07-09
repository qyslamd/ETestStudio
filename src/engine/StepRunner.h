#ifndef ETEST_ENGINE_STEP_RUNNER_H_
#define ETEST_ENGINE_STEP_RUNNER_H_

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <atomic>

namespace etest::engine {

class HardwareManager;
class SignalCodec;
class SignalResolver;
struct ResolvedSignal;

// ── Command types ──
enum class CommandType { SET, CHECK, VERIFY, WAIT, DELAY, LOOP, WHILE, IF, UNKNOWN };

// ── Step status ──
enum StepStatus { PENDING, PASS, FAIL, TIMEOUT, ERROR, SKIPPED };

// ── Forward declarations ──
struct StepResult;

// ── Branch result for IF/ELSE ──
struct BranchesResult {
    bool conditionMet = false;
    QList<StepResult> thenSteps;
    QList<StepResult> elseSteps;
};

// ── Iteration result for LOOP/WHILE ──
struct IterationResult {
    int iteration = 0;
    QList<StepResult> subSteps;
};

// ── Step data (self-contained; does NOT depend on test_program/TestProgramData.h) ──
struct TestStepData {
    QString command;                        // "SET", "CHECK", etc.
    QString target;                         // UUID hex
    double value = 0.0;                     // 设定值/期望值
    double tolerance = 0.0;                 // 容差
    QString extra;                          // 额外参数（WAIT条件, DELAY ms等）
    int timeoutMs = 0;                      // 超时ms（0=使用全局默认值）
    int loopCount = 0;                      // LOOP/WHILE 最大迭代次数
    QString condition;                      // IF/WHILE 条件表达式
    QList<TestStepData> subSteps;           // LOOP/WHILE 的子步骤
    QList<TestStepData> thenSteps;          // IF 的 then 分支
    QList<TestStepData> elseSteps;          // IF 的 else 分支
};

// ── Test case data ──
struct TestCaseData {
    QString caseName;
    QList<TestStepData> steps;
};

// ── Program data ──
struct ProgramData {
    QString suiteName;
    QList<TestCaseData> cases;
};

// ── Step result with fluent API ──
struct StepResult {
    QString stepPath;
    QString command;
    QString target;
    StepStatus status = PENDING;
    QString message;
    double actualValue = 0.0;
    double expectedValue = 0.0;
    int elapsedMs = 0;
    QDateTime timestamp;
    QList<IterationResult> iterations;      // LOOP/WHILE
    BranchesResult branches;                // IF

    StepResult& setStatus(StepStatus s) { status = s; return *this; }
    StepResult& setMessage(const QString& m) { message = m; return *this; }
};

// ══════════════════════════════════════════════════════════════════════════════
// StepRunner — command dispatch engine for test execution
// ══════════════════════════════════════════════════════════════════════════════
class StepRunner : public QObject {
    Q_OBJECT

 public:
    StepRunner(HardwareManager* hw, SignalCodec* codec,
               SignalResolver* resolver, QObject* parent = nullptr);

    Q_INVOKABLE void executeProgram(const ProgramData& program);
    void cancel();
    void pause();
    void resume();
    void resetCancel();
    bool isCancelling() const;
    bool isPaused() const;

    static constexpr int kDefaultTimeoutMs = 30000;

 signals:
    void suiteStarted(const QString& name);
    void suiteFinished(const QString& name, int pass, int fail);
    void caseStarted(int index, const QString& name);
    void caseFinished(int index, const QString& name, int result);
    void stepStarted(int caseIndex, const QString& stepPath,
                     const QString& command, const QString& target);
    void stepFinished(int caseIndex, const QString& stepPath,
                      const StepResult& result);
    void engineError(const QString& msg);
    void progressUpdated(int current, int total);

 private:
    StepResult execSet(const TestStepData& step, const ResolvedSignal& signal);
    StepResult execCheck(const TestStepData& step, const ResolvedSignal& signal);
    StepResult execVerify(const TestStepData& step, const ResolvedSignal& signal);
    StepResult execWait(const TestStepData& step, const ResolvedSignal& signal);
    StepResult execDelay(const TestStepData& step);
    StepResult execLoop(const TestStepData& step, int caseIndex, const QString& stepPath);
    StepResult execWhile(const TestStepData& step, int caseIndex, const QString& stepPath);
    StepResult execIf(const TestStepData& step, int caseIndex, const QString& stepPath);
    StepResult executeSingleStep(const TestStepData& step, int caseIndex, const QString& stepPath);
    CommandType commandType(const QString& cmd) const;
    bool evaluateCondition(const QString& condition, const ResolvedSignal& signal) const;

    HardwareManager* hw_;
    SignalCodec* codec_;
    SignalResolver* resolver_;
    std::atomic<int> cancel_flag_{0};

    // 允许测试辅助类访问私有方法
    friend class StepRunnerTestHelper;
};

}  // namespace etest::engine

Q_DECLARE_METATYPE(etest::engine::StepResult)
Q_DECLARE_METATYPE(etest::engine::ProgramData)

#endif  // ETEST_ENGINE_STEP_RUNNER_H_
