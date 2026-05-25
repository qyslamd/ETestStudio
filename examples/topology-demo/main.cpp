#include <QApplication>

#include "MainWindow.h"
#include "ThemeManager.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("topology-demo"));

  // 使用 light 主题（demo 不依赖持久化配置）
  etest::app::ThemeManager::instance().setTheme(QStringLiteral("default"));

  MainWindow window;
  window.resize(1200, 800);
  window.show();

  return app.exec();
}
