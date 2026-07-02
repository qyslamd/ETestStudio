#ifndef ETEST_PROGRAM_TESTPROGRAM_DATA_H_
#define ETEST_PROGRAM_TESTPROGRAM_DATA_H_

#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QVector>

namespace etest::app {

// ── 条件表达式（WAIT / WHILE / IF 共用） ──
struct ConditionExpr {
  QString target;       // 信号 UUID 或名称
  QString op;           // ==, !=, >, <, >=, <=
  QVariant value;       // 条件值
};

// ── 容差规范（VERIFY 使用） ──
struct ToleranceSpec {
  double min = 0.0;
  double max = 0.0;
  bool enabled = false;
};

// ── 故障注入配置（INJECT_FAULT 使用） ──
struct FaultConfig {
  QString type;         // stuck_at, offset, noise, crc_error
  QVariant value;
};

// ── 测试步骤 ──
struct TestStepData {
  QString cmd;                   // SET / VERIFY / WAIT / DELAY / ACTION / LOG /
                                 // LOOP / WHILE / IF /
                                 // INJECT_FAULT / CLEAR_FAULT / PHOTO / RECORD
  QString target;                // 信号/变量名（或 UUID）
  QVariant value;                // 设定值/期望值
  int delayMs = 0;               // 延迟(毫秒)
  int timeoutMs = 5000;          // 超时(毫秒)
  QString description;           // 步骤说明

  // 条件表达式（WAIT / WHILE / IF）
  ConditionExpr condition;

  // 容差（VERIFY）
  ToleranceSpec tolerance;

  // 故障注入（INJECT_FAULT）
  FaultConfig fault;

  // 循环参数（LOOP / WHILE）
  int loopCount = 1;
  int loopIntervalMs = 0;

  // 嵌套子步骤（LOOP body / WHILE body / IF then-branch）
  QVector<TestStepData> subSteps;
  // IF else-branch（可选）
  QVector<TestStepData> elseSubSteps;

  bool isControlFlow() const {
    return cmd == QStringLiteral("LOOP") || cmd == QStringLiteral("WHILE") ||
           cmd == QStringLiteral("IF");
  }

  bool hasSubSteps() const {
    return !subSteps.isEmpty() || !elseSubSteps.isEmpty();
  }
};

// ── 单个测试用例 ──
struct TestCaseData {
  QString name;
  QString description;
  QVector<TestStepData> steps;
};

// ── 测试程序（对应一个 .tcase 文件） ──
struct TestProgramData {
  QString name;
  QString description;
  QString version = "1.1";
  QVector<TestStepData> setup;     // 前置步骤
  QVector<TestStepData> teardown;  // 后置步骤
  QVector<TestCaseData> cases;
};

// ── 序列化 ──
QJsonObject testStepToJson(const TestStepData& step);
TestStepData testStepFromJson(const QJsonObject& obj);

QJsonObject testCaseToJson(const TestCaseData& tc);
TestCaseData testCaseFromJson(const QJsonObject& obj);

QJsonObject testProgramToJson(const TestProgramData& suite);
TestProgramData testProgramFromJson(const QJsonObject& obj);

bool saveTestProgram(const QString& filePath, const TestProgramData& suite);
bool saveDefaultTestProgram(const QString& filePath);
TestProgramData loadTestProgram(const QString& filePath);

}  // namespace etest::app

#endif  // ETEST_PROGRAM_TESTPROGRAM_DATA_H_
