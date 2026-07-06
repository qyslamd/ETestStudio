#ifndef ETEST_PROGRAM_STEP_TABLE_WIDGET_H_
#define ETEST_PROGRAM_STEP_TABLE_WIDGET_H_

#include <QTableView>

#include "CommandTypeDelegate.h"
#include "TestProgramData.h"

class QStandardItemModel;

namespace etest::app {

// 步骤表格控件 — 替代 QTableWidget，使用 QTableView + QStandardItemModel，
// 封装步骤编辑所需的扩展数据存取、拖拽排序等功能。列头固定，不随命令类型变化。
class StepTableWidget : public QTableView {
  Q_OBJECT

 public:
  // 列索引常量
  enum Col {
    kColDesc = 0,     // 步骤说明
    kColCmd = 1,      // 命令
    kColTarget = 2,   // 目标 / 条件目标 / 期望值
    kColValue = 3,    // 值 / 运算符 / 故障类型 / 期望值
    kColExtra = 4,    // 延迟ms / 容差min / 条件值 / 故障值（按命令，固定列）
    kColExtra2 = 5,   // 容差max / 间隔ms（按命令，固定列）
    kColTimeout = 6,  // 超时ms
    kColCount = 7
  };

  explicit StepTableWidget(CommandTypeDelegate::Mode delegateMode,
                           QWidget* parent = nullptr);
  ~StepTableWidget() override = default;

  // ── 行操作 ──
  int rowCount() const;
  void setRowCount(int rows);
  void removeRow(int row);
  int currentRow() const;
  void selectRow(int row);

  // ── 单元格读写（通过 model data 操作，避免 QTableWidgetItem 开销） ──
  void setCellText(int row, int col, const QString& text);
  QString cellText(int row, int col) const;
  void setCellData(int row, int col, const QVariant& value, int role);
  QVariant cellData(int row, int col, int role) const;

  // ── 步骤扩展数据（条件/容差/故障/循环参数/子步骤，序列化存入 UserRole） ──
  void setStepExtData(int row, const TestStepData& step);
  TestStepData stepExtData(int row) const;

  // ── 重新编号（vertical header） ──
  void renumberSteps();

  // ── 按当前字体刷新行高（主题切换/构造后调用，避免 cell editor 字体被截断） ──
  void refreshRowHeight();

 signals:
  void cellDataChanged(int row, int column);
  void stepSelectionChanged();

 private:
  void setupModel();
  void setupView();

  // 扩展数据角色
  static constexpr int kStepDataRole = Qt::UserRole + 1;

  QStandardItemModel* model_;
  CommandTypeDelegate::Mode delegateMode_;
  // setRowCount 批量插入时抑制 renumberSteps，最后统一调一次
  bool batch_renumber_ = false;
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_STEP_TABLE_WIDGET_H_
