#include "TestExecutionEngine.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QThread>

#include <spdlog/spdlog.h>

#include "logger/Logger.h"

#include "plugin_sdk/IPlugin.h"
#include "plugin_sdk/PluginManager.h"

#include "HardwareManager.h"
#include "MockUUTBuilder.h"
#include "ResultCollector.h"
#include "SignalCodec.h"
#include "SignalResolver.h"
#include "StepRunner.h"

namespace etest::engine {

// ==========================================================================
// 静态辅助：Mock 一致性校验
// ==========================================================================

static bool deviceIsMock(const QJsonObject& deviceObj) {
    QString pluginId = deviceObj["pluginId"].toString();
    if (pluginId.isEmpty()) {
        return false;
    }
    auto* plugin = etest::core::plugin::PluginManager::instance().plugin(pluginId);
    if (!plugin) {
        return false;
    }
    return plugin->metaData().is_mock;
}

static bool hasMockDevices(const QJsonObject& root) {
    QJsonArray devices = root["devices"].toArray();
    for (const auto& d : devices) {
        if (deviceIsMock(d.toObject())) {
            return true;
        }
    }
    return false;
}

static bool checkMockConsistency(const QJsonObject& root) {
    QJsonArray devices = root["devices"].toArray();
    if (devices.isEmpty()) {
        return true;
    }

    bool firstMock = deviceIsMock(devices[0].toObject());
    for (const auto& d : devices) {
        if (deviceIsMock(d.toObject()) != firstMock) {
            return false;
        }
    }
    return true;
}

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
    LOG_INFO("ENGINE", "设置测试程序 [cases={}]", program.cases.size());
    current_program_ = program;

    // 计算总步骤数
    total_steps_ = 0;
    for (const auto& tc : program.cases) {
        total_steps_ += tc.steps.size();
    }
}

void TestExecutionEngine::setRegistry(etest::core::SignalRegistry* registry,
                                       icd::Repository* icdRepo) {
    signal_registry_ = registry;
    icd_repository_ = icdRepo;
    // 重新创建 Resolver 使新指针生效
    resolver_ = std::make_unique<SignalResolver>(signal_registry_,
                                                  icd_repository_);
}

void TestExecutionEngine::setTopologyDoc(const QJsonObject& topologyDoc) {
    topology_doc_ = topologyDoc;
}

void TestExecutionEngine::clearTopologyState() {
    topology_doc_ = QJsonObject();
    if (hw_manager_) {
        hw_manager_->closeAllDevices();
    }
    // MonitorManager 由外部持有，其状态清理不在此处理（见 ExecutionPanelController::clearMonitorState）
}

bool TestExecutionEngine::loadTopology(const QString& etopoPath) {
    QFile file(etopoPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR("ENGINE", "[TestExecutionEngine] Cannot open topology: {}",
                      etopoPath.toStdString());
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR("ENGINE", "[TestExecutionEngine] JSON parse error: {}",
                      parseError.errorString().toStdString());
        return false;
    }

    if (!doc.isObject()) {
        LOG_ERROR("ENGINE", "[TestExecutionEngine] Topology root is not an object");
        return false;
    }

    topology_doc_ = doc.object();
    LOG_INFO("ENGINE", "加载拓扑 [path={}]", etopoPath.toStdString());
    const QJsonObject& root = topology_doc_;

    // ── 校验 mock 一致性 ──
    if (!checkMockConsistency(root)) {
        LOG_ERROR("ENGINE", "[TestExecutionEngine] 拓扑中不能混合 Mock 设备和真实设备");
        emit engineError(QStringLiteral("拓扑中不能混合 Mock 设备和真实设备，"
                                        "请检查设备选用的插件类型"));
        return false;
    }

    // ── 加载所有设备（传入 QJsonObject 单次解析） ──
    if (!hw_manager_) {
        LOG_ERROR("ENGINE", "[TestExecutionEngine] HardwareManager 未初始化");
        return false;
    }
    bool ok = hw_manager_->loadFromTopology(root);

    // ── 有 Mock 设备 → 创建 MockUUT ──
    if (ok && hasMockDevices(root)) {
        MockUUTBuilder builder(icd_repository_, root);
        QFileInfo fi(etopoPath);

        builder.loadResponseConfigFile(
            fi.absolutePath() + QStringLiteral("/MockResponses.emock"));

        std::vector<std::unique_ptr<MockUUT>> mockUUTs;
        if (!builder.buildAll(mockUUTs)) {
            LOG_ERROR("ENGINE", "[TestExecutionEngine] MockUUT 构建失败: {}",
                          builder.lastError().toStdString());
            emit engineError(
                QStringLiteral("MockUUT 构建失败: %1").arg(builder.lastError()));
            hw_manager_->closeAllDevices();  // 回滚已打开的设备
            return false;
        }

        size_t mockCount = mockUUTs.size();
        hw_manager_->setMockUUT(std::move(mockUUTs));
        LOG_INFO("ENGINE", "MockUUT 构建成功 [count={}]", mockCount);
    }

    // 监听器不再从拓扑加载（移出拓扑，存于 .etproj），
    // 由 ExecutionPanelController 在 loadTopology 成功后以 loadMonitors 注入。

    return ok;
}

