#ifndef ETEST_APP_EXECUTION_DEBUG_WIDGET_H_
#define ETEST_APP_EXECUTION_DEBUG_WIDGET_H_

#include <QLabel>
#include <QWidget>

#include <QMap>

#include "engine/StepRunner.h"

class QTreeWidget;
class QTreeWidgetItem;

namespace etest::engine {
class TestExecutionEngine;
enum class EngineState;
}  // namespace etest::engine

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace icd {
class Repository;
}  // namespace icd

namespace etest::app {

class ExecutionDebugWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ExecutionDebugWidget(QWidget* parent = nullptr);
  void bindEngine(etest::engine::TestExecutionEngine* engine);
  void setDependencies(etest::core::SignalRegistry* signalRegistry,
                       icd::Repository* icdRepo);
  void clear();

  /// 触发前提检查并刷新概览区
  void refreshPreconditions();
  /// 返回是否有阻断性错误（false = 存在阻断性错误）
  bool canRun() const;

 signals:
  void programSelected(const QString& filePath);

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

 private:
  void initUi();
  void initSignals();
  void updateStats();
  void checkPreconditions();
  QString statusIcon(etest::engine::StepStatus status) const;
  QString statusHtmlColor(etest::engine::StepStatus status) const;
  QTreeWidgetItem* findOrCreateStepItem(int caseIndex,
                                        const QString& stepPath);

  // ── 概览区 ──
  QWidget* overview_container_ = nullptr;
  QLabel* label_overview_summary_ = nullptr;   ///< "🟢 运行前提: 5/6 满足"
  QWidget* overview_detail_widget_ = nullptr;   ///< 展开后的详细列表容器

  // ── 进度树 ──
  QTreeWidget* tree_progress_ = nullptr;

  // ── 前提状态缓存 ──
  struct PreconditionState {
    bool met = false;
    bool isError = false;  // true = 阻断性错误, false = 警告
    QString label;
  };
  QVector<PreconditionState> precondition_states_;
  bool preconditions_checked_ = false;

  // ── 外部依赖 ──
  etest::core::SignalRegistry* signal_registry_ = nullptr;
  icd::Repository* icd_repo_ = nullptr;

  // ── 执行数据 ──
  int count_pass_ = 0;
  int count_fail_ = 0;
  int count_timeout_ = 0;

  // Maps case index -> tree top-level item
  QMap<int, QTreeWidgetItem*> case_items_;
  // Maps case index -> case name
  QMap<int, QString> case_names_;
  // Flat lookup: "caseIndex/stepPath" -> leaf tree item
  QMap<QString, QTreeWidgetItem*> step_items_;
};

}  // namespace etest::app

#endif  // ETEST_APP_EXECUTION_DEBUG_WIDGET_H_
