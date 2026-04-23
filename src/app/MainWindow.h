#ifndef ETEST_APP_MAINWINDOW_H_
#define ETEST_APP_MAINWINDOW_H_

#include <QMainWindow>
#include "DockManager.h"

class ActivityBarWidget;
class SidebarWidget;
class OutputPanel;
class ProblemsPanel;
class TerminalPanel;

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

  // QADS
  ads::CDockManager* dock_manager_;

  // 活动栏 + 侧边栏
  ActivityBarWidget* activity_bar_;
  SidebarWidget* sidebar_;

  // 底部面板
  OutputPanel* output_panel_;
  ProblemsPanel* problems_panel_;
  TerminalPanel* terminal_panel_;
};

#endif  // ETEST_APP_MAINWINDOW_H_