// ==========================================================================
// 生命周期
// ==========================================================================

bool TestExecutionEngine::start() {
    LOG_INFO("ENGINE", "启动引擎");
    EngineState expected = EngineState::Idle;
    if (!state_.compare_exchange_strong(expected, EngineState::Running)) {
        LOG_WARN("ENGINE", "[TestExecutionEngine] start() ignored: state is not Idle");
        return false;
    }

    // 清理上一次运行残留（含线程等待）
    cleanupRunner();

    // 清空 MonitorManager 的运行时数据，避免上一轮 CVT/history 残留污染本轮
    if (monitor_manager_) {
        monitor_manager_->clearRuntime();
    }

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

    // ── MonitorManager 集成：监听硬件操作信号 ──
    // MonitorManager 由外部持有，此处通过指针连接（跨线程自动 QueuedConnection）
    if (monitor_manager_) {
        connect(runner_.get(), &StepRunner::hardwareOperationFinished,
                monitor_manager_, &MonitorManager::onHardwareOpFinished);
    }

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
        LOG_ERROR("ENGINE",
                  "Failed to invoke executeProgram on runner");
        state_.store(EngineState::Error);
        emit engineError(QStringLiteral("Failed to start test execution"));
        emit engineStateChanged(EngineState::Error);
        return false;
    }

    return true;
}

void TestExecutionEngine::stop() {
    LOG_INFO("ENGINE", "停止引擎");
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
            LOG_WARN("ENGINE",
                     "Worker thread did not stop within 5s");
        }
    }

    // 清理线程中的对象
    cleanupRunner();

    emit engineFinished();
    emit engineStateChanged(EngineState::Idle);
}

void TestExecutionEngine::pause() {
    LOG_INFO("ENGINE", "暂停引擎");
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
    LOG_INFO("ENGINE", "继续引擎");
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
        // 1. flush 监听器数据到 ResultCollector
        if (monitor_manager_) {
            collector_->setMonitorData(monitor_manager_->flushSamples());
        }
        // 2. 保存 .etlog（含 monitors[] 段）
        collector_->saveToFile(etlogPath);
    } else {
        LOG_WARN("ENGINE", "[TestExecutionEngine] No collector to save report");
    }
}

void TestExecutionEngine::flushMonitorData() {
    if (collector_ && monitor_manager_) {
        collector_->setMonitorData(monitor_manager_->flushSamples());
    }
}

void TestExecutionEngine::saveReportSegment(const QString& etlogPath,
                                             const QString& suiteName,
                                             int startCase,
                                             int caseCount) {
    if (!collector_) {
        LOG_WARN("ENGINE",
                 "[TestExecutionEngine] No collector to save segment report");
        return;
    }
    // 不 flush，依赖外部已调 flushMonitorData
    collector_->saveSegmentToFile(etlogPath, suiteName, startCase, caseCount);
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
    EngineState expected = EngineState::Running;
    if (!state_.compare_exchange_strong(expected, EngineState::Idle)) {
        return;
    }

    worker_thread_.quit();
    // 必须等待 worker 线程真正结束，否则 emit engineFinished 触发 destroyEngine
    // 析构引擎/runner 时，worker 线程可能还在执行 StepRunner 代码（如 spdlog 调用）
    // 导致访问已释放对象
    worker_thread_.wait(5000);

    emit engineFinished();
    emit engineStateChanged(EngineState::Idle);
}

void TestExecutionEngine::onThreadStarted() {
    LOG_DEBUG("ENGINE", "[TestExecutionEngine] Worker thread started");
}

// ==========================================================================
// 私有辅助函数
// ==========================================================================

void TestExecutionEngine::initEngine() {
    resolver_ = std::make_unique<SignalResolver>(signal_registry_,
                                                  icd_repository_);
    codec_ = std::make_unique<SignalCodec>();
    hw_manager_ = std::make_unique<HardwareManager>();
    // MonitorManager 由外部注入（setMonitorManager），不在 initEngine 中创建
}

void TestExecutionEngine::startExecution() {
    collector_ = std::make_unique<ResultCollector>();

    runner_ = std::make_unique<StepRunner>(
        hw_manager_.get(), codec_.get(), resolver_.get(), nullptr);

    collector_->attach(runner_.get());
}

void TestExecutionEngine::cleanupRunner() {
    if (worker_thread_.isRunning()) {
        worker_thread_.quit();
        if (!worker_thread_.wait(5000)) {
            LOG_WARN("ENGINE",
                     "Worker thread did not stop within 5s (destructor)");
        }
    }

    if (runner_) {
        runner_.reset();
    }
    collector_.reset();
}

}  // namespace etest::engine
