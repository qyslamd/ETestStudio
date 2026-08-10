#pragma once

#include "BaseWizardDialog.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace etest::topology {
class TopologyDocument;
}

namespace etest::app {

// ── 扁平拓扑模型（向导内部数据，resultDocument() 时转为 TopologyDocument）──
// 设备由内置 5 类 mock 模板生成（决策 2）；UUT 端口方向恒为 Bidirectional，
// 建线时把设备 deviceType 并入端口 allowedDeviceTypes，保证 canConnect 通过
// （决策 7）。
struct WizardDevice {
  QString name;
  QString deviceType;
  QString pluginId;
  QStringList ports;
};

struct WizardUut {
  QString name;
  QStringList ports;
  QList<QStringList> portAllowedTypes;  // 与 ports 一一对应
};

struct WizardConnection {
  QString deviceName;
  QString devicePort;
  QString uutName;
  QString uutPort;
};

struct TopologyData {
  QList<WizardDevice> devices;
  QList<WizardUut> uuts;
  QList<WizardConnection> connections;
};

// 新建拓扑文件向导：4 步（模板 / 设备&UUT / 连线 / 完成），复刻
// docs/prototype/新建拓扑文件向导设计.html。创建时由 resultDocument() 产出
// 完整 TopologyDocument，调用方经 TopologyJsonSerializer 写 .etopo。
class TopologyFileWizard : public BaseWizardDialog {
  Q_OBJECT

 public:
  explicit TopologyFileWizard(QWidget* parent = nullptr);

  /// 拓扑名称（第 1 页输入框），调用方据此命名 .etopo 文件
  QString topologyName() const;
  /// 构建并返回拓扑文档；所有权归向导，调用方仅读取（向导随栈销毁）
  etest::topology::TopologyDocument* resultDocument();

 protected:
  /// 创建前整体校验：拓扑名称合法
  bool onCreateValidate() override;
  void confirmCancel() override;

 private:
  class TemplatePage;
  class DeviceUutPage;
  class ConnectionPage;
  class SummaryPage;

  void initUi();
  void initSignals();
  void onTemplateSelected(const QString& templateId);
  void loadTemplate(const QString& templateId);
  void updateSummary();
  static QString templateLabel(const QString& templateId);

  TemplatePage* template_page_ = nullptr;
  DeviceUutPage* device_uut_page_ = nullptr;
  ConnectionPage* connection_page_ = nullptr;
  SummaryPage* summary_page_ = nullptr;
  TopologyData data_;
  QString template_id_;
};

}  // namespace etest::app
