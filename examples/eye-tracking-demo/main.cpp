#include <QApplication>
#include "EyeWidget.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("Eye Tracking Demo");

  EyeWidget w;
  w.setWindowTitle("Interactive Eyes");
  w.resize(400, 300);
  w.show();

  return app.exec();
}
