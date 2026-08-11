#pragma once

#include "wizards/BaseWizardDialog.h"

class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;

namespace etest::app {

// 快速开始向导（UI 空壳，D1/D3）：引导几步创建测试工程。
// 三页：设备模板选择 / 项目信息 / 摘要。本期仅界面与导航，完成/取消不创建，
// 功能后接映射到 ProjectController::newProject。
class QuickStartWizard : public BaseWizardDialog {
  Q_OBJECT

 public:
  explicit QuickStartWizard(QWidget* parent = nullptr);

 protected:
  bool onCreateValidate() override;

 private:
  // 页 1：设备模板选择（占位卡片）
  class TemplatePage : public WizardPage {
   public:
    explicit TemplatePage(QWidget* parent = nullptr);
    QString selectedTemplateName() const;

   private:
    void initUi();
    QButtonGroup* group_ = nullptr;
  };
  // 页 2：项目信息（占位控件，无实际创建）
  class InfoPage : public WizardPage {
   public:
    explicit InfoPage(QWidget* parent = nullptr);
    QString projectName() const;

   private:
    void initUi();
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* location_edit_ = nullptr;
  };
  // 页 3：摘要（占位）
  class SummaryPage : public WizardPage {
   public:
    explicit SummaryPage(QWidget* parent = nullptr);
    void setSummary(const QString& text);

   private:
    void initUi();
    QLabel* summary_label_ = nullptr;
  };

  TemplatePage* template_page_ = nullptr;
  InfoPage* info_page_ = nullptr;
  SummaryPage* summary_page_ = nullptr;
};

}  // namespace etest::app
