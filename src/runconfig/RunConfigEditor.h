#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

#include "IEditor.h"
#include "IEditorCommands.h"
#include "RunConfig.h"

class QAction;
class QDockWidget;
class QMenu;
class QToolBar;
class QToolButton;

namespace etest::runconfig {
class VisualizationArea;
}  // namespace etest::runconfig

namespace etest::runconfig {

class MonitorPropertyWidget;
class ProgramChecklistWidget;
class VisualizerPaletteWidget;

// RunConfigEditor -- 运行编辑器（独立编辑器，编辑态 page0）
// 编辑 .erun 运行配置。纯新增骨架：IEditor 接口 + .erun 序列化/反序列化
// + 基础数据展示。可视化区手动布局（VisualizerProxy/resize）后续作为
// 主视图增强接入，不破坏现有 ExecutionDashboard。
class RunConfigEditor : public QMainWindow,
                        public etest::app::IEditor,
                        public etest::app::IEditorCommandSource {
  Q_OBJECT

 public:
  explicit RunConfigEditor(const QString& id, QWidget* parent = nullptr);
  ~RunConfigEditor() override;

  // ── IEditor ──
  QString displayName() const override;
  QString editorId() const override;
  QString editorType() const override;
  QString filePath() const override;
  QWidget* widget() override;
  QObject* signalObject() override;

  // IEditorCommandSource
  QList<etest::app::EditorCommand> editorCommands() override;
  QObject* commandStateObject() override;

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
  void undoStateChanged();
  void commandsChanged();

 protected:
  // 拦截 dock 关闭按钮，同步工具栏 toggle 勾选态（与三编辑器统一）
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  void initUi();
  void refreshUi();
  // 拖放：可视化组件 visualizer 拖入 → 新建未绑定卡片（UUID/displayMode/sizeHint/位置）
  void addMonitorFromDrop(const QString& displayMode, const QPointF& scenePos);
  // 删除卡片（右键 visualizerRemoved / 属性面板按钮双入口收敛）
  void removeMonitorById(const QString& id);
  // 场景选中变化 → 属性面板加载 / 清空
  void refreshPropertyPanel();
  // 重建后重新选中卡片（绑定/切类型后保持属性面板焦点）
  void selectMonitorCard(const QString& id);
  // 保存到当前项目 run/ 下时，将 .erun 设为当前运行配置（写 .etproj settings）
  void syncRunConfigRef(const QString& path);
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
  QDockWidget* palette_dock_ = nullptr;       // 可视化组件（visualizer 拖放源）
  VisualizerPaletteWidget* palette_widget_ = nullptr;
  QDockWidget* property_dock_ = nullptr;      // 属性面板（选中卡片加载）
  MonitorPropertyWidget* property_widget_ = nullptr;
  etest::runconfig::VisualizationArea* vis_area_ = nullptr;
  QToolButton* align_btn_ = nullptr;   // 排列（选中≥2 才启用）
  QToolButton* dist_btn_ = nullptr;    // 分布
  QMenu* align_menu_ = nullptr;        // 排列下拉菜单（命令定义模式复用）
  QMenu* dist_menu_ = nullptr;         // 分布下拉菜单（命令定义模式复用）

  // 工具栏 actions（存成员供 reloadToolbarIcons 重设图标）
  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;
  QAction* new_action_ = nullptr;
  QAction* save_action_ = nullptr;
  QAction* test_program_toggle_action_ = nullptr;  // 显示/隐藏测试程序面板
  QAction* palette_toggle_action_ = nullptr;       // 显示/隐藏可视化组件 dock
  QAction* property_toggle_action_ = nullptr;      // 显示/隐藏属性面板 dock
};

}  // namespace etest::runconfig
