#pragma once

#include <QString>

class QGraphicsScene;
class QWidget;

namespace etest::topology {

class TopologyExportController {
 public:
  static QString completeFilePath(QString filePath,
                                  const QString& selectedFilter);
  static bool exportScene(QWidget* parent, QGraphicsScene* scene,
                          QString* exportedPath = nullptr);
};

}  // namespace etest::topology
