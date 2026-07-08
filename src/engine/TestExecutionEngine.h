#ifndef ETEST_ENGINE_TEST_EXECUTION_ENGINE_H_
#define ETEST_ENGINE_TEST_EXECUTION_ENGINE_H_

#include <QObject>
#include <QThread>
#include <QJsonObject>
#include <memory>
#include <atomic>

#include "StepRunner.h"

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace icd {
class Repository;
}  // namespace icd

namespace etest::engine {

class SignalResolver;
class SignalCodec;
class HardwareManager;
class ResultCollector;

enum class EngineState {
    Idle,
    Running,
    Paused,
    Finished,
    Error
};

class TestExecutionEngine : public QObject {
    Q_OBJECT

public:
    explicit TestExecutionEngine(etest::core::SignalRegistry* registry,
                                  icd::Repository* icdRepo,
                                  QObject* parent = nullptr);
    ~TestExecutionEngine() override;

    // -- 配置 --
    void setProgram(const ProgramData& program);
    void setTopologyDoc(const QJsonObject& topologyDoc);
    bool loadTopology(const QString& etopoPath);

    // -- 生命周期 --
    bool start();       // 启动工作线程开始执行
    void stop();        // 请求停止
    void pause();       // 暂停
    void resume();      // 继续
    EngineState state() const { return state_.load(); }

    // -- 结果 --
    void saveReport(const QString& etlogPath);

    // -- 进度 --
    int totalSteps() const;
    int completedSteps() const;

signals:
    void engineStarted();
    void engineFinished();
    void engineStateChanged(EngineState state);
    void engineError(const QString& message);

    // 代理信号（来自StepRunner，跨线程转发）
    void suiteStarted(const QString& name);
    void suiteFinished(const QString& name, int pass, int fail);
    void caseStarted(int idx, const QString& name);
    void caseFinished(int idx, const QString& name, int result);
    void stepStarted(int caseIndex, const QString& stepPath,
                     const QString& command, const QString& target);
    void stepFinished(int caseIndex, const QString& stepPath,
                      const StepResult& result);
    void progressUpdated(int current, int total);

private slots:
    void onWorkerFinished();
    void onThreadStarted();

private:
    void initEngine();
    void startExecution();
    void cleanupRunner();

    // 拥有的组件（在initEngine中创建）
    std::unique_ptr<SignalResolver> resolver_;
    std::unique_ptr<SignalCodec> codec_;
    std::unique_ptr<HardwareManager> hw_manager_;
    std::unique_ptr<StepRunner> runner_;
    std::unique_ptr<ResultCollector> collector_;

    // 外部依赖（注入，不拥有）
    etest::core::SignalRegistry* signal_registry_;
    icd::Repository* icd_repository_;

    // 数据
    ProgramData current_program_;
    QJsonObject topology_doc_;
    int total_steps_ = 0;
    int completed_steps_ = 0;

    // 线程
    QThread worker_thread_;
    std::atomic<EngineState> state_{EngineState::Idle};
};

}  // namespace etest::engine

Q_DECLARE_METATYPE(etest::engine::EngineState)

#endif  // ETEST_ENGINE_TEST_EXECUTION_ENGINE_H_
