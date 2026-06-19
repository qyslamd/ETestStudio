#include "WisdomWidget.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("WisdomViewer");

    WisdomWidget w;
    w.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    w.setAttribute(Qt::WA_TranslucentBackground);
    w.setGeometry(200, 150, 600, 400);
    w.show();

    return app.exec();
}
