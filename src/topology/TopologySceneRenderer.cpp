#include "TopologySceneRenderer.h"

#include <QFileInfo>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QPageSize>
#include <QPdfWriter>
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
      QPainter painter;
      if (!painter.begin(&gen))
        return false;
      painter.setRenderHint(QPainter::Antialiasing);
      // 白底
      painter.fillRect(QRectF(0, 0, sr.width(), sr.height()), Qt::white);
      // scene->render 自动将 sr 映射到 painter 设备坐标
      scene->render(&painter, QRectF(), sr);
      painter.end();
    }
    QFileInfo out(filePath);
    return out.exists() && out.size() > 0;
  }

  if (suffix == QStringLiteral("pdf")) {
    // 使用 QPdfWriter 直接写 PDF 文件，不经过 Windows 打印后台处理程序，
    // 避免在无打印机驱动环境下弹出「正在等待打印机连接」对话框。
    QPdfWriter writer(filePath);
    const int dpi = 120;
    writer.setResolution(dpi);
    // 按场景尺寸与分辨率换算页面毫米尺寸，与位图/SVG 比例一致
    QSizeF sizeMM(sr.width() * 25.4 / dpi, sr.height() * 25.4 / dpi);
    writer.setPageSize(QPageSize(sizeMM, QPageSize::Millimeter));
    {
      QPainter painter;
      if (!painter.begin(&writer))
        return false;
      renderScene(scene, &painter, sr);
      painter.end();
    }
    QFileInfo out(filePath);
    return out.exists() && out.size() > 0;
  }

  return false;
}

}  // namespace etest::topology
