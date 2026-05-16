#ifndef ETEST_APP_MAINWINDOW_H_
#define ETEST_APP_MAINWINDOW_H_

#include <QJsonObject>
#include <QMainWindow>
#include <QToolBar>
#include "DockManager.h"

class QMenu;
class QAction;
class QLabel;

namespace etest::topology {
class TopologyEditorWidget;
}

namespace etest::app {

class ActivityBarWidget;
class SidebarWidget;
class OutputPanel;
class ProblemsPanel;
class SettingsWidget;
class TerminalPanel;
class PanelContainerWidget;
class EditorManager;
class WelcomeWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void initUi();
  void initSignals();

  void saveWindowState();
  void restoreWindowState();
  QJsonObject captureSessionData();
  void writeSessionFile(const QJsonObject& data);
  void restoreSession();

  void createMenuBar();
  void createStatusBar();
  void createToolBar();
  void createEditMenu();

  // 项目相关
  bool tryCloseCurrentProject();
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
  ads::CDockWidget* sidebar_dock_ = nullptr;

  // 编辑器管理
  EditorManager* editor_manager_;

  // 欢迎页
  WelcomeWidget* welcome_widget_ = nullptr;

  // 拓扑编辑器
  ads::CDockWidget* topology_dock_ = nullptr;

  // 底部面板
  OutputPanel* output_panel_;
  ProblemsPanel* problems_panel_;
  TerminalPanel* terminal_panel_;
  PanelContainerWidget* panel_container_ = nullptr;

  // 设置对话框（非模态，只创建一次）
  SettingsWidget* settings_dialog_ = nullptr;

  // 菜单和状态
  QMenu* view_menu_ = nullptr;
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

  // 工具栏相关
  QToolBar* file_toolbar_ = nullptr;
  QToolBar* edit_toolbar_ = nullptr;

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