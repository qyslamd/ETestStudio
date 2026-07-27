#include "ResultCollector.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <spdlog/spdlog.h>
#include "StepRunner.h"
#include "logger/Logger.h"

namespace etest::engine {

// ══════════════════════════════════════════════════════════════════════════════
// Construction / public API
// ══════════════════════════════════════════════════════════════════════════════

ResultCollector::ResultCollector(QObject* parent) : QObject(parent) {}

void ResultCollector::attach(StepRunner* runner) {
  connect(runner, &StepRunner::suiteStarted, this,
          &ResultCollector::onSuiteStarted);
  connect(runner, &StepRunner::suiteFinished, this,
          &ResultCollector::onSuiteFinished);
  connect(runner, &StepRunner::caseStarted, this,
          &ResultCollector::onCaseStarted);
  connect(runner, &StepRunner::caseFinished, this,
          &ResultCollector::onCaseFinished);
  connect(runner, &StepRunner::stepFinished, this,
          &ResultCollector::onStepFinished);
}

void ResultCollector::setMonitorData(const QJsonArray& monitors) {
  monitor_data_ = monitors;
}

void ResultCollector::clear() {
  current_report_ = QJsonObject();
  current_case_ = QJsonObject();
  current_steps_ = QJsonArray();
  monitor_data_ = QJsonArray();
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
  current_report_["startTime"] =
      QDateTime::currentDateTime().toString(Qt::ISODate);

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

void ResultCollector::onSuiteFinished(const QString& suiteName,
                                      int passCount,
                                      int failCount) {
  Q_UNUSED(suiteName);

  current_report_["endTime"] =
      QDateTime::currentDateTime().toString(Qt::ISODate);

  QJsonObject summary = current_report_["summary"].toObject();
  summary["passCount"] = passCount;
  summary["failCount"] = failCount;
  summary["totalSteps"] = step_count_;

  QDateTime startTime = QDateTime::fromString(
      current_report_["startTime"].toString(), Qt::ISODate);
  if (startTime.isValid()) {
    summary["durationMs"] =
        static_cast<int>(startTime.msecsTo(QDateTime::currentDateTime()));
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

void ResultCollector::onCaseFinished(int caseIndex,
                                     const QString& caseName,
                                     int result) {
  Q_UNUSED(caseIndex);
  Q_UNUSED(caseName);
  Q_UNUSED(result);

  // Aggregate case status from steps instead of trusting the result parameter
  current_case_["status"] = aggregateCaseStatus(current_steps_);

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

void ResultCollector::onStepFinished(int caseIndex,
                                     const QString& stepPath,
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

QString ResultCollector::aggregateCaseStatus(const QJsonArray& steps) {
  int worst = 0;  // 0=PASS, 1=PENDING, 2=SKIPPED, 3=TIMEOUT, 4=FAIL, 5=ERROR
  for (const auto& s : steps) {
    QJsonObject step = s.toObject();
    QString st = step[QStringLiteral("status")].toString();

    int prio = 0;
    if (st == QStringLiteral("ERROR"))
      prio = 5;
    else if (st == QStringLiteral("FAIL"))
      prio = 4;
    else if (st == QStringLiteral("TIMEOUT"))
      prio = 3;
    else if (st == QStringLiteral("SKIPPED"))
      prio = 2;
    else if (st == QStringLiteral("PENDING"))
      prio = 1;
    if (prio > worst) {
      worst = prio;
      if (worst == 5) return QStringLiteral("ERROR");
    }

    // Recurse into LOOP/WHILE iterations
    for (const auto& iter : step[QStringLiteral("iterations")].toArray()) {
      QString sub = aggregateCaseStatus(
          iter.toObject()[QStringLiteral("subSteps")].toArray());
      if (sub == QStringLiteral("ERROR")) return QStringLiteral("ERROR");
      if (sub == QStringLiteral("FAIL") && worst < 4) worst = 4;
      if (sub == QStringLiteral("TIMEOUT") && worst < 3) worst = 3;
      if (sub == QStringLiteral("SKIPPED") && worst < 2) worst = 2;
    }

    // Recurse into IF branches
    QJsonObject branches =
        step[QStringLiteral("branches")].toObject();
    if (!branches.isEmpty()) {
      QString thenSub = aggregateCaseStatus(
          branches[QStringLiteral("then")].toArray());
      QString elseSub = aggregateCaseStatus(
          branches[QStringLiteral("else")].toArray());
      for (const auto& sub : {thenSub, elseSub}) {
        if (sub == QStringLiteral("ERROR"))
          return QStringLiteral("ERROR");
        if (sub == QStringLiteral("FAIL") && worst < 4) worst = 4;
        if (sub == QStringLiteral("TIMEOUT") && worst < 3) worst = 3;
        if (sub == QStringLiteral("SKIPPED") && worst < 2) worst = 2;
      }
    }
  }

  switch (worst) {
    case 4:
      return QStringLiteral("FAIL");
    case 3:
      return QStringLiteral("TIMEOUT");
    case 2:
      return QStringLiteral("SKIPPED");
    case 1:
      return QStringLiteral("PENDING");
    default:
      return QStringLiteral("PASS");
  }
}

QJsonObject ResultCollector::buildStepJson(const StepResult& step) {
  QJsonObject obj;
  obj["path"] = step.stepPath;
  obj["command"] = step.command;

  if (!step.target.isEmpty()) {
    obj["target"] = step.target;
  }

  if (!step.targetName.isEmpty()) {
    obj["targetName"] = step.targetName;
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

QJsonObject ResultCollector::buildIterationJson(
    const IterationResult& iteration) {
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
  QJsonObject report = current_report_;

  if (!monitor_data_.isEmpty()) {
    report[QStringLiteral("monitors")] = monitor_data_;
  }

  QFile file(etlogPath);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR("ResultCollector", "Cannot write etlog: {}",
              etlogPath.toStdString());
    return;
  }

  QJsonDocument doc(report);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  LOG_INFO("ResultCollector", "Wrote etlog: {}", etlogPath.toStdString());
}

// ══════════════════════════════════════════════════════════════════════════════
// Segment export -- 按程序分段切分报告
// ══════════════════════════════════════════════════════════════════════════════

void ResultCollector::traverseSteps(
    const QJsonObject& step,
    const std::function<void(const QJsonObject&)>& fn) {
  fn(step);

  // LOOP / WHILE 迭代子步骤
  const QJsonArray iterations = step.value(QStringLiteral("iterations")).toArray();
  for (const auto& iterVal : iterations) {
    const QJsonObject iter = iterVal.toObject();
    const QJsonArray subSteps = iter.value(QStringLiteral("subSteps")).toArray();
    for (const auto& subVal : subSteps) {
      traverseSteps(subVal.toObject(), fn);
    }
  }

  // IF 分支子步骤
  const QJsonObject branches = step.value(QStringLiteral("branches")).toObject();
  if (!branches.isEmpty()) {
    for (const auto& key : {QStringLiteral("then"), QStringLiteral("else")}) {
      const QJsonArray branchSteps = branches.value(key).toArray();
      for (const auto& subVal : branchSteps) {
        traverseSteps(subVal.toObject(), fn);
      }
    }
  }
}

void ResultCollector::saveSegmentToFile(const QString& etlogPath,
                                        const QString& suiteName,
                                        int startCase,
                                        int caseCount) {
  QJsonObject segment = current_report_;
  segment[QStringLiteral("suiteName")] = suiteName;

  // 切分 cases 数组
  const QJsonArray allCases = segment.value(QStringLiteral("cases")).toArray();
  QJsonArray segCases;
  int totalSteps = 0;
  int errorCount = 0;
  int passCount = 0;
  int failCount = 0;
  int durationMs = 0;

  for (int i = startCase; i < startCase + caseCount && i < allCases.size(); ++i) {
    const QJsonObject caseObj = allCases[i].toObject();
    segCases.append(caseObj);

    durationMs += caseObj.value(QStringLiteral("durationMs")).toInt();

    // 顶层步骤统计 pass/fail
    const QJsonArray steps = caseObj.value(QStringLiteral("steps")).toArray();
    for (const auto& stepVal : steps) {
      const QJsonObject step = stepVal.toObject();
      const QString status = step.value(QStringLiteral("status")).toString();
      if (status == QStringLiteral("PASS")) {
        ++passCount;
      } else if (status == QStringLiteral("FAIL") ||
                 status == QStringLiteral("TIMEOUT") ||
                 status == QStringLiteral("ERROR")) {
        ++failCount;
      }

      // 递归遍历所有步骤（含子步骤）统计 totalSteps 和 errorCount
      traverseSteps(step, [&](const QJsonObject& s) {
        ++totalSteps;
        if (s.value(QStringLiteral("status")).toString() ==
            QStringLiteral("ERROR")) {
          ++errorCount;
        }
      });
    }
  }

  segment[QStringLiteral("cases")] = segCases;

  // 重算 summary
  QJsonObject summary = segment.value(QStringLiteral("summary")).toObject();
  summary[QStringLiteral("totalCases")] = segCases.size();
  summary[QStringLiteral("totalSteps")] = totalSteps;
  summary[QStringLiteral("passCount")] = passCount;
  summary[QStringLiteral("failCount")] = failCount;
  summary[QStringLiteral("errorCount")] = errorCount;
  summary[QStringLiteral("durationMs")] = durationMs;
  segment[QStringLiteral("summary")] = summary;

  // monitor 数据完整保留
  if (!monitor_data_.isEmpty()) {
    segment[QStringLiteral("monitors")] = monitor_data_;
  }

  QFile file(etlogPath);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR("ResultCollector", "Cannot write segment etlog: {}",
              etlogPath.toStdString());
    return;
  }

  QJsonDocument doc(segment);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  LOG_INFO("ResultCollector", "Wrote segment etlog: {} [suite={} cases={}]",
           etlogPath.toStdString(), suiteName.toStdString(),
           segCases.size());
}

}  // namespace etest::engine
