#include <QApplication>

#include "MainWindow.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("testprogram-demo"));

  MainWindow window;
  window.resize(1200, 800);
  window.show();

  return app.exec();
}
