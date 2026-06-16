#include "TopologySceneRenderer.h"

#include <QFileInfo>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QPrinter>
#include <QSvgGenerator>

namespace etest::topology {

static QRectF paddedSceneRect(QGraphicsScene* scene) {
  QRectF r = scene->sceneRect();
  if (r.isEmpty())
    return r;
  return r.adjusted(-30, -30, 30, 30);
}

static void renderScene(QGraphicsScene* scene, QPainter* painter,
                        const QRectF& sr) {
  painter->setRenderHint(QPainter::Antialiasing);
  scene->render(painter, QRectF(), sr);
}

bool renderSceneToFile(QGraphicsScene* scene, const QString& filePath) {
  if (!scene)
    return false;

  QRectF sr = paddedSceneRect(scene);
  if (sr.isEmpty())
    return false;

  QString suffix = QFileInfo(filePath).suffix().toLower();

  if (suffix == QStringLiteral("png")) {
    QImage image(sr.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(qRgb(255, 255, 255));
    {
      QPainter painter(&image);
      renderScene(scene, &painter, sr);
    }
    return image.save(filePath, "PNG");
  }

  if (suffix == QStringLiteral("svg")) {
    QSvgGenerator gen;
    gen.setFileName(filePath);
    gen.setSize(sr.size().toSize());
    gen.setTitle(QStringLiteral("拓扑图"));
    {
      QPainter painter(&gen);
      painter.setRenderHint(QPainter::Antialiasing);
      // 白底
      painter.fillRect(QRectF(0, 0, sr.width(), sr.height()), Qt::white);
      // scene->render 自动将 sr 映射到 painter 设备坐标
      scene->render(&painter, QRectF(), sr);
    }
    return true;
  }

  if (suffix == QStringLiteral("pdf")) {
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSizeMM(
        QSizeF(sr.width() * 25.4 / printer.resolution(),
               sr.height() * 25.4 / printer.resolution()));
    {
      QPainter painter(&printer);
      renderScene(scene, &painter, sr);
    }
    return true;
  }

  return false;
}

}  // namespace etest::topology
