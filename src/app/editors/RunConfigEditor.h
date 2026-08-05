#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "IEditor.h"

class QLabel;
class QToolBar;
class QToolButton;

namespace etest::app {

class MonitorConfigDialog;
class VisualizationArea;

// ── 运行配置数据（.erun 文件） ──
// 运行编辑器产出：选择测试程序 + 监听器配置 + 布局 + 运行参数（预留）。
// 纯数据模型，与 UI 解耦，便于独立运行程序消费。
struct RunConfig {
  struct Monitor {
    QString connectionId;
    QString displayMode;
    QString name;
  };
  struct LayoutItem {
    QString connectionId;
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
  };

  QString testProgram;  // 相对路径（同项目内）
  QVector<Monitor> monitors;
  QVector<LayoutItem> layout;
  QJsonObject runParams;  // 预留

  QJsonObject toJson() const;
  bool fromJson(const QJsonObject& obj);
};

// RunConfigEditor -- 运行编辑器（独立编辑器，编辑态 page0）
// 编辑 .erun 运行配置。纯新增骨架：IEditor 接口 + .erun 序列化/反序列化
// + 基础数据展示。可视化区手动布局（VisualizerProxy/resize）后续作为
// 主视图增强接入，不破坏现有 ExecutionDashboard。
class RunConfigEditor : public QWidget, public IEditor {
  Q_OBJECT

 public:
  explicit RunConfigEditor(const QString& id, QWidget* parent = nullptr);
  ~RunConfigEditor() override = default;

  // ── IEditor ──
  QString displayName() const override;
  QString editorId() const override;
  QString editorType() const override;
  QString filePath() const override;
  QWidget* widget() override;
  QObject* signalObject() override;
  bool isModified() const override;
  bool save() override;
  bool saveAs(const QString& path) override;
  bool canUndo() const override;
  bool canRedo() const override;
  void undo() override;
  void redo() override;
  void openFile(const QString& filePath) override;

  const RunConfig& config() const { return config_; }

 signals:
  void modificationChanged(bool modified);

 private:
  void initUi();
  void refreshUi();
  void onAddMonitorClicked();
  QList<QPair<QString, QString>> loadConnectionsFromProject() const;
  void markModified();
  bool loadFromFile(const QString& path);
  bool saveToFile(const QString& path);

  QString file_path_;
  bool modified_ = false;
  RunConfig config_;

  // 主视图：可视化区（编辑态，监听器卡片 + 手动布局）
  QToolBar* toolbar_ = nullptr;
  QLabel* file_label_ = nullptr;
  QLabel* test_program_label_ = nullptr;
  VisualizationArea* vis_area_ = nullptr;
  MonitorConfigDialog* channel_dialog_ = nullptr;  // 复用 page1 通道选择对话框
  QToolButton* align_btn_ = nullptr;   // 排列（选中≥2 才启用）
  QToolButton* dist_btn_ = nullptr;    // 分布
};

}  // namespace etest::app
