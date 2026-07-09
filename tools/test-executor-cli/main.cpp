/// @file main.cpp
/// @brief test-executor-cli standalone CLI engine — no Qt Widgets.
///        Drives TestExecutionEngine, outputs results to stdout.

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

#include <cstdio>

#include <spdlog/spdlog.h>

#include "engine/StepRunner.h"
#include "engine/TestExecutionEngine.h"
#include "core/SignalRegistry.h"

// =============================================================================
// Anonymous namespace: helpers
// =============================================================================
namespace {

/// Convert .etprog step JSON to a TestStepData (recursive).
etest::engine::TestStepData parseStepJson(const QJsonObject& obj) {
    etest::engine::TestStepData step;
    step.command = obj.value(QStringLiteral("cmd")).toString();
    step.target = obj.value(QStringLiteral("target")).toString();
    step.timeoutMs = obj.value(QStringLiteral("timeoutMs")).toInt(5000);

    // value is stored as a string in the .etprog JSON
    QString valStr = obj.value(QStringLiteral("value")).toString();
    if (!valStr.isEmpty()) {
        bool ok = false;
        double v = valStr.toDouble(&ok);
        if (ok) {
            step.value = v;
        }
    }

    // tolerance object (optional)
    QJsonValue tolVal = obj.value(QStringLiteral("tolerance"));
    if (tolVal.isObject()) {
        QJsonObject tolObj = tolVal.toObject();
        if (tolObj.value(QStringLiteral("enabled")).toBool(false)) {
            double tmax = tolObj.value(QStringLiteral("max")).toDouble(0.0);
            step.tolerance = tmax;
        }
    }

    // delayMs -> extra field (used by DELAY steps)
    int delayMs = obj.value(QStringLiteral("delayMs")).toInt(0);
    if (delayMs > 0) {
        step.extra = QString::number(delayMs);
    }

    // loop support
    step.loopCount = obj.value(QStringLiteral("loopCount")).toInt(0);

    // condition (IF / WHILE)
    QJsonValue condVal = obj.value(QStringLiteral("condition"));
    if (condVal.isObject()) {
        QJsonObject condObj = condVal.toObject();
        QString op = condObj.value(QStringLiteral("op")).toString();
        QString condTarget = condObj.value(QStringLiteral("target")).toString();
        QString condValue = condObj.value(QStringLiteral("value")).toString();

        // Condition string: "{op}{condValue}" so evaluateCondition can parse it
        if (!op.isEmpty() && !condValue.isEmpty()) {
            step.condition = op + condValue;
        } else if (!condValue.isEmpty()) {
            step.condition = QStringLiteral("==") + condValue;
        }
        // If condition has a target, store extra info in extra field
        if (!condTarget.isEmpty()) {
            if (!step.extra.isEmpty()) {
                step.extra += QStringLiteral("|target=") + condTarget;
            } else {
                step.extra = QStringLiteral("target=") + condTarget;
            }
        }
    }

    // Recursive: subSteps (LOOP/WHILE body, or IF body when then only)
    QJsonArray subArr = obj.value(QStringLiteral("subSteps")).toArray();
    for (const QJsonValue& sv : subArr) {
        step.subSteps.append(parseStepJson(sv.toObject()));
    }

    // Recursive: elseSubSteps (IF-else branch)
    QJsonArray elseArr = obj.value(QStringLiteral("elseSubSteps")).toArray();
    for (const QJsonValue& ev : elseArr) {
        step.elseSteps.append(parseStepJson(ev.toObject()));
    }

    // For IF commands, subSteps -> thenSteps, elseSubSteps -> elseSteps
    QString upperCmd = step.command.trimmed().toUpper();
    if (upperCmd == QStringLiteral("IF")) {
        step.thenSteps = step.subSteps;
        step.subSteps.clear();
    }

    return step;
}

/// Parse a .etprog JSON file into a ProgramData struct.
etest::engine::ProgramData parseEtProg(const QString& path) {
    etest::engine::ProgramData program;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        spdlog::error("[executor] Cannot open .etprog: {}", path.toStdString());
        return program;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        spdlog::error("[executor] Invalid .etprog JSON: {}",
                      err.errorString().toStdString());
        return program;
    }

    QJsonObject root = doc.object();

    // Suite name
    program.suiteName = root.value(QStringLiteral("name")).toString();

    // Cases
    QJsonArray casesArr = root.value(QStringLiteral("cases")).toArray();
    for (const QJsonValue& cv : casesArr) {
        QJsonObject caseObj = cv.toObject();
        etest::engine::TestCaseData tc;
        tc.caseName = caseObj.value(QStringLiteral("name")).toString();

        QJsonArray stepsArr = caseObj.value(QStringLiteral("steps")).toArray();
        for (const QJsonValue& sv : stepsArr) {
            tc.steps.append(parseStepJson(sv.toObject()));
        }

        program.cases.append(tc);
    }

