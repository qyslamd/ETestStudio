#ifndef ETEST_APP_MAINWINDOW_H_
#define ETEST_APP_MAINWINDOW_H_

#include "SARibbonMainWindow.h"
#include "DockManager.h"

class QMenu;
class QAction;
class QLabel;
class QSplitter;

namespace etest::app {

class ActivityBarWidget;
class SidebarWidget;
class OutputPanel;
class ProblemsPanel;
class SettingsDialog;
class TerminalPanel;
class BottomContainerWidget;
class EditorManager;
class WelcomeWidget;

class MainWindow : public SARibbonMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void applyTheme();

  void saveWindowState();
  void restoreWindowState();

  void setupRibbon();
  void createStatusBar();

  // 项目相关
  bool tryCloseCurrentProject();
  void openRecentProject(const QString& path);
  void onNewProject();
  void onOpenProject();
  void onCloseProject();
  void onProjectOpened(const QString& projectPath);
  void onProjectClosed();
  void updateWindowTitle();
  void updateRecentProjectsMenu();

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

  // QADS
  static void hideDockTitleBarButtons(ads::CDockAreaWidget* area);
  ads::CDockManager* dock_manager_;

  // 活动栏 + 侧边栏
  ActivityBarWidget* activity_bar_;
  SidebarWidget* sidebar_;

  // 水平/垂直分割器
  QSplitter* h_splitter_;  // 水平：Sidebar / 垂直区域 / AuxSidebar
  QSplitter* v_splitter_;  // 垂直：CDockManager / BottomContainer

  // 编辑器管理
  EditorManager* editor_manager_;

  // 欢迎页
  WelcomeWidget* welcome_widget_ = nullptr;
  ads::CDockWidget* central_dock_ = nullptr;

  // 底部面板
  OutputPanel* output_panel_;
  ProblemsPanel* problems_panel_;
  TerminalPanel* terminal_panel_;
  BottomContainerWidget* bottom_container_ = nullptr;
  int bottom_container_height_ = 200;

  // 辅助侧边栏
  QWidget* aux_sidebar_widget_ = nullptr;
  int aux_sidebar_width_ = 280;

  // 侧边栏展开宽度（用于折叠记忆）
  int sidebar_expanded_width_ = 280;

  // 设置对话框（非模态，只创建一次）
  SettingsDialog* settings_dialog_ = nullptr;

  // 菜单和状态
  QMenu* recent_projects_menu_ = nullptr;
  QAction* view_panel_action_ = nullptr;
  QAction* view_aux_sidebar_action_ = nullptr;
  QAction* new_project_action_ = nullptr;
  QAction* open_project_action_ = nullptr;
  QAction* close_project_action_ = nullptr;
  QAction* save_action_ = nullptr;
  QAction* save_as_action_ = nullptr;
  QAction* save_all_action_ = nullptr;
  QAction* close_file_action_ = nullptr;
  QAction* close_all_files_action_ = nullptr;

  // 状态栏标签
  QLabel* status_message_label_ = nullptr;
  QLabel* status_project_label_ = nullptr;
  QLabel* status_errors_label_ = nullptr;
  QLabel* status_cursor_label_ = nullptr;
  QLabel* status_encoding_label_ = nullptr;
  QLabel* status_eol_label_ = nullptr;
  QLabel* status_language_label_ = nullptr;

  // 编辑菜单动作
  QAction* edit_undo_action_ = nullptr;
  QAction* edit_redo_action_ = nullptr;
  QAction* edit_cut_action_ = nullptr;
  QAction* edit_copy_action_ = nullptr;
  QAction* edit_paste_action_ = nullptr;
  QAction* edit_find_action_ = nullptr;
  QAction* edit_replace_action_ = nullptr;
  QAction* edit_go_to_line_action_ = nullptr;

  // 剪贴板
  QClipboard* clipboard_ = nullptr;

  // 当前编辑器的信号连接
  QMetaObject::Connection current_editor_modification_connection_;
  QMetaObject::Connection current_editor_selection_connection_;
  QMetaObject::Connection current_editor_state_connection_;
};

}  // namespace etest::app

#endif  // ETEST_APP_MAINWINDOW_H_
