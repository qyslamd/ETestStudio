#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 静态库模式下需要显式初始化 SARibbon 的 Qt 资源（主题 QSS 文件等）
    Q_INIT_RESOURCE(SARibbonResource);

    MainWindow win;
    win.show();

    return app.exec();
}