    return program;
}

/// Extract device name list from a .etopo JSON root object.
QStringList extractDevicesFromTopology(const QJsonObject& root) {
    QStringList devices;
    QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
    for (const QJsonValue& val : nodes) {
        QJsonObject obj = val.toObject();
        QString name = obj.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) {
            devices.append(name);
        }
    }
    return devices;
}

/// Convert StepStatus to short label.
QString statusLabel(int status) {
    switch (status) {
        case etest::engine::PASS:
            return QStringLiteral("PASS");
        case etest::engine::FAIL:
            return QStringLiteral("FAIL");
        case etest::engine::TIMEOUT:
            return QStringLiteral("TIMEOUT");
        case etest::engine::ERROR:
            return QStringLiteral("ERROR");
        case etest::engine::SKIPPED:
            return QStringLiteral("SKIPPED");
        default:
            return QStringLiteral("PENDING");
    }
}

/// Convert StepStatus to short icon string (ASCII only for CLI).
const char* statusIcon(int status) {
    switch (status) {
        case etest::engine::PASS:
            return "[PASS]";
        case etest::engine::FAIL:
            return "[FAIL]";
        case etest::engine::TIMEOUT:
            return "[TIMEOUT]";
        case etest::engine::ERROR:
            return "[ERROR]";
        case etest::engine::SKIPPED:
            return "[SKIP]";
        default:
            return "[..]";
    }
}

}  // anonymous namespace

