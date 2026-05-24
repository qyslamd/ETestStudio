#include <QApplication>

#include "MainWindow.h"
#include "core/common/ThemeState.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("topology-demo"));

  etest::core::common::setDarkTheme(false);

  MainWindow window;
  window.resize(1200, 800);
  window.show();

  return app.exec();
}
