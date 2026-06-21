#include "WisdomWidget.h"

#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("sb_wisdom_demo"));

  QMainWindow window;
  window.setWindowTitle(QStringLiteral("哲思·片刻 Demo"));
  window.resize(1200, 800);

  auto* wisdom = new WisdomWidget(&window);
  window.setCentralWidget(wisdom);
  wisdom->setDarkTheme(true);

  window.show();
  return app.exec();
}
