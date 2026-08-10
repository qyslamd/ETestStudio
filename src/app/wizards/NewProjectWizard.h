#pragma once

#include "BaseWizardDialog.h"

namespace etest::app {

// 新建项目向导：4 步（模板 / 信息 / 配置 / 完成），视觉与交互复刻
// temp/向导设计.html。V1 仅消费 name + location（由 ProjectController 调用
// ProjectManager::createProject），模板/版本/描述/开关值记录但不参与创建。
class NewProjectWizard : public BaseWizardDialog {
  Q_OBJECT

 public:
  explicit NewProjectWizard(QWidget* parent = nullptr);

  // ── 创建参数读取（供 ProjectController）──
  QString projectName() const;
  QString projectLocation() const;  // 绝对路径
  QString projectVersion() const;
  QString projectDescription() const;
  QString templateId() const;
  bool createIcdTemplate() const;
  bool enableMockDevice() const;
  bool initGitRepo() const;
  bool generateSampleTests() const;

 private:
  class TemplatePage;
  class ProjectInfoPage;
  class AdvancedConfigPage;
  class SummaryPage;

  void initUi();
  void initSignals();
  /// 刷新第 4 页摘要（进入完成页或任何输入变化时）
  void updateSummary();

  TemplatePage* template_page_ = nullptr;
  ProjectInfoPage* info_page_ = nullptr;
  AdvancedConfigPage* config_page_ = nullptr;
  SummaryPage* summary_page_ = nullptr;
};

}  // namespace etest::app
