#ifndef ETEST_PROGRAM_TESTPROGRAM_DATA_H_
#define ETEST_PROGRAM_TESTPROGRAM_DATA_H_

#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QVector>

namespace etest::app {

// ── 测试步骤 ──
struct TestStepData {
  QString cmd;          // SET / DELAY / CHECK / WAIT
  QString target;       // 信号/变量名
  QVariant value;       // 设定值/期望值
  int delayMs = 0;      // 延迟(毫秒)
  int timeoutMs = 5000; // 超时(毫秒)
  QString description;  // 步骤说明
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
  QString version = "1.0";
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
