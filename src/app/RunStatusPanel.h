#ifndef ETEST_APP_RUN_STATUS_PANEL_H_
#define ETEST_APP_RUN_STATUS_PANEL_H_

#include <QHash>
#include <QWidget>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// RunStatusPanel — 运行状态侧栏
// ══════════════════════════════════════════════════════════════════════════════
// 展示套件/用例/步骤的进度树，底部显示 PASS/FAIL/TIMEOUT 统计。
// 通过 onSuiteStarted / onCaseStarted / onStepFinished 驱动更新。
// ══════════════════════════════════════════════════════════════════════════════
class RunStatusPanel : public QWidget {
  Q_OBJECT

 public:
  explicit RunStatusPanel(QWidget* parent = nullptr);

  // ── 套件开始/结束 ──
  void onSuiteStarted(const QString& name);
  void onSuiteFinished(const QString& name, int pass, int fail);

  // ── 用例开始/结束 ──
  void onCaseStarted(int caseIndex, const QString& name);
  void onCaseFinished(int caseIndex, const QString& name, int result);

  // ── 步骤结果（更新树节点图标和消息） ──
  void onStepFinished(int caseIndex, const QString& stepPath,
                      const QString& status, const QString& message);

  // ── 统计更新 ──
  void updateStats(int pass, int fail, int timeout);

  // ── 重置 ──
  void clearAll();

 private:
  void initUi();
  static QString statusIcon(const QString& status);
  QTreeWidgetItem* findOrCreateStepItem(int caseIndex,
                                        const QString& stepPath);

  QTreeWidget* tree_ = nullptr;
  QLabel* stats_label_ = nullptr;
  // caseIndex → tree top-level item
  QHash<int, QTreeWidgetItem*> case_items_;
  // "caseIndex/stepPath" → leaf item
  QHash<QString, QTreeWidgetItem*> step_items_;

  int pass_count_ = 0;
  int fail_count_ = 0;
  int timeout_count_ = 0;
};

}  // namespace etest::app

#endif  // ETEST_APP_RUN_STATUS_PANEL_H_
