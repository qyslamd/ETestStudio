#pragma once

#include "OverlayDialog.h"

#include <QString>

class QCheckBox;
class QComboBox;
class QKeyEvent;
class QLineEdit;
class QPushButton;
class QWidget;

namespace etest::app {

// 步骤编辑结果：命令类型 + 各字段原文（不做嵌套翻译，由向导转换为扁平模型行）。
struct StepEditResult {
  QString cmd;           // SET/CHECK/VERIFY/WAIT/DELAY/LOOP/WHILE/IF
  QString target;
  QString value;
  QString tolerance;
  QString timeout;
  QString condition;     // ==, !=, >, <, >=, <=（仅 WAIT/WHILE/IF）
  QString loopCount;
  bool includeElse = false;  // 仅 IF：是否包含 ELSE 分支
};

// 添加/编辑测试步骤模态框（对象名 stepModal）。字段可见性按命令类型联动（复刻
// HTML updateModalFields）；处于控制流块体内时禁用 LOOP/WHILE/IF 命令项（叶子
// 命令含 CHECK 仍可用）。Enter=确认、Esc=取消。
class StepEditDialog : public OverlayDialog {
  Q_OBJECT

 public:
  explicit StepEditDialog(QWidget* parent = nullptr);

  /// 打开前配置。editing: true=编辑模式（预填 + 「保存修改」），false=添加模式；
  /// insideBlock: 插入点处于控制流块体内（禁用 LOOP/WHILE/IF）；initial: 编辑
  /// 模式预填的当前值（添加模式传默认构造即可）。
  void configure(bool editing, bool insideBlock, const StepEditResult& initial);

  /// 编辑结果（configure 后、accept 后读取）
  StepEditResult result() const;

 protected:
  void initUi();
  void initSignals();
  /// 命令类型变化 → 联动字段行可见性
  void onCmdChanged(const QString& cmd);
  /// 必填字段校验；缺失时聚焦对应输入框，返回 false 阻止关闭
  bool validateInputs();
  /// 当前命令类型（combo 当前项）
  QString currentCmd() const;
  void keyPressEvent(QKeyEvent* event) override;

  QComboBox* cmd_combo_ = nullptr;
  QLineEdit* target_edit_ = nullptr;
  QLineEdit* value_edit_ = nullptr;
  QLineEdit* tolerance_edit_ = nullptr;
  QLineEdit* timeout_edit_ = nullptr;
  QComboBox* condition_combo_ = nullptr;
  QLineEdit* loop_count_edit_ = nullptr;
  QCheckBox* else_checkbox_ = nullptr;
  QPushButton* confirm_btn_ = nullptr;
  QPushButton* cancel_btn_ = nullptr;

  // 可见性联动用行容器
  QWidget* target_row_ = nullptr;
  QWidget* value_row_ = nullptr;
  QWidget* tolerance_col_ = nullptr;
  QWidget* extra_row_ = nullptr;  // 超时 + 条件
  QWidget* timeout_col_ = nullptr;
  QWidget* condition_col_ = nullptr;
  QWidget* loop_row_ = nullptr;
  QWidget* else_row_ = nullptr;
};

}  // namespace etest::app
