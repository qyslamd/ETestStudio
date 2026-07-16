#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "StepRunner.h"
#include "ResultCollector.h"

using namespace etest::engine;

// ══════════════════════════════════════════════════════════════════════════════
// Test 1: Basic .etlog structure after a simple single-step execution
// ══════════════════════════════════════════════════════════════════════════════
TEST(ResultCollectorTest, BasicReportStructure) {
    StepRunner runner(nullptr, nullptr, nullptr);
    ResultCollector collector;

    collector.attach(&runner);

    // Emit lifecycle signals in the correct order
    emit runner.suiteStarted("TestSuite");

    emit runner.caseStarted(0, "Case1");

    StepResult stepResult;
    stepResult.stepPath = "0";
    stepResult.command = "SET";
    stepResult.target = "uuid-abc123";
    stepResult.status = PASS;
    stepResult.elapsedMs = 12;
    stepResult.timestamp = QDateTime::currentDateTime();
    emit runner.stepFinished(0, "0", stepResult);

    emit runner.caseFinished(0, "Case1", 0);   // 0 = PASS
    emit runner.suiteFinished("TestSuite", 1, 0);

    // Save to temp file
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    QString etlogPath = tempDir.path() + "/test.etlog";

    collector.saveToFile(etlogPath);

    // Verify the file was created
    ASSERT_TRUE(QFile::exists(etlogPath));

    // Read back and validate JSON structure
    QFile file(etlogPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    ASSERT_TRUE(doc.isObject());

    QJsonObject root = doc.object();

    // ── Top-level fields ──
    EXPECT_EQ(root["version"].toString(), QStringLiteral("1.0"));
    EXPECT_EQ(root["suiteName"].toString(), QStringLiteral("TestSuite"));
    EXPECT_TRUE(root.contains("startTime"));
    EXPECT_TRUE(root.contains("endTime"));
    EXPECT_TRUE(root.contains("executionInfo"));

    // ── Summary ──
    ASSERT_TRUE(root.contains("summary"));
    QJsonObject summary = root["summary"].toObject();
    EXPECT_EQ(summary["totalCases"].toInt(), 1);
    EXPECT_EQ(summary["totalSteps"].toInt(), 1);
    EXPECT_EQ(summary["passCount"].toInt(), 1);
    EXPECT_EQ(summary["failCount"].toInt(), 0);
    EXPECT_EQ(summary["errorCount"].toInt(), 0);
    EXPECT_GT(summary["durationMs"].toInt(), 0);

    // ── Execution info ──
    QJsonObject execInfo = root["executionInfo"].toObject();
    EXPECT_EQ(execInfo["engineVersion"].toString(), QStringLiteral("1.0"));

    // ── Cases ──
    ASSERT_TRUE(root.contains("cases"));
    QJsonArray cases = root["cases"].toArray();
    ASSERT_EQ(cases.size(), 1);

    QJsonObject firstCase = cases[0].toObject();
    EXPECT_EQ(firstCase["caseIndex"].toInt(), 0);
    EXPECT_EQ(firstCase["caseName"].toString(), QStringLiteral("Case1"));
    EXPECT_EQ(firstCase["status"].toString(), QStringLiteral("PASS"));
    EXPECT_GE(firstCase["durationMs"].toInt(), 0);

    // ── Steps ──
    ASSERT_TRUE(firstCase.contains("steps"));
    QJsonArray steps = firstCase["steps"].toArray();
    ASSERT_EQ(steps.size(), 1);

    QJsonObject stepObj = steps[0].toObject();
    EXPECT_EQ(stepObj["path"].toString(), QStringLiteral("0"));
    EXPECT_EQ(stepObj["command"].toString(), QStringLiteral("SET"));
    EXPECT_EQ(stepObj["target"].toString(), QStringLiteral("uuid-abc123"));
    EXPECT_EQ(stepObj["status"].toString(), QStringLiteral("PASS"));
    EXPECT_EQ(stepObj["elapsedMs"].toInt(), 12);
    EXPECT_TRUE(stepObj.contains("timestamp"));
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 2: Multiple cases are collected in order
// ══════════════════════════════════════════════════════════════════════════════
TEST(ResultCollectorTest, MultipleCasesInOrder) {
    StepRunner runner(nullptr, nullptr, nullptr);
    ResultCollector collector;
    collector.attach(&runner);

    emit runner.suiteStarted("MultiCaseSuite");

    // Case 1 — PASS
    emit runner.caseStarted(0, "Case1");
    {
        StepResult step;
        step.stepPath = "0";
        step.command = "SET";
        step.status = PASS;
        emit runner.stepFinished(0, "0", step);
    }
    emit runner.caseFinished(0, "Case1", 0);

    // Case 2 — FAIL
    emit runner.caseStarted(1, "Case2");
    {
        StepResult step;
        step.stepPath = "0";
        step.command = "CHECK";
        step.status = FAIL;
        step.message = QStringLiteral("超出容差范围");
        step.actualValue = 95.3;
        emit runner.stepFinished(1, "0", step);
    }
    emit runner.caseFinished(1, "Case2", 1);

    emit runner.suiteFinished("MultiCaseSuite", 1, 1);

    // Write and verify
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    QString etlogPath = tempDir.path() + "/multi.etlog";
    collector.saveToFile(etlogPath);

    ASSERT_TRUE(QFile::exists(etlogPath));
    QFile file(etlogPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();

    // Summary
    QJsonObject summary = root["summary"].toObject();
    EXPECT_EQ(summary["totalCases"].toInt(), 2);
    EXPECT_EQ(summary["passCount"].toInt(), 1);
    EXPECT_EQ(summary["failCount"].toInt(), 1);

    // Cases — verify order
    QJsonArray cases = root["cases"].toArray();
    ASSERT_EQ(cases.size(), 2);

    EXPECT_EQ(cases[0].toObject()["caseName"].toString(),
              QStringLiteral("Case1"));
    EXPECT_EQ(cases[0].toObject()["status"].toString(),
              QStringLiteral("PASS"));

    EXPECT_EQ(cases[1].toObject()["caseName"].toString(),
              QStringLiteral("Case2"));
    EXPECT_EQ(cases[1].toObject()["status"].toString(),
              QStringLiteral("FAIL"));

    // Case2 should have the error message in its step
    QJsonArray steps2 = cases[1].toObject()["steps"].toArray();
    ASSERT_EQ(steps2.size(), 1);
    EXPECT_EQ(steps2[0].toObject()["message"].toString(),
              QStringLiteral("超出容差范围"));
    EXPECT_EQ(steps2[0].toObject()["actualValue"].toDouble(), 95.3);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 3: Clear resets all collected data
// ══════════════════════════════════════════════════════════════════════════════
TEST(ResultCollectorTest, ClearResetsData) {
    StepRunner runner(nullptr, nullptr, nullptr);
    ResultCollector collector;
    collector.attach(&runner);

    // Collect some data
    emit runner.suiteStarted("Suite");
    emit runner.caseStarted(0, "Case");
    {
        StepResult step;
        step.stepPath = "0";
        step.command = "SET";
        step.status = PASS;
        emit runner.stepFinished(0, "0", step);
    }
    emit runner.caseFinished(0, "Case", 0);
    emit runner.suiteFinished("Suite", 1, 0);

    // Clear — should reset everything
    collector.clear();

    // Save after clear — should produce minimal JSON with no meaningful data
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    QString etlogPath = tempDir.path() + "/cleared.etlog";
    collector.saveToFile(etlogPath);

    ASSERT_TRUE(QFile::exists(etlogPath));
    QFile file(etlogPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    ASSERT_TRUE(doc.isObject());

    // After a fresh clear the report object is empty
    QJsonObject root = doc.object();
    EXPECT_TRUE(root.isEmpty());
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 4: LOOP control flow step with nested iterations
// ══════════════════════════════════════════════════════════════════════════════
TEST(ResultCollectorTest, LoopStepWithIterations) {
    StepRunner runner(nullptr, nullptr, nullptr);
    ResultCollector collector;
    collector.attach(&runner);

    emit runner.suiteStarted("ControlFlowSuite");
    emit runner.caseStarted(0, "LoopCase");

    // Build a LOOP step result with two iterations
    StepResult loopResult;
    loopResult.stepPath = "0";
    loopResult.command = "LOOP";
    loopResult.status = PASS;
    loopResult.elapsedMs = 250;
    loopResult.timestamp = QDateTime::currentDateTime();

    // Iteration 0
    {
        IterationResult iter0;
        iter0.iteration = 0;

        StepResult sub0;
        sub0.stepPath = "0/0/0";
        sub0.command = "SET";
        sub0.status = PASS;
        sub0.elapsedMs = 10;
        iter0.subSteps.append(sub0);

        StepResult sub1;
        sub1.stepPath = "0/0/1";
        sub1.command = "CHECK";
        sub1.status = FAIL;
        sub1.elapsedMs = 150;
        sub1.actualValue = 95.3;
        sub1.message = QStringLiteral("超出容差范围");
        iter0.subSteps.append(sub1);

        loopResult.iterations.append(iter0);
    }

    // Iteration 1
    {
        IterationResult iter1;
        iter1.iteration = 1;

        StepResult sub2;
        sub2.stepPath = "0/1/0";
        sub2.command = "SET";
        sub2.status = PASS;
        sub2.elapsedMs = 8;
        iter1.subSteps.append(sub2);

        loopResult.iterations.append(iter1);
    }

    emit runner.stepFinished(0, "0", loopResult);
    emit runner.caseFinished(0, "LoopCase", 0);
    emit runner.suiteFinished("ControlFlowSuite", 1, 0);

    // Write and verify
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    QString etlogPath = tempDir.path() + "/loop.etlog";
    collector.saveToFile(etlogPath);

    ASSERT_TRUE(QFile::exists(etlogPath));
    QFile file(etlogPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();

    QJsonArray cases = root["cases"].toArray();
    ASSERT_EQ(cases.size(), 1);
    QJsonArray steps = cases[0].toObject()["steps"].toArray();
    ASSERT_EQ(steps.size(), 1);

    QJsonObject step = steps[0].toObject();
    EXPECT_EQ(step["command"].toString(), QStringLiteral("LOOP"));
    EXPECT_EQ(step["status"].toString(), QStringLiteral("PASS"));
    EXPECT_EQ(step["elapsedMs"].toInt(), 250);

    // iterations array
    ASSERT_TRUE(step.contains("iterations"));
    QJsonArray iters = step["iterations"].toArray();
    ASSERT_EQ(iters.size(), 2);

    // First iteration
    {
        QJsonObject i0 = iters[0].toObject();
        EXPECT_EQ(i0["iteration"].toInt(), 0);
        ASSERT_TRUE(i0.contains("subSteps"));
        QJsonArray subs = i0["subSteps"].toArray();
        ASSERT_EQ(subs.size(), 2);

        EXPECT_EQ(subs[0].toObject()["command"].toString(),
                  QStringLiteral("SET"));
        EXPECT_EQ(subs[0].toObject()["status"].toString(),
                  QStringLiteral("PASS"));

        EXPECT_EQ(subs[1].toObject()["command"].toString(),
                  QStringLiteral("CHECK"));
        EXPECT_EQ(subs[1].toObject()["status"].toString(),
                  QStringLiteral("FAIL"));
        EXPECT_EQ(subs[1].toObject()["actualValue"].toDouble(), 95.3);
    }

    // Second iteration
    {
        QJsonObject i1 = iters[1].toObject();
        EXPECT_EQ(i1["iteration"].toInt(), 1);
        ASSERT_TRUE(i1.contains("subSteps"));
        QJsonArray subs = i1["subSteps"].toArray();
        ASSERT_EQ(subs.size(), 1);
        EXPECT_EQ(subs[0].toObject()["command"].toString(),
                  QStringLiteral("SET"));
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 5: Step-finished signals filtered by depth
// ══════════════════════════════════════════════════════════════════════════════
TEST(ResultCollectorTest, NestedStepsAreNotAddedAsTopLevel) {
    StepRunner runner(nullptr, nullptr, nullptr);
    ResultCollector collector;
    collector.attach(&runner);

    emit runner.suiteStarted("DepthTestSuite");
    emit runner.caseStarted(0, "Case");

    // Top-level step
    {
        StepResult step;
        step.stepPath = "0";
        step.command = "SET";
        step.status = PASS;
        emit runner.stepFinished(0, "0", step);
    }

    // Nested step — should NOT appear in the top-level steps array
    {
        StepResult step;
        step.stepPath = "1/0/0";
        step.command = "SET";
        step.status = PASS;
        emit runner.stepFinished(0, "1/0/0", step);
    }

    // Another nested step
    {
        StepResult step;
        step.stepPath = "1/0/1";
        step.command = "CHECK";
        step.status = FAIL;
        emit runner.stepFinished(0, "1/0/1", step);
    }

    emit runner.caseFinished(0, "Case", 0);
    emit runner.suiteFinished("DepthTestSuite", 1, 0);

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    QString etlogPath = tempDir.path() + "/depth.etlog";
    collector.saveToFile(etlogPath);

    ASSERT_TRUE(QFile::exists(etlogPath));
    QFile file(etlogPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();

    QJsonArray cases = root["cases"].toArray();
    ASSERT_EQ(cases.size(), 1);
    QJsonArray steps = cases[0].toObject()["steps"].toArray();

    // Only top-level step "0" should be in the array
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0].toObject()["path"].toString(), QStringLiteral("0"));

    // totalSteps should count ALL steps (including nested)
    EXPECT_EQ(root["summary"].toObject()["totalSteps"].toInt(), 3);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 6: saveToFile produces valid JSON even with empty report
// ══════════════════════════════════════════════════════════════════════════════
TEST(ResultCollectorTest, SaveToFileCreatesValidJson) {
    ResultCollector collector;

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    QString etlogPath = tempDir.path() + "/empty.etlog";

    // Save without any signals — should still produce a valid, empty JSON object
    collector.saveToFile(etlogPath);

    ASSERT_TRUE(QFile::exists(etlogPath));
    QFile file(etlogPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));

    QByteArray content = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(content, &parseError);

    EXPECT_EQ(parseError.error, QJsonParseError::NoError)
        << "JSON parse error: " << parseError.errorString().toStdString();
    EXPECT_TRUE(doc.isObject());
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 7: Case status is aggregated from steps, ignoring the result parameter
// ══════════════════════════════════════════════════════════════════════════════
TEST(ResultCollectorTest, CaseStatusAggregatedFromSteps) {
    StepRunner runner(nullptr, nullptr, nullptr);
    ResultCollector collector;
    collector.attach(&runner);

    emit runner.suiteStarted("AggregationSuite");

    // Case 1 — steps all PASS => case status PASS
    emit runner.caseStarted(0, "AllPass");
    {
        StepResult step;
        step.stepPath = "0";
        step.command = "SET";
        step.status = PASS;
        emit runner.stepFinished(0, "0", step);
    }
    emit runner.caseFinished(0, "AllPass", 0);  // result=0 should be ignored

    // Case 2 — step FAIL => case status FAIL
    emit runner.caseStarted(1, "WithFail");
    {
        StepResult step;
        step.stepPath = "0";
        step.command = "CHECK";
        step.status = FAIL;
        emit runner.stepFinished(1, "0", step);
    }
    emit runner.caseFinished(1, "WithFail", 0);  // result=0 should be ignored

    // Case 3 — step ERROR => case status ERROR (not FAIL)
    emit runner.caseStarted(2, "WithError");
    {
        StepResult step;
        step.stepPath = "0";
        step.command = "SET";
        step.status = ERROR;
        emit runner.stepFinished(2, "0", step);
    }
    emit runner.caseFinished(2, "WithError", 0);  // result=0 should be ignored

    // Case 4 — step TIMEOUT => case status TIMEOUT
    emit runner.caseStarted(3, "WithTimeout");
    {
        StepResult step;
        step.stepPath = "0";
        step.command = "WAIT";
        step.status = TIMEOUT;
        emit runner.stepFinished(3, "0", step);
    }
    emit runner.caseFinished(3, "WithTimeout", 0);

    // Case 5 — no steps => case status PASS (edge case for empty steps)
    emit runner.caseStarted(4, "NoSteps");
    emit runner.caseFinished(4, "NoSteps", 0);

    emit runner.suiteFinished("AggregationSuite", 1, 0);

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    QString etlogPath = tempDir.path() + "/aggregation.etlog";
    collector.saveToFile(etlogPath);

    ASSERT_TRUE(QFile::exists(etlogPath));
    QFile file(etlogPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();

    QJsonArray cases = root["cases"].toArray();
    ASSERT_EQ(cases.size(), 5);

    EXPECT_EQ(cases[0].toObject()["status"].toString(), "PASS");
    EXPECT_EQ(cases[1].toObject()["status"].toString(), "FAIL");
    EXPECT_EQ(cases[2].toObject()["status"].toString(), "ERROR");
    EXPECT_EQ(cases[3].toObject()["status"].toString(), "TIMEOUT");
    EXPECT_EQ(cases[4].toObject()["status"].toString(), "PASS");
}
