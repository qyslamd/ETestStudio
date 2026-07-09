#include "TestExecutionEngine.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QThread>

#include <spdlog/spdlog.h>

#include "HardwareManager.h"
#include "ResultCollector.h"
#include "SignalCodec.h"
#include "SignalResolver.h"
#include "StepRunner.h"

namespace etest::engine {

// ==========================================================================
// 构造 / 析构
// ==========================================================================

TestExecutionEngine::TestExecutionEngine(etest::core::SignalRegistry* registry,
                                          icd::Repository* icdRepo,
                                          QObject* parent)
    : QObject(parent)
    , signal_registry_(registry)
    , icd_repository_(icdRepo) {
    // 注册元类型（必须在使用 QMetaObject::invokeMethod / 跨线程信号前完成）
    // Qt moc 生成的信号参数类型名是 namespace 内的短名称 "StepResult"
    // 因此必须同时注册短名称和完整名称以匹配 Qt 的内部查找
    qRegisterMetaType<EngineState>("etest::engine::EngineState");
    qRegisterMetaType<StepResult>("StepResult");
    qRegisterMetaType<StepResult>("etest::engine::StepResult");
    qRegisterMetaType<ProgramData>("ProgramData");
    qRegisterMetaType<ProgramData>("etest::engine::ProgramData");
    initEngine();
}

TestExecutionEngine::~TestExecutionEngine() {
    stop();
}

// ==========================================================================
// 配置
// ==========================================================================

void TestExecutionEngine::setProgram(const ProgramData& program) {
    current_program_ = program;

    // 计算总步骤数
    total_steps_ = 0;
    for (const auto& tc : program.cases) {
        total_steps_ += tc.steps.size();
    }
}

void TestExecutionEngine::setTopologyDoc(const QJsonObject& topologyDoc) {
    topology_doc_ = topologyDoc;
}

bool TestExecutionEngine::loadTopology(const QString& etopoPath) {
    QFile file(etopoPath);
    if (!file.open(QIODevice::ReadOnly)) {
        spdlog::error("[TestExecutionEngine] Cannot open topology: {}",
                      etopoPath.toStdString());
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        spdlog::error("[TestExecutionEngine] JSON parse error: {}",
                      parseError.errorString().toStdString());
        return false;
    }

    if (!doc.isObject()) {
        spdlog::error("[TestExecutionEngine] Topology root is not an object");
        return false;
    }

    topology_doc_ = doc.object();

    // 如果有 HardwareManager，加载拓扑
    if (hw_manager_) {
        return hw_manager_->loadFromTopology(etopoPath);
    }

    return true;
}

// ==========================================================================
// 生命周期
// ==========================================================================

bool TestExecutionEngine::start() {
    EngineState expected = EngineState::Idle;
    if (!state_.compare_exchange_strong(expected, EngineState::Running)) {
        spdlog::warn("[TestExecutionEngine] start() ignored: state is not Idle");
        return false;
    }

    // 清理上一次运行残留（含线程等待）
    cleanupRunner();

    // 创建执行组件
    startExecution();

    // 将 runner 移动到工作线程
    runner_->moveToThread(&worker_thread_);

    // 连接 StepRunner 信号 → 引擎代理信号
    // 跨线程时 Qt 自动使用 QueuedConnection
    connect(runner_.get(), &StepRunner::suiteStarted,
            this, &TestExecutionEngine::suiteStarted);
    connect(runner_.get(), &StepRunner::suiteFinished,
            this, &TestExecutionEngine::suiteFinished);
    connect(runner_.get(), &StepRunner::suiteFinished,
            this, &TestExecutionEngine::onWorkerFinished);
    connect(runner_.get(), &StepRunner::caseStarted,
            this, &TestExecutionEngine::caseStarted);
    connect(runner_.get(), &StepRunner::caseFinished,
            this, &TestExecutionEngine::caseFinished);
    connect(runner_.get(), &StepRunner::stepStarted,
            this, &TestExecutionEngine::stepStarted);
    connect(runner_.get(), &StepRunner::stepFinished,
            this, &TestExecutionEngine::stepFinished);
    connect(runner_.get(), &StepRunner::engineError,
            this, &TestExecutionEngine::engineError);

    // progressUpdated 同时转发信号和更新内部计数
    connect(runner_.get(), &StepRunner::progressUpdated,
            this, [this](int current, int total) {
                completed_steps_ = current;
                emit progressUpdated(current, total);
            });

    // 线程启动回调
    connect(&worker_thread_, &QThread::started,
            this, &TestExecutionEngine::onThreadStarted);

    emit engineStateChanged(EngineState::Running);
    emit engineStarted();

    // 启动工作线程并调用 executeProgram
    worker_thread_.start();

    bool invoked = QMetaObject::invokeMethod(
        runner_.get(), "executeProgram", Qt::QueuedConnection,
        Q_ARG(ProgramData, current_program_));
    if (!invoked) {
        spdlog::error(
            "[TestExecutionEngine] Failed to invoke executeProgram on runner");
        state_.store(EngineState::Error);
        emit engineError(QStringLiteral("Failed to start test execution"));
        emit engineStateChanged(EngineState::Error);
        return false;
    }

    return true;
}

void TestExecutionEngine::stop() {
    EngineState prev = state_.exchange(EngineState::Idle);
    if (prev == EngineState::Idle) {
        return;
    }

    // 通知 runner 取消执行
    if (runner_) {
        runner_->cancel();
    }

    // 停止工作线程
    if (worker_thread_.isRunning()) {
        worker_thread_.quit();
        if (!worker_thread_.wait(5000)) {
            spdlog::warn(
                "[TestExecutionEngine] Worker thread did not stop within 5s");
        }
    }

    // 清理线程中的对象
    cleanupRunner();

    emit engineFinished();
    emit engineStateChanged(EngineState::Idle);
}

void TestExecutionEngine::pause() {
    EngineState expected = EngineState::Running;
    if (!state_.compare_exchange_strong(expected, EngineState::Paused)) {
        return;
    }
    if (runner_) {
        runner_->pause();
    }
    emit engineStateChanged(EngineState::Paused);
}

void TestExecutionEngine::resume() {
    EngineState expected = EngineState::Paused;
    if (!state_.compare_exchange_strong(expected, EngineState::Running)) {
        return;
    }
    if (runner_) {
        runner_->resume();
    }
    emit engineStateChanged(EngineState::Running);
}

// ==========================================================================
// 结果 / 进度
// ==========================================================================

void TestExecutionEngine::saveReport(const QString& etlogPath) {
    if (collector_) {
        collector_->saveToFile(etlogPath);
    } else {
        spdlog::warn("[TestExecutionEngine] No collector to save report");
    }
}

int TestExecutionEngine::totalSteps() const {
    return total_steps_;
}

int TestExecutionEngine::completedSteps() const {
    return completed_steps_;
}

// ==========================================================================
// 私有槽函数
// ==========================================================================

void TestExecutionEngine::onWorkerFinished() {
    // 只有当状态仍为 Running 时才执行完成逻辑。
    // 如果 stop() 已先将状态置为 Idle，则跳过以防止重复 emit。
    EngineState expected = EngineState::Running;
    if (!state_.compare_exchange_strong(expected, EngineState::Idle)) {
        return;
    }

    // 请求工作线程退出（如果尚未退出）
    worker_thread_.quit();

    emit engineFinished();
    emit engineStateChanged(EngineState::Idle);
}

void TestExecutionEngine::onThreadStarted() {
    spdlog::debug("[TestExecutionEngine] Worker thread started");
}

// ==========================================================================
// 私有辅助函数
// ==========================================================================

void TestExecutionEngine::initEngine() {
    resolver_ = std::make_unique<SignalResolver>(signal_registry_,
                                                  icd_repository_);
    codec_ = std::make_unique<SignalCodec>();
    hw_manager_ = std::make_unique<HardwareManager>();
}

void TestExecutionEngine::startExecution() {
    collector_ = std::make_unique<ResultCollector>();

    // StepRunner 不能在创建时指定父对象为工作线程，
    // 因为稍后要通过 moveToThread 移动它。
    runner_ = std::make_unique<StepRunner>(
        hw_manager_.get(), codec_.get(), resolver_.get(), nullptr);

    // 将 collector 连接到 runner 的信号（同一线程内直连）
    collector_->attach(runner_.get());
}

void TestExecutionEngine::cleanupRunner() {
    // 清理前确保工作线程已停止
    if (worker_thread_.isRunning()) {
        worker_thread_.quit();
        if (!worker_thread_.wait(5000)) {
            spdlog::warn(
                "[TestExecutionEngine] Worker thread did not stop within 5s");
        }
    }

    if (runner_) {
        // 工作线程已停止，可以直接销毁 runner（无需 moveToThread）
        runner_.reset();
    }
    collector_.reset();
}

}  // namespace etest::engine
