#pragma once

#include "BaseWizardDialog.h"

#include <QList>
#include <QString>

#include <icd/frame.hpp>

namespace etest::app {

// ── 扁平字段模型（设计决策 #5/#9）──
// offset 为绝对位偏移（HTML 语义），映射 icd::Node 时按 bit/8 拆分。
struct WizardField {
  QString name;
  int offset = 0;
  int width = 8;
  QString type = QStringLiteral("uint8");
  QString scale = QStringLiteral("1.0");
  QString unit;
};

// 新建协议文件向导：4 步（模板 / 帧属性 / 字段定义 / 完成），复刻
// docs/prototype/新建协议文件向导设计.html。创建时由 resultFrame() 产出
// 完整 icd::Frame（含 roots=Nodes），调用方包 Repository 后经
// serialize_xml_repository 写 .eprotox。
class ProtocolFileWizard : public BaseWizardDialog {
  Q_OBJECT

 public:
  explicit ProtocolFileWizard(QWidget* parent = nullptr);

  /// 完整单帧（含 roots=Nodes），供调用方包 Repository 序列化
  icd::Frame resultFrame() const;
  QString templateId() const;

 protected:
  /// 创建前整体校验：逐行校验字段行，失败红闪定位并阻止关闭
  bool onCreateValidate() override;
  void confirmCancel() override;

 private:
  class TemplatePage;
  class FrameInfoPage;
  class FieldPage;
  class SummaryPage;

  void initUi();
  void initSignals();
  void onTemplateSelected(const QString& templateId);
  void loadTemplate(const QString& templateId);
  void updateSummary();
  /// 模板 ID → 摘要显示名
  static QString templateLabel(const QString& templateId);

  TemplatePage* template_page_ = nullptr;
  FrameInfoPage* info_page_ = nullptr;
  FieldPage* field_page_ = nullptr;
  SummaryPage* summary_page_ = nullptr;
  QString template_id_;
};

}  // namespace etest::app
