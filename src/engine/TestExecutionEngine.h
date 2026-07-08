#ifndef ETEST_ENGINE_TEST_EXECUTION_ENGINE_H_
#define ETEST_ENGINE_TEST_EXECUTION_ENGINE_H_

#include <QObject>
#include <QThread>
#include <atomic>

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace icd {
class Repository;
}  // namespace icd

namespace etest::engine {

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

    // -- 生命周期 --
    bool start();       // 启动工作线程开始执行
    void stop();        // 请求停止
    void pause();       // 暂停
    void resume();      // 继续
    EngineState state() const { return state_; }

signals:
    void engineStarted();
    void engineFinished();
    void engineStateChanged(EngineState state);
    void engineError(const QString& message);

private:
    void initEngine();
    void cleanupEngine();

    etest::core::SignalRegistry* signal_registry_;
    icd::Repository* icd_repository_;
    QThread worker_thread_;
    std::atomic<EngineState> state_{EngineState::Idle};
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_TEST_EXECUTION_ENGINE_H_
