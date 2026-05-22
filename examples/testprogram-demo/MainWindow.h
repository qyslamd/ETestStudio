#pragma once

#include <QMainWindow>

namespace etest::app {
class TestProgramEditorWidget;
}

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private slots:
  void onNew();
  void onOpen();
  void onSave();
  void onSaveAs();

 private:
  void createMenus();
  bool confirmSave();
  void updateWindowTitle();

  etest::app::TestProgramEditorWidget* editor_ = nullptr;
};
