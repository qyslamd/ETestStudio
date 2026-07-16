#ifndef ETEST_ENGINE_RESULT_COLLECTOR_H_
#define ETEST_ENGINE_RESULT_COLLECTOR_H_

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace etest::engine {

class StepRunner;
struct StepResult;
struct IterationResult;

class ResultCollector : public QObject {
    Q_OBJECT

 public:
    explicit ResultCollector(QObject* parent = nullptr);

    void attach(StepRunner* runner);
    void saveToFile(const QString& etlogPath);
    void setMonitorData(const QJsonArray& monitors);
    void clear();

 private slots:
    void onSuiteStarted(const QString& name);
    void onSuiteFinished(const QString& name, int pass, int fail);
    void onCaseStarted(int idx, const QString& name);
    void onCaseFinished(int idx, const QString& name, int result);
    void onStepFinished(int idx, const QString& stepPath, const StepResult& result);

 private:
    static QJsonObject buildStepJson(const StepResult& step);
    static QJsonObject buildIterationJson(const IterationResult& iteration);
    static QString statusToString(int status);

    QJsonObject current_report_;
    QJsonObject current_case_;
    QJsonArray current_steps_;
    QJsonArray monitor_data_;  // 由 setMonitorData 注入，saveToFile 时写入 monitors[]
    QDateTime case_start_time_;
    int step_count_ = 0;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_RESULT_COLLECTOR_H_
