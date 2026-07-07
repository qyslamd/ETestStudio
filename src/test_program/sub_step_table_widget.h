#ifndef ETEST_PROGRAM_SUB_STEP_TABLE_WIDGET_H_
#define ETEST_PROGRAM_SUB_STEP_TABLE_WIDGET_H_

#include <QVector>
#include <QWidget>

#include "TestProgramData.h"

class QTableWidget;
class QPushButton;

namespace etest::app {

// 子步骤表控件：带工具栏（添加/删除/上移/下移）的 QTableWidget 封装，
// 用于 LOOP/WHILE/IF 的循环体/分支步骤编辑。6 列固定（步骤说明/命令/目标/值/延迟/容差），
// 命令列用 FlatOnly 委托，子步骤不含控制流，保证不嵌套。
class SubStepTableWidget : public QWidget {
  Q_OBJECT

 public:
  explicit SubStepTableWidget(QWidget* parent = nullptr);
  ~SubStepTableWidget() override = default;

  void setSubSteps(const QVector<TestStepData>& steps);
  QVector<TestStepData> subSteps() const;
  void setReadOnly(bool ro);

 signals:
  // 行增删移动 / 单元格编辑都发。程序化填充（setSubSteps）不发。
  void subStepsChanged();

 private slots:
  void onAdd();
  void onRemove();
  void onMoveUp();
  void onMoveDown();
  void onDoubleClicked(const QModelIndex& index);

 private:
  void initUi();
  void initSignals();
  void updateButtonStates();
  // 逐 cell 交换两行文本（不交换 item 指针，避免 delegate/选中状态错乱）
  void swapRows(int rowA, int rowB);
  // 刷新指定行容差列的显示文本
  void refreshToleranceCell(int row);

  QTableWidget* table_;
  QPushButton* add_btn_;
  QPushButton* remove_btn_;
  QPushButton* up_btn_;
  QPushButton* down_btn_;
  bool read_only_ = false;
  // 每行的容差配置（按行索引，与 table_ 行同步增删移动）
  QVector<ToleranceSpec> tolerances_;
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_SUB_STEP_TABLE_WIDGET_H_
