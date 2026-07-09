#ifndef ETEST_APP_EXECUTION_MONITOR_PANEL_H_
#define ETEST_APP_EXECUTION_MONITOR_PANEL_H_

#include <QLabel>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QWidget>

#include <QMap>
#include <QSet>

#include "engine/StepRunner.h"

namespace etest::engine {
class TestExecutionEngine;
enum class EngineState;
enum class DeviceStatus;
}  // namespace etest::engine

namespace etest::app {

class ExecutionMonitorPanel : public QWidget {
  Q_OBJECT

 public:
  explicit ExecutionMonitorPanel(QWidget* parent = nullptr);
  void bindEngine(etest::engine::TestExecutionEngine* engine);
  void clear();

 private slots:
  void onSuiteStarted(const QString& name);
  void onSuiteFinished(const QString& name, int pass, int fail);
  void onCaseStarted(int caseIndex, const QString& name);
  void onStepStarted(int caseIndex, const QString& stepPath,
                     const QString& command, const QString& target);
  void onStepFinished(int caseIndex, const QString& stepPath,
                      const etest::engine::StepResult& result);
  void onProgressUpdated(int current, int total);
  void onEngineStateChanged(etest::engine::EngineState state);
  void onDeviceStatusChanged(const QString& deviceId,
                             etest::engine::DeviceStatus status);

 private:
  void initUi();
  void updateStats();
  QString statusIcon(etest::engine::StepStatus status) const;
  QString statusText(etest::engine::StepStatus status) const;
  QString statusHtmlColor(etest::engine::StepStatus status) const;
  QTreeWidgetItem* findOrCreateStepItem(int caseIndex,
                                        const QString& stepPath);

  QTreeWidget* tree_progress_ = nullptr;
  QPlainTextEdit* text_log_ = nullptr;
  QLabel* label_pass_ = nullptr;
  QLabel* label_fail_ = nullptr;
  QLabel* label_timeout_ = nullptr;
  QLabel* label_status_ = nullptr;
  QLabel* label_device_ = nullptr;

  int count_pass_ = 0;
  int count_fail_ = 0;
  int count_timeout_ = 0;

  // Maps case index -> tree top-level item
  QMap<int, QTreeWidgetItem*> case_items_;
  // Maps case index -> case name
  QMap<int, QString> case_names_;
  // Flat lookup: "caseIndex/stepPath" -> leaf tree item
  QMap<QString, QTreeWidgetItem*> step_items_;
  // Online device IDs
  QSet<QString> online_devices_;
};

}  // namespace etest::app

#endif  // ETEST_APP_EXECUTION_MONITOR_PANEL_H_
