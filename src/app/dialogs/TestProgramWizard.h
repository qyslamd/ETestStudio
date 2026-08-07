#pragma once

#include "BaseWizardDialog.h"

#include <QString>
#include <QVector>

namespace etest::app {

struct TestProgramData;

// ── 扁平步骤模型（设计决策 #1，单层嵌套）──
// cmd 为 SET/CHECK/VERIFY/WAIT/DELAY/LOOP/WHILE/IF/ELSE/END_LOOP/END_WHILE/
// END_IF。END_* 与 ELSE 为结构行，仅用于展示与配对维护，翻译时不产出步骤。
struct WizardStep {
  QString cmd;
  QString target;
  QString value;
  QString tolerance;
  QString timeout;
  QString condition;  // ==, !=, >, <, >=, <=（仅 WAIT/WHILE/IF）
  QString loopCount;
  bool parent = false;  // 位于控制流块体内（表格缩进显示）
};

struct WizardCase {
  QString name;
  QVector<WizardStep> steps;
};

// 新建测试程序向导：4 步（模板 / 信息 / 用例与步骤 / 完成），复刻
// docs/prototype/新建 测试程序文件向导设计.html。创建时由 resultProgram()
// 产出完整嵌套 TestProgramData。
class TestProgramWizard : public BaseWizardDialog {
  Q_OBJECT

 public:
  explicit TestProgramWizard(QWidget* parent = nullptr);

  /// 完整嵌套数据（供 TestProgramManagerWidget 保存）
  TestProgramData resultProgram() const;
  QString templateId() const;

 protected:
  /// 创建前结构防御：每个用例经 buildNestedSteps 翻译，失败则弹窗并阻止关闭
  bool onCreateValidate() override;
  void confirmCancel() override;

 private:
  class TemplatePage;
  class ProgramInfoPage;
  class CasesPage;
  class SummaryPage;
  class StepCommandBadge;

  void initUi();
  void initSignals();
  void onTemplateSelected(const QString& templateId);
  void loadTemplate(const QString& templateId);
  void updateSummary();
  /// 模板 ID → 摘要显示名
  static QString templateLabel(const QString& templateId);

  TemplatePage* template_page_ = nullptr;
  ProgramInfoPage* info_page_ = nullptr;
  CasesPage* cases_page_ = nullptr;
  SummaryPage* summary_page_ = nullptr;
  QString template_id_;
};

}  // namespace etest::app
