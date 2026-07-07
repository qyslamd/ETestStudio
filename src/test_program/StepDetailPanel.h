#ifndef ETEST_PROGRAM_STEP_DETAIL_PANEL_H_
#define ETEST_PROGRAM_STEP_DETAIL_PANEL_H_

#include <QWidget>

#include "TestProgramData.h"

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QStackedWidget;
class QTableWidget;
class QLabel;
class QPushButton;

namespace etest::app {

// 步骤详情面板：为控制流步骤（LOOP/WHILE/IF）提供子步骤编辑，
// 以及 VERIFY 容差、WAIT 条件、INJECT_FAULT 配置等复杂字段的编辑。
class StepDetailPanel : public QWidget {
  Q_OBJECT

 public:
  explicit StepDetailPanel(QWidget* parent = nullptr);
  ~StepDetailPanel() override;

  // 设置当前编辑的步骤数据
  void setStepData(const TestStepData& step, bool readOnly = false);

  // 获取面板中编辑的步骤数据
  TestStepData stepData() const;

  // 清空面板
  void clear();

 signals:
  void dataChanged();

 private:
  void initUi();

  // 页面索引
  enum PageIndex {
    kPageEmpty = 0,
    kPageSetVerify,
    kPageCondition,
    kPageLoop,
    kPageWhile,
    kPageIf,
    kPageFault,
    kPageActionLog,
    kPageCount
  };

  // 根据命令类型切换到对应的页面
  void switchToPageForCommand(const QString& cmd);

  // 把当前页面的控件值刷回 cache_（切换页前必须调用）
  void writePageToCache();
  // 把 cache_ 写入当前页的控件
  void fillPageFromCache();

  // 创建各页面
  QWidget* createEmptyPage();
  QWidget* createSetVerifyPage();
  QWidget* createConditionPage();
  QWidget* createWhilePage();
  QWidget* createLoopPage();
  QWidget* createIfPage();
  QWidget* createFaultPage();
  QWidget* createActionLogPage();

  // 创建子步骤表格
  QTableWidget* createSubStepTable(QWidget* parent);

  // 始终反映面板内的最新值；setStepData 初始化，切页/控件改动时刷新
  TestStepData cache_;

  QStackedWidget* stack_;

  // Empty page
  QLabel* empty_label_;

  // SetVerify page
  QDoubleSpinBox* tol_min_spin_ = nullptr;
  QDoubleSpinBox* tol_max_spin_ = nullptr;
  QCheckBox* tol_enable_check_ = nullptr;

  // Condition page (WAIT/WHILE/IF condition)
  QLineEdit* cond_target_edit_ = nullptr;
  QComboBox* cond_op_combo_ = nullptr;
  QLineEdit* cond_value_edit_ = nullptr;

  // Loop page
  QLineEdit* loop_count_edit_ = nullptr;
  QLineEdit* loop_interval_edit_ = nullptr;
  QTableWidget* loop_sub_table_ = nullptr;

  // While page
  QLineEdit* while_target_edit_ = nullptr;
  QComboBox* while_op_combo_ = nullptr;
  QLineEdit* while_value_edit_ = nullptr;
  QLineEdit* while_interval_edit_ = nullptr;
  QLineEdit* while_timeout_edit_ = nullptr;
  QTableWidget* while_sub_table_ = nullptr;

  // If page
  QLineEdit* if_target_edit_ = nullptr;
  QComboBox* if_op_combo_ = nullptr;
  QLineEdit* if_value_edit_ = nullptr;
  QTableWidget* if_then_table_ = nullptr;
  QTableWidget* if_else_table_ = nullptr;

  // Fault page
  QLineEdit* fault_type_edit_ = nullptr;
  QLineEdit* fault_value_edit_ = nullptr;

  // ActionLog page
  QLineEdit* action_log_edit_ = nullptr;

  QString current_cmd_;
  bool read_only_ = false;
  bool internal_update_ = false;
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_STEP_DETAIL_PANEL_H_
