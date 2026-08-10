#pragma once

#include "BaseWizardDialog.h"

#include <QString>

namespace etest::app {

// 新建文件引导向导：单页选择文件类型，确认后由调用方（MainWindow）派发到
// 对应的具体向导（协议/拓扑/测试程序），复用 ProjectStructureWidget::createNewFile。
class NewFileGuideWizard : public BaseWizardDialog {
  Q_OBJECT

 public:
  explicit NewFileGuideWizard(QWidget* parent = nullptr);

  QString selectedCategoryId() const;
  QString selectedExtension() const;
  QString selectedBaseName() const;

 protected:
  bool onCreateValidate() override;

 private:
  class TypePage;

  void initUi();
  void initSignals();

  TypePage* type_page_ = nullptr;
};

}  // namespace etest::app
