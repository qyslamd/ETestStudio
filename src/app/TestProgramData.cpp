#include "TestProgramData.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace etest::app {

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

TestProgramData loadTestProgram(const QString& filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return TestProgramData{};
  }

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  file.close();

  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return TestProgramData{};
  }

  return testProgramFromJson(doc.object());
}

}  // namespace etest::app
