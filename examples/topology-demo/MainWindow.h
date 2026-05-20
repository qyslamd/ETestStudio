#pragma once

#include <QMainWindow>

namespace etest::topology {
class TopologyEditorWidget;
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

  etest::topology::TopologyEditorWidget* editor_ = nullptr;
};
