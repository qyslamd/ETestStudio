#ifndef ETEST_PROGRAM_STEP_TABLE_WIDGET_H_
#define ETEST_PROGRAM_STEP_TABLE_WIDGET_H_

#include <QStyledItemDelegate>
#include <QTableView>

#include "CommandTypeDelegate.h"
#include "SignalSelectionInterface.h"
#include "TestProgramData.h"

class QStandardItemModel;

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace etest::app {

// M5: UUID 显示委托 — 将 UUID hex 解析为可读名称
class UuidDisplayDelegate : public QStyledItemDelegate {
  Q_OBJECT
 public:
  explicit UuidDisplayDelegate(etest::core::SignalRegistry* registry,
                               QObject* parent = nullptr);
  QString displayText(const QVariant& value,
                      const QLocale& locale) const override;

 private:
  etest::core::SignalRegistry* registry_;
};

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

  // ── 只读模式（运行态禁用表格编辑） ──
  void setReadOnly(bool readOnly);

  // ── M0: ISignalSelection 注入 ──
  // nullptr 降级为 QTableView 默认文本编辑
  void setSignalSelection(ISignalSelection* sel) { signal_selection_ = sel; }
  // M5: SignalRegistry 绑定（用于 UUID → 可读名称 resolve）
  void setRegistry(etest::core::SignalRegistry* reg);

 signals:
  void cellDataChanged(int row, int column);
  void stepSelectionChanged();

 private:
  void setupModel();
  void setupView();

  // 扩展数据角色
  static constexpr int kStepDataRole = Qt::UserRole + 1;

  // M0: 拦截 target 列编辑，走 ISignalSelection
  bool edit(const QModelIndex& index, EditTrigger trigger,
            QEvent* event) override;

  QStandardItemModel* model_;
  CommandTypeDelegate::Mode delegateMode_;
  // setRowCount 批量插入时抑制 renumberSteps，最后统一调一次
  bool batch_renumber_ = false;

  // M0: 信号选择器（nullptr = 降级默认文本编辑）
  ISignalSelection* signal_selection_ = nullptr;
  // M5: UUID → 名称 resolve
  etest::core::SignalRegistry* registry_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_STEP_TABLE_WIDGET_H_
