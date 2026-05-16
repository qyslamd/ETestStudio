#include <QApplication>
#include "TopologyEditorWidget.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("topology-demo"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    etest::topology::TopologyEditorWidget w;
    w.show();

    return app.exec();
}
