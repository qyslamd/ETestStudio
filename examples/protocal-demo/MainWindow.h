#pragma once

#include <QMainWindow>

namespace etest::protocal {
class ProtocalEditorWidget;
}

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private:
  void createMenus();
  void updateWindowTitle();

  etest::protocal::ProtocalEditorWidget* editor_ = nullptr;
};
