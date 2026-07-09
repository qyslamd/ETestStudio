#include "ResultCollector.h"
#include "StepRunner.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <spdlog/spdlog.h>

namespace etest::engine {

// ══════════════════════════════════════════════════════════════════════════════
// Construction / public API
// ══════════════════════════════════════════════════════════════════════════════

ResultCollector::ResultCollector(QObject* parent)
    : QObject(parent) {
}

void ResultCollector::attach(StepRunner* runner) {
    connect(runner, &StepRunner::suiteStarted,
            this, &ResultCollector::onSuiteStarted);
    connect(runner, &StepRunner::suiteFinished,
            this, &ResultCollector::onSuiteFinished);
    connect(runner, &StepRunner::caseStarted,
            this, &ResultCollector::onCaseStarted);
    connect(runner, &StepRunner::caseFinished,
            this, &ResultCollector::onCaseFinished);
    connect(runner, &StepRunner::stepFinished,
            this, &ResultCollector::onStepFinished);
}

void ResultCollector::clear() {
    current_report_ = QJsonObject();
    current_case_ = QJsonObject();
    current_steps_ = QJsonArray();
    step_count_ = 0;
    case_start_time_ = QDateTime();
}

// ══════════════════════════════════════════════════════════════════════════════
// Signal slots
// ══════════════════════════════════════════════════════════════════════════════

void ResultCollector::onSuiteStarted(const QString& suiteName) {
    clear();

    current_report_["version"] = QStringLiteral("1.0");
    current_report_["suiteName"] = suiteName;
    current_report_["startTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // executionInfo — populated by TestExecutionEngine when available
    QJsonObject execInfo;
    execInfo["engineVersion"] = QStringLiteral("1.0");
    execInfo["topologyFile"] = QString();
    execInfo["programFile"] = QString();
    current_report_["executionInfo"] = execInfo;

    // summary — zeroed, updated as signals arrive
    QJsonObject summary;
    summary["totalCases"] = 0;
    summary["totalSteps"] = 0;
    summary["passCount"] = 0;
    summary["failCount"] = 0;
    summary["errorCount"] = 0;
    summary["durationMs"] = 0;
    current_report_["summary"] = summary;

    current_report_["cases"] = QJsonArray();
}

void ResultCollector::onSuiteFinished(const QString& suiteName, int passCount,
                                       int failCount) {
    Q_UNUSED(suiteName);

    current_report_["endTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject summary = current_report_["summary"].toObject();
    summary["passCount"] = passCount;
    summary["failCount"] = failCount;
    summary["totalSteps"] = step_count_;

    QDateTime startTime = QDateTime::fromString(
        current_report_["startTime"].toString(), Qt::ISODate);
    if (startTime.isValid()) {
        summary["durationMs"] = static_cast<int>(
            startTime.msecsTo(QDateTime::currentDateTime()));
    }

    current_report_["summary"] = summary;
}

void ResultCollector::onCaseStarted(int caseIndex, const QString& caseName) {
    current_case_ = QJsonObject();
    current_case_["caseIndex"] = caseIndex;
    current_case_["caseName"] = caseName;
    current_case_["status"] = QStringLiteral("RUNNING");
    current_case_["durationMs"] = 0;
    current_case_["steps"] = QJsonArray();
    current_steps_ = QJsonArray();
    case_start_time_ = QDateTime::currentDateTime();
}

void ResultCollector::onCaseFinished(int caseIndex, const QString& caseName,
                                      int result) {
    Q_UNUSED(caseIndex);
    Q_UNUSED(caseName);

    // Update status
    current_case_["status"] =
        (result == 0) ? QStringLiteral("PASS") : QStringLiteral("FAIL");

    // Calculate duration
    if (case_start_time_.isValid()) {
        current_case_["durationMs"] = static_cast<int>(
            case_start_time_.msecsTo(QDateTime::currentDateTime()));
    }

    // Collect steps into the case object
    current_case_["steps"] = current_steps_;

    // Append case to the report
    QJsonArray cases = current_report_["cases"].toArray();
    cases.append(current_case_);
    current_report_["cases"] = cases;

    // Update totalCases in summary
    QJsonObject summary = current_report_["summary"].toObject();
    summary["totalCases"] = cases.size();
    current_report_["summary"] = summary;

    // Reset per-case state
    current_case_ = QJsonObject();
    current_steps_ = QJsonArray();
}

void ResultCollector::onStepFinished(int caseIndex, const QString& stepPath,
                                      const StepResult& result) {
    Q_UNUSED(caseIndex);

    step_count_++;

    // Track error count for the report summary
    if (result.status == ERROR) {
        QJsonObject summary = current_report_["summary"].toObject();
        summary["errorCount"] = summary["errorCount"].toInt() + 1;
        current_report_["summary"] = summary;
    }

    // Only top-level steps (no '/' in stepPath) are added to the steps array.
    // Sub-steps from control-flow nesting (LOOP/WHILE/IF) are embedded in the
    // parent StepResult via iterations (LOOP/WHILE) or branches (IF) and are
    // handled by buildStepJson().
    if (!stepPath.contains(QLatin1Char('/'))) {
        current_steps_.append(buildStepJson(result));
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// .etlog JSON serialisation helpers
// ══════════════════════════════════════════════════════════════════════════════

QString ResultCollector::statusToString(int status) {
    switch (status) {
        case 0:
            return QStringLiteral("PENDING");
        case 1:
            return QStringLiteral("PASS");
        case 2:
            return QStringLiteral("FAIL");
        case 3:
            return QStringLiteral("TIMEOUT");
        case 4:
            return QStringLiteral("ERROR");
        case 5:
            return QStringLiteral("SKIPPED");
        default:
            return QStringLiteral("UNKNOWN");
    }
}

QJsonObject ResultCollector::buildStepJson(const StepResult& step) {
    QJsonObject obj;
    obj["path"] = step.stepPath;
    obj["command"] = step.command;

    if (!step.target.isEmpty()) {
        obj["target"] = step.target;
    }

    obj["status"] = statusToString(step.status);
    obj["elapsedMs"] = step.elapsedMs;
    obj["timestamp"] = step.timestamp.toString(Qt::ISODate);

    // Include value fields when meaningful
    if (step.expectedValue != 0.0 || step.command == QStringLiteral("CHECK") ||
        step.command == QStringLiteral("VERIFY")) {
        obj["expectedValue"] = step.expectedValue;
    }
    if (step.actualValue != 0.0 || step.command == QStringLiteral("CHECK") ||
        step.command == QStringLiteral("VERIFY")) {
        obj["actualValue"] = step.actualValue;
    }

    if (!step.message.isEmpty()) {
        obj["message"] = step.message;
    }

    // LOOP / WHILE — serialise iterations
    if (!step.iterations.isEmpty()) {
        QJsonArray iters;
        for (const auto& iter : step.iterations) {
            iters.append(buildIterationJson(iter));
        }
        obj["iterations"] = iters;
    }

    // IF — serialise branches
    if (step.command == QStringLiteral("IF") &&
        (!step.branches.thenSteps.isEmpty() ||
         !step.branches.elseSteps.isEmpty())) {
        QJsonObject branches;
        QJsonArray thenArr;
        for (const auto& s : step.branches.thenSteps) {
            thenArr.append(buildStepJson(s));
        }
        QJsonArray elseArr;
        for (const auto& s : step.branches.elseSteps) {
            elseArr.append(buildStepJson(s));
        }
        branches["then"] = thenArr;
        branches["else"] = elseArr;
        obj["branches"] = branches;
    }

    return obj;
}

QJsonObject ResultCollector::buildIterationJson(const IterationResult& iteration) {
    QJsonObject obj;
    obj["iteration"] = iteration.iteration;
    QJsonArray subSteps;
    for (const auto& step : iteration.subSteps) {
        subSteps.append(buildStepJson(step));
    }
    obj["subSteps"] = subSteps;
    return obj;
}

// ══════════════════════════════════════════════════════════════════════════════
// File I/O
// ══════════════════════════════════════════════════════════════════════════════

void ResultCollector::saveToFile(const QString& etlogPath) {
    QFile file(etlogPath);
    if (!file.open(QIODevice::WriteOnly)) {
        spdlog::error("[ResultCollector] Cannot write etlog: {}",
                      etlogPath.toStdString());
        return;
    }

    QJsonDocument doc(current_report_);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    spdlog::info("[ResultCollector] Wrote etlog: {}", etlogPath.toStdString());
}

}  // namespace etest::engine
