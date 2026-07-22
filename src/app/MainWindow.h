#ifndef ETEST_APP_MAINWINDOW_H_
#define ETEST_APP_MAINWINDOW_H_

#include <memory>

#include "DockManager.h"
#include "SARibbonMainWindow.h"

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace icd {
class Repository;
}  // namespace icd

class QMenu;
class QAction;
class QLabel;
class QSplitter;
class QStackedWidget;
class QTimer;

namespace etest::app {

class ActivityBarWidget;
class SidebarWidget;
class ExecutionOutputPanel;
class LogOutputPanel;
class SettingsDialog;
class TerminalPanel;
class BottomContainerWidget;
class EditorManager;
class HintBarWidget;
class WelcomeWidget;
class LoadingOverlay;
class TestProgramManagerWidget;
class AppStatusBarController;
class TuxSaverController;
class EditorPanelController;
class ProjectController;
class ExecutionDashboard;
class ExecutionPanelController;
}  // namespace etest::app

namespace etest::engine {
class TestExecutionEngine;
}  // namespace etest::engine

class SARibbonCategory;

namespace etest::app {

class MainWindow : public SARibbonMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  void showEvent(QShowEvent* event) override;

 private:
  void initUi();
  void initSignalsEarly();
  void initSignalsLate();
  void lazyInit();
  void onThemeChanged(bool isDark);

  void saveWindowState();
  void restoreWindowState();

  void setupRibbon();
  void createStatusBar();

  // 项目相关
  bool tryCloseCurrentProject();
  void openRecentProject(const QString& path);
  void onNewProject();
  static QString findProjectFile(const QString& dirPath);
  void onOpenProject();
  void onOpenFile();
  void onCloseProject();
  void onProjectOpened(const QString& projectPath);
  void onProjectClosed();
  void updateWindowTitle();
  void updateRecentProjectsMenu();
  void updateRecentFilesMenu();

  // 编辑器相关
  void onSaveFile();
  void onSaveFileAs();
  void onSaveAllFiles();
  void onCloseCurrentFile();
  void onCloseAllFiles();

  // 编辑操作
  void onUndo();
  void onRedo();
  void onCut();
  void onCopy();
  void onPaste();

  // 搜索替换
  void onFind();
  void onReplace();

  // 跳转到行
  void onGoToLine();

  // 运行态/编辑态 Action 管理
  void disableEditActions();
  void enableEditActions();

  // QADS
  static void setupDockTitleBarButtons(ads::CDockAreaWidget* area);
  ads::CDockManager* dock_manager_ = nullptr;

  // 活动栏 + 侧边栏
  ActivityBarWidget* activity_bar_ = nullptr;
  SidebarWidget* sidebar_ = nullptr;
  TestProgramManagerWidget* test_program_mgr_ = nullptr;

  // 水平/垂直分割器
  QSplitter* h_splitter_ = nullptr;  // 水平：Sidebar / 垂直区域 / AuxSidebar
  QSplitter* v_splitter_ = nullptr;  // 垂直：ContainerWidget / BottomContainer

  // 提示栏
  HintBarWidget* hint_bar_ = nullptr;

  // 编辑器管理
  EditorManager* editor_manager_ = nullptr;

  // 中央堆叠容器（编辑态/运行态）
  QStackedWidget* central_stack_ = nullptr;
  QWidget* page_editor_widget_ = nullptr;  // page 0 编辑态容器
  ExecutionDashboard* exec_dashboard_page_ = nullptr;

  // 欢迎页
  WelcomeWidget* welcome_widget_ = nullptr;
  ads::CDockWidget* central_dock_ = nullptr;

  // 底部面板
  LogOutputPanel* log_panel_ = nullptr;
  ExecutionOutputPanel* execution_output_panel_ = nullptr;
  TerminalPanel* terminal_panel_ = nullptr;
  BottomContainerWidget* bottom_container_ = nullptr;
  int bottom_container_height_ = 200;

  // 辅助侧边栏
  QWidget* aux_sidebar_widget_ = nullptr;
  int aux_sidebar_width_ = 280;

  // 侧边栏展开宽度（用于折叠记忆）
  int sidebar_expanded_width_ = 280;

  // M5/M6: ICD 信号注册表 + Repository（shared_ptr 确保存活）
  etest::core::SignalRegistry* signal_registry_ = nullptr;
  std::shared_ptr<icd::Repository> icd_repository_;

  // 设置对话框（非模态，只创建一次）
  SettingsDialog* settings_dialog_ = nullptr;

  // 菜单和状态
  QMenu* recent_projects_menu_ = nullptr;
  QMenu* recent_files_menu_ = nullptr;
  QAction* view_output_action_ = nullptr;
  QAction* view_terminal_action_ = nullptr;
  QAction* view_aux_sidebar_action_ = nullptr;
  QAction* new_project_action_ = nullptr;
  QAction* open_project_action_ = nullptr;
  QAction* open_file_action_ = nullptr;
  QAction* close_project_action_ = nullptr;
  QAction* save_action_ = nullptr;
  QAction* save_as_action_ = nullptr;
  QAction* save_all_action_ = nullptr;
  QAction* close_file_action_ = nullptr;
  QAction* close_all_files_action_ = nullptr;

  // 子系统 Controller（委托）
  AppStatusBarController* status_bar_ctrl_ = nullptr;
  EditorPanelController* editor_controller_ = nullptr;
  ProjectController* project_controller_ = nullptr;
  ExecutionPanelController* execution_controller_ = nullptr;

  // 编辑菜单动作
  QAction* edit_undo_action_ = nullptr;
  QAction* edit_redo_action_ = nullptr;
  QAction* edit_cut_action_ = nullptr;
  QAction* edit_copy_action_ = nullptr;
  QAction* edit_paste_action_ = nullptr;
  QAction* edit_find_action_ = nullptr;
  QAction* edit_replace_action_ = nullptr;
  QAction* edit_go_to_line_action_ = nullptr;

  // Ribbon category 管理
  SARibbonCategory* category_exec_ = nullptr;
  bool switching_page_ = false;

  // 剪贴板
  QClipboard* clipboard_ = nullptr;

  // Tux 屏保（委托给 TuxSaverController）
  TuxSaverController* tux_controller_ = nullptr;

  // 懒加载覆盖层
  LoadingOverlay* loading_overlay_ = nullptr;

  // 登录认证
  QMenu* login_menu_ = nullptr;
  QAction* login_user_info_action_ = nullptr;
  QAction* login_manage_users_action_ = nullptr;

  // ── 执行引擎（委托给 ExecutionPanelController） ──

  // 附属工具启动
  QAction* tool_topology_action_ = nullptr;
  QAction* tool_protocol_action_ = nullptr;
  QAction* tool_testprogram_action_ = nullptr;

  // 当前编辑器的信号连接
  QMetaObject::Connection current_editor_modification_connection_;
  QMetaObject::Connection current_editor_selection_connection_;
  QMetaObject::Connection current_editor_state_connection_;

  bool first_show_ = true;

  QAction* tool_testexecutor_action_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_MAINWINDOW_H_