// =============================================================================
// main
// =============================================================================

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("test-executor-cli"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    // -- Command line parser --
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("测试执行引擎 CLI - 独立的命令行测试执行器"));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(
        {{QStringLiteral("t"), QStringLiteral("topology")},
         QStringLiteral("拓扑文件路径 (*.etopo)"),
         QStringLiteral("path")});
    parser.addOption(
        {{QStringLiteral("i"), QStringLiteral("icd")},
         QStringLiteral("ICD 文件路径 (*.eproto)"),
         QStringLiteral("path")});
    parser.addOption(
        {{QStringLiteral("p"), QStringLiteral("program")},
         QStringLiteral("测试程序文件路径 (*.etprog)"),
         QStringLiteral("path")});
    parser.addOption(
        {{QStringLiteral("o"), QStringLiteral("output")},
         QStringLiteral("结果输出文件路径 (*.etlog)"),
         QStringLiteral("path")});
    parser.addOption(
        {{QStringLiteral("timeout")},
         QStringLiteral("全局超时时间（毫秒）"),
         QStringLiteral("ms")});
    parser.addOption(
        {{QStringLiteral("verify-only")},
         QStringLiteral("仅验证文件，不执行")});

    parser.process(app);

    // -- Extract options --
    QString topologyPath = parser.value(QStringLiteral("topology"));
    QString icdPath = parser.value(QStringLiteral("icd"));
    QString programPath = parser.value(QStringLiteral("program"));
    QString outputPath = parser.value(QStringLiteral("output"));
    bool verifyOnly = parser.isSet(QStringLiteral("verify-only"));
    int globalTimeoutMs = 0;
    if (parser.isSet(QStringLiteral("timeout"))) {
        bool ok = false;
        globalTimeoutMs = parser.value(QStringLiteral("timeout")).toInt(&ok);
        if (!ok || globalTimeoutMs <= 0) {
            qCritical("Invalid --timeout value; must be a positive integer.");
            return 3;
        }
    }

    // -- Validate required: program --
    if (programPath.isEmpty()) {
        qCritical("Error: --program is required.");
        parser.showHelp(3);
        return 3;
    }

    // -- Validate file existence --
    auto checkFile = [](const QString& path, const QString& label) -> bool {
        if (path.isEmpty()) {
            return true;  // optional
        }
        if (!QFileInfo::exists(path)) {
            qCritical("Error: %s file does not exist: %s",
                      qPrintable(label), qPrintable(path));
            return false;
        }
        return true;
    };

    if (!checkFile(topologyPath, QStringLiteral("Topology"))) {
        return 3;
    }
    if (!checkFile(icdPath, QStringLiteral("ICD"))) {
        return 3;
    }
    if (!checkFile(programPath, QStringLiteral("Program"))) {
        return 3;
    }

    // -- Register meta types for cross-thread signals --
    qRegisterMetaType<etest::engine::StepResult>();
    qRegisterMetaType<etest::engine::ProgramData>();

    // -- Create core dependencies --
    auto* registry = new etest::core::SignalRegistry(&app);
    // CLI mode: no ICD repository (pass nullptr)
    icd::Repository* icdRepo = nullptr;
    Q_UNUSED(icdPath);

    // -- Create engine --
    auto* engine = new etest::engine::TestExecutionEngine(registry, icdRepo, &app);

    // -- Load topology (optional) --
    if (!topologyPath.isEmpty()) {
        bool topologyLoaded = engine->loadTopology(topologyPath);
        if (!topologyLoaded) {
            qWarning("Warning: Failed to load topology: %s",
                     qPrintable(topologyPath));
        }
    }

    // -- Load and set program --
    etest::engine::ProgramData program = parseEtProg(programPath);
    if (program.suiteName.isEmpty()) {
        qCritical("Error: Failed to parse .etprog or suite is empty: %s",
                  qPrintable(programPath));
        return 2;
    }
    engine->setProgram(program);

    // -- Verify-only mode --
    if (verifyOnly) {
        int totalSteps = 0;
        for (const auto& tc : program.cases) {
            totalSteps += tc.steps.size();
        }
        QTextStream out(stdout);
        out << "[VERIFY] Suite: " << program.suiteName
            << " | Cases: " << program.cases.size()
            << " | Steps: " << totalSteps
            << " | PASS" << Qt::endl;
        return 0;
    }

    // -- Tracking state --
    struct RunState {
        bool hasFailure = false;
        bool hasError = false;
    };
    RunState runState;

    // -- Connect engine signals -> stdout --
    QObject::connect(engine,
        &etest::engine::TestExecutionEngine::suiteStarted,
        [](const QString& name) {
            QTextStream out(stdout);
            out << "[SUITE] " << name << Qt::endl;
        });

    QObject::connect(engine,
        &etest::engine::TestExecutionEngine::caseStarted,
        [](int idx, const QString& name) {
            QTextStream out(stdout);
            out << "  [CASE " << (idx + 1) << "/" << name << "] " << name
                << Qt::endl;
        });

    QObject::connect(engine,
        &etest::engine::TestExecutionEngine::stepFinished,
        [&runState](int caseIndex, const QString& stepPath,
                    const etest::engine::StepResult& result) {
            Q_UNUSED(caseIndex);
            QTextStream out(stdout);
            QString detail = result.message.isEmpty()
                ? QStringLiteral("OK") : result.message;
            out << "    [" << stepPath << "] " << result.command
                << " " << statusIcon(result.status)
                << " (" << result.elapsedMs << "ms)"
                << " [" << detail << "]" << Qt::endl;

            if (result.status == etest::engine::FAIL ||
                result.status == etest::engine::TIMEOUT) {
                runState.hasFailure = true;
            } else if (result.status == etest::engine::ERROR) {
                runState.hasError = true;
            }
        });

    QObject::connect(engine,
        &etest::engine::TestExecutionEngine::suiteFinished,
        [](const QString& name, int pass, int fail) {
            QTextStream out(stdout);
            out << "[SUITE FINISHED] " << name
                << "  PASS: " << pass
                << "  FAIL: " << fail << Qt::endl;
        });

    // -- Event loop for async execution --
    QEventLoop loop;

    QObject::connect(engine,
        &etest::engine::TestExecutionEngine::engineFinished,
        [&loop]() {
            loop.quit();
        });

    QObject::connect(engine,
        &etest::engine::TestExecutionEngine::engineError,
        [&loop, &runState](const QString& message) {
            QTextStream out(stdout);
            out << "    [ERROR] " << message << Qt::endl;
            runState.hasError = true;
            loop.quit();
        });

    // -- Global timeout (optional) --
    QTimer globalTimer;
    if (globalTimeoutMs > 0) {
        globalTimer.setSingleShot(true);
        QObject::connect(&globalTimer, &QTimer::timeout,
            [&loop, &runState]() {
                QTextStream out(stdout);
                out << "    [ERROR] Global timeout reached." << Qt::endl;
                runState.hasError = true;
                runState.hasFailure = true;
                loop.quit();
            });
        globalTimer.start(globalTimeoutMs);
    }

    // -- Start execution --
    if (!engine->start()) {
        qCritical("Error: Engine failed to start.");
        return 2;
    }

    // -- Wait for completion --
    loop.exec();

    // -- Stop global timer if still running --
    if (globalTimer.isActive()) {
        globalTimer.stop();
    }

    // -- Save report if requested --
    if (!outputPath.isEmpty()) {
        engine->saveReport(outputPath);
        QTextStream out(stdout);
        out << "Report saved to: " << outputPath << Qt::endl;
    }

    // -- Determine exit code --
    // 0 = all pass, 1 = has fail/timeout, 2 = internal error, 3 = param error
    if (runState.hasError) {
        return 2;
    }
    if (runState.hasFailure) {
        return 1;
    }
    return 0;
}
