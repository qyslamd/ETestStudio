#ifndef ETEST_APP_MAINWINDOW_H_
#define ETEST_APP_MAINWINDOW_H_

#include <QMainWindow>
#include "DockManager.h"

class ActivityBarWidget;
class SidebarWidget;
class OutputPanel;
class ProblemsPanel;
class TerminalPanel;

class QMenu;
class QAction;
class QLabel;

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

  void createMenuBar();
  void createStatusBar();

  // 项目相关
  void onNewProject();
  void onOpenProject();
  void onCloseProject();
  void onProjectOpened(const QString& projectPath);
  void onProjectClosed();
  void updateWindowTitle();
  void updateRecentProjectsMenu();

  // QADS
  ads::CDockManager* dock_manager_;

  // 活动栏 + 侧边栏
  ActivityBarWidget* activity_bar_;
  SidebarWidget* sidebar_;

  // 底部面板
  OutputPanel* output_panel_;
  ProblemsPanel* problems_panel_;
  TerminalPanel* terminal_panel_;

  // 菜单和状态
  QMenu* recent_projects_menu_ = nullptr;
  QAction* close_project_action_ = nullptr;
  QLabel* status_project_label_ = nullptr;
};

#endif  // ETEST_APP_MAINWINDOW_H_
