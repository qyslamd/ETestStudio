#pragma once

#include <QAbstractButton>

class QPaintEvent;

namespace etest::app {

// 模板选择卡片（NewProjectWizard / TestProgramWizard 共用）：QAbstractButton
// 派生，自带 checked/clicked，选中态由 QSS `#templateCard:checked` 驱动；配合
// QButtonGroup 实现互斥选择。从 NewProjectWizard.cpp 提取。
class WizardTemplateCard : public QAbstractButton {
  Q_OBJECT

 public:
  WizardTemplateCard(const QString& iconName, const QString& title,
                     const QString& desc, const QString& badge,
                     QWidget* parent = nullptr);

 protected:
  // 委托给 style 绘制，让 QSS 的 background/border/圆角及 :hover/:checked 生效
  void paintEvent(QPaintEvent* event) override;
};

}  // namespace etest::app
