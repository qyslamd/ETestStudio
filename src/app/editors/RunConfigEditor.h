#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QVector>

#include "IEditor.h"

class QAction;
class QDockWidget;
class QToolBar;
class QToolButton;

namespace etest::app {

class MonitorConfigDialog;
class ProgramChecklistWidget;
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

  QStringList programs;  // 测试程序（相对项目根路径），与监听/布局正交
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
class RunConfigEditor : public QMainWindow, public IEditor {
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

  // 嵌入 IDE 模式：隐藏 QMainWindow 菜单栏（独立运行时显示）
  void setEmbeddedMode(bool embedded);
  // 主题切换时重载工具栏图标（AppIconProvider 按主题变体取图）
  void reloadToolbarIcons();

 signals:
  void modificationChanged(bool modified);

 protected:
  // 拦截 dock 关闭按钮，同步工具栏 toggle 勾选态（与三编辑器统一）
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  void initUi();
  void refreshUi();
  void onAddMonitorClicked();
  // 从 .erun 所在目录向上找含 topology 的项目根
  QString findProjectRoot() const;
  QList<QPair<QString, QString>> loadConnectionsFromProject() const;
  void markModified();
  bool loadFromFile(const QString& path);
  bool saveToFile(const QString& path);

  // ── 撤销/重做（快照式，仿 ProtocolEditorWidget） ──
  void saveSnapshot();
  void restoreSnapshot(const QByteArray& data);
  void collectLayout();
  void updateUndoRedoActions();

  static constexpr int kMaxSnapshots = 32;
  QVector<QByteArray> snapshots_;
  int snapshot_index_ = -1;

  QString file_path_;
  bool modified_ = false;
  bool embedded_ = false;
  RunConfig config_;

  // 主视图：可视化区（编辑态，监听器卡片 + 手动布局）
  QToolBar* toolbar_ = nullptr;
  QDockWidget* test_program_dock_ = nullptr;  // 测试程序多选面板（可关可拖，toggle 重开）
  ProgramChecklistWidget* program_list_ = nullptr;
  VisualizationArea* vis_area_ = nullptr;
  MonitorConfigDialog* channel_dialog_ = nullptr;  // 复用 page1 通道选择对话框
  QToolButton* align_btn_ = nullptr;   // 排列（选中≥2 才启用）
  QToolButton* dist_btn_ = nullptr;    // 分布

  // 工具栏 actions（存成员供 reloadToolbarIcons 重设图标）
  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;
  QAction* new_action_ = nullptr;
  QAction* save_action_ = nullptr;
  QAction* add_monitor_action_ = nullptr;
  QAction* test_program_toggle_action_ = nullptr;  // 显示/隐藏测试程序面板
};

}  // namespace etest::app
