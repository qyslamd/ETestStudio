#include <QApplication>

#include "ProtocalEditorWidget.h"

int main(int argc, char* argv[]) {
  QApplication a(argc, argv);

  etest::protocal::ProtocalEditorWidget w;
  w.resize(600, 400);
  w.show();

  return a.exec();
}
