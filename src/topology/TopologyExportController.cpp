#include "TopologyExportController.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsScene>

#include "TopologySceneRenderer.h"

namespace etest::topology {

QString TopologyExportController::completeFilePath(
    QString filePath, const QString& selectedFilter) {
  if (!QFileInfo(filePath).suffix().isEmpty())
    return filePath;

  if (selectedFilter.contains(QStringLiteral("SVG")))
    return filePath + QStringLiteral(".svg");
  if (selectedFilter.contains(QStringLiteral("PDF")))
    return filePath + QStringLiteral(".pdf");
  return filePath + QStringLiteral(".png");
}

bool TopologyExportController::exportScene(QWidget* parent,
                                           QGraphicsScene* scene,
                                           QString* exportedPath) {
  QString filter =
      QStringLiteral("PNG 图片 (*.png);;SVG 矢量图 (*.svg);;PDF 文档 (*.pdf)");
  QString selectedFilter;
  QString path = QFileDialog::getSaveFileName(
      parent, QStringLiteral("导出拓扑图"), QString(), filter, &selectedFilter);
  if (path.isEmpty())
    return false;

  path = completeFilePath(path, selectedFilter);
  if (exportedPath)
    *exportedPath = path;
  return renderSceneToFile(scene, path);
}

}  // namespace etest::topology
