#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <Qsci/qsciscintilla.h>
#include "DockManager.h"

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private:
  // 初始化界面元素
  void initUi();
  // 初始化信号槽连接
  void initSignals();

  // QADS停靠管理器
  ads::CDockManager* dock_manager_;
  // QScintilla测试编辑器
  QsciScintilla* m_editor;
};

#endif // MAINWINDOW_H
