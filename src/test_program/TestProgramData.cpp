#include "TestProgramData.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include "logger/Logger.h"

namespace etest::app {

// ── ConditionExpr ──

static QJsonObject conditionExprToJson(const ConditionExpr& expr) {
  if (expr.target.isEmpty()) {
    return {};
  }
  QJsonObject obj;
  obj["target"] = expr.target;
  obj["op"] = expr.op;
  if (expr.value.isValid()) {
    obj["value"] = QJsonValue::fromVariant(expr.value);
  }
  return obj;
}

static ConditionExpr conditionExprFromJson(const QJsonObject& obj) {
  ConditionExpr expr;
  expr.target = obj["target"].toString();
  expr.op = obj["op"].toString();
  expr.value = obj["value"].toVariant();
  return expr;
}

// ── ToleranceSpec ──

static QJsonObject toleranceToJson(const ToleranceSpec& tol) {
  if (!tol.enabled) {
    return {};
  }
  QJsonObject obj;
  obj["min"] = tol.min;
  obj["max"] = tol.max;
  obj["enabled"] = tol.enabled;
  return obj;
}

static ToleranceSpec toleranceFromJson(const QJsonObject& obj) {
  ToleranceSpec tol;
  tol.min = obj["min"].toDouble(0.0);
  tol.max = obj["max"].toDouble(0.0);
  tol.enabled = obj["enabled"].toBool(false);
  return tol;
}

// ── FaultConfig ──

static QJsonObject faultConfigToJson(const FaultConfig& fault) {
  if (fault.type.isEmpty()) {
    return {};
  }
  QJsonObject obj;
  obj["type"] = fault.type;
  if (fault.value.isValid()) {
    obj["value"] = QJsonValue::fromVariant(fault.value);
  }
  return obj;
}

static FaultConfig faultConfigFromJson(const QJsonObject& obj) {
  FaultConfig fault;
  fault.type = obj["type"].toString();
  fault.value = obj["value"].toVariant();
  return fault;
}

// ── 子步骤序列化辅助 ──

static QJsonArray stepsToJsonArray(const QVector<TestStepData>& steps) {
  QJsonArray arr;
  for (const auto& s : steps) {
    arr.append(testStepToJson(s));
  }
  return arr;
}

static QVector<TestStepData> stepsFromJsonArray(const QJsonArray& arr) {
  QVector<TestStepData> steps;
  for (const auto& v : arr) {
    steps.append(testStepFromJson(v.toObject()));
  }
  return steps;
}

// ── TestStepData ──

QJsonObject testStepToJson(const TestStepData& step) {
  QJsonObject obj;
  obj["cmd"] = step.cmd;
  obj["target"] = step.target;
  if (step.value.isValid()) {
    obj["value"] = QJsonValue::fromVariant(step.value);
  }
  obj["delayMs"] = step.delayMs;
  obj["timeoutMs"] = step.timeoutMs;
  obj["description"] = step.description;

  // 新字段（仅序列化非默认值）
  if (!step.condition.target.isEmpty()) {
    obj["condition"] = conditionExprToJson(step.condition);
  }
  if (step.tolerance.enabled) {
    obj["tolerance"] = toleranceToJson(step.tolerance);
  }
  if (!step.fault.type.isEmpty()) {
    obj["fault"] = faultConfigToJson(step.fault);
  }
  if (step.loopCount > 1 || step.loopIntervalMs > 0) {
    obj["loopCount"] = step.loopCount;
    obj["loopIntervalMs"] = step.loopIntervalMs;
  }
  if (!step.subSteps.isEmpty()) {
    obj["subSteps"] = stepsToJsonArray(step.subSteps);
  }
  if (!step.elseSubSteps.isEmpty()) {
    obj["elseSubSteps"] = stepsToJsonArray(step.elseSubSteps);
  }

  return obj;
}

TestStepData testStepFromJson(const QJsonObject& obj) {
  TestStepData step;
  step.cmd = obj["cmd"].toString();
  step.target = obj["target"].toString();
  step.value = obj["value"].toVariant();
  step.delayMs = obj["delayMs"].toInt();
  step.timeoutMs = obj["timeoutMs"].toInt(5000);
  step.description = obj["description"].toString();

  // 新字段（缺省时为默认值，后向兼容）
  if (obj.contains("condition")) {
    step.condition = conditionExprFromJson(obj["condition"].toObject());
  }
  if (obj.contains("tolerance")) {
    step.tolerance = toleranceFromJson(obj["tolerance"].toObject());
  }
  if (obj.contains("fault")) {
    step.fault = faultConfigFromJson(obj["fault"].toObject());
  }
  if (obj.contains("loopCount")) {
    step.loopCount = obj["loopCount"].toInt(1);
  }
  if (obj.contains("loopIntervalMs")) {
    step.loopIntervalMs = obj["loopIntervalMs"].toInt();
  }
  if (obj.contains("subSteps")) {
    step.subSteps = stepsFromJsonArray(obj["subSteps"].toArray());
  }
  if (obj.contains("elseSubSteps")) {
    step.elseSubSteps = stepsFromJsonArray(obj["elseSubSteps"].toArray());
  }

  return step;
}

// ── TestCaseData ──

QJsonObject testCaseToJson(const TestCaseData& tc) {
  QJsonObject obj;
  obj["name"] = tc.name;
  obj["description"] = tc.description;

  QJsonArray steps;
  for (const auto& s : tc.steps) {
    steps.append(testStepToJson(s));
  }
  obj["steps"] = steps;

  return obj;
}

TestCaseData testCaseFromJson(const QJsonObject& obj) {
  TestCaseData tc;
  tc.name = obj["name"].toString();
  tc.description = obj["description"].toString();

  const auto stepsArr = obj["steps"].toArray();
  for (const auto& v : stepsArr) {
    tc.steps.append(testStepFromJson(v.toObject()));
  }

  return tc;
}

// ── TestProgramData ──

QJsonObject testProgramToJson(const TestProgramData& suite) {
  QJsonObject obj;
  obj["version"] = suite.version;
  obj["name"] = suite.name;
  obj["description"] = suite.description;

  // setup
  QJsonArray setupArr;
  for (const auto& s : suite.setup) {
    setupArr.append(testStepToJson(s));
  }
  obj["setup"] = setupArr;

  // teardown
  QJsonArray teardownArr;
  for (const auto& s : suite.teardown) {
    teardownArr.append(testStepToJson(s));
  }
  obj["teardown"] = teardownArr;

  // cases
  QJsonArray casesArr;
  for (const auto& tc : suite.cases) {
    casesArr.append(testCaseToJson(tc));
  }
  obj["cases"] = casesArr;

  return obj;
}

TestProgramData testProgramFromJson(const QJsonObject& obj) {
  TestProgramData suite;
  suite.version = obj["version"].toString("1.0");
  suite.name = obj["name"].toString();
  suite.description = obj["description"].toString();

  const auto setupArr = obj["setup"].toArray();
  for (const auto& v : setupArr) {
    suite.setup.append(testStepFromJson(v.toObject()));
  }

  const auto teardownArr = obj["teardown"].toArray();
  for (const auto& v : teardownArr) {
    suite.teardown.append(testStepFromJson(v.toObject()));
  }

  const auto casesArr = obj["cases"].toArray();
  for (const auto& v : casesArr) {
    suite.cases.append(testCaseFromJson(v.toObject()));
  }

  return suite;
}

// ── 文件 I/O ──

bool saveTestProgram(const QString& filePath, const TestProgramData& suite) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  QJsonDocument doc(testProgramToJson(suite));
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();
  return true;
}

bool saveDefaultTestProgram(const QString& filePath) {
  TestProgramData suite;
  suite.name = QFileInfo(filePath).completeBaseName();
  return saveTestProgram(filePath, suite);
}

TestProgramData loadTestProgram(const QString& filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_ERROR("TP", "无法打开文件: {}", filePath.toStdString());
    return TestProgramData{};
  }

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  file.close();

  if (err.error != QJsonParseError::NoError) {
    LOG_ERROR("TP", "JSON解析失败 ({}): {}",
              filePath.toStdString(), err.errorString().toStdString());
    return TestProgramData{};
  }

  if (!doc.isObject()) {
    LOG_ERROR("TP", "JSON根节点不是对象: {}", filePath.toStdString());
    return TestProgramData{};
  }

  return testProgramFromJson(doc.object());
}

}  // namespace etest::app
