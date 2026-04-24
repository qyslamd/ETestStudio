#include "utility_qt.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonParseError>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QtDebug>
#include <QtMath>
#include <algorithm>
#include <exception>

namespace utility_qt {

QSpinBox* createSpinBox(bool readonly, bool noButtonSymbol) {
  QSpinBox* spinBox = new QSpinBox;
  spinBox->setMinimumWidth(100);
  spinBox->setMinimum(INT32_MIN);
  spinBox->setMaximum(INT32_MAX);
  spinBox->setValue(0x0);
  spinBox->setEnabled(!readonly);
  if (noButtonSymbol) {
    spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
  }
  return spinBox;
}

QSpinBox* createHexSpinBox(bool readonly, bool noButtonSymbol) {
  auto spinBox = createSpinBox(readonly, noButtonSymbol);
  spinBox->setPrefix("0x");
  spinBox->setDisplayIntegerBase(16);
  return spinBox;
}

QGraphicsDropShadowEffect* createGraphicsShadow(QWidget* widget,
                                                const QColor& color,
                                                int blurRadius) {
  auto shadow = new QGraphicsDropShadowEffect(widget);
  shadow->setOffset(0, 0);
  shadow->setBlurRadius(blurRadius);
  shadow->setColor(color);
  //  shadow->setColor(QColor("#696e7ff3"));
  widget->setGraphicsEffect(shadow);
  return shadow;
};

bool createFile(const QString& path) {
  // 没有实现好，不要用
  return false;

  QFileInfo info(path);               // 获取QFileInFo
  QDir fileDir = info.absoluteDir();  // 获取文件所在的目录
  if (!fileDir.exists()) {
    qDebug() << __FUNCTION__ << "dir:" << fileDir
             << "not exists, try to cerate it";
    if (!fileDir.mkpath(".")) {
      qCritical() << __FUNCTION__ << "Can not create dir:" << fileDir;
      return false;
    }
  }

  // 目录创建成功
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    qCritical() << __FUNCTION__ << "Can not create file:" << path;
    return false;
  }

  file.close();
  qDebug() << __FUNCTION__ << "File create successfully!" << path;
  return true;
}

void calcStartEndPoint(const QRectF& rect,
                       qreal angle,
                       QPointF& start,
                       QPointF& end) {
  /*     o
   *    | \
   *    |  \ c
   *  a |   \
   *    |    \
   *    ------------
   *      b
   *
   * 假设 ∠bc 为α
   * 正弦:
   *   sinα = a / c
   * 余弦：
   *   cosα = b / c
   * 正切：
   *   tanα = a / b
   *
   * */

  // 去掉角度的周期和符号并限制在360度以内
  decltype(angle) angle1 = abs(angle);
  if (angle1 > 360) {
    int integerAngle = static_cast<int>(angle1);
    qreal floatRemain = angle1 - integerAngle;

    auto remain = integerAngle % 360;

    angle1 = remain + floatRemain;
  }

  auto halfW = rect.width() / 2.0;
  auto halfH = rect.height() / 2.0;
  auto center = rect.center();

  // 矩形对角线和经过圆矩形中心点水平线的夹角
  auto quarterAngle = qRadiansToDegrees(atan2(halfH, halfW));
  auto radian = qDegreesToRadians(angle1);

  qreal dxStart = 0, dyStart = 0, dxEnd = 0, dyEnd = 0;
  if (angle1 >= 0 && angle1 < quarterAngle) {
    /*
     *  ----------------
     * |       |      ^|
     * |       |     ^ |
     * |       |   ^   |
     * |-------o-------|
     * |    ^  |       |
     * |  ^    |       |
     * |^      |       |
     * -----------------
     * */
    auto dy = tan(radian) * halfW;

    dxStart = halfW;
    dyStart = -dy;
    dxEnd = -halfW;
    dyEnd = dy;
  } else if (angle1 >= quarterAngle && angle1 < 90) {
    /*
     * |------------^---
     * |       |   ^   |
     * |       |  ^    |
     * |       | ^     |
     * |-------o-------|
     * |      ^|       |
     * |     ^ |       |
     * -----^-----------
     * */
    auto dx = halfW / tan(radian);

    dxStart = dx;
    dyStart = -halfH;
    dxEnd = -dx;
    dyEnd = halfH;
  } else if (angle1 >= 90 && angle1 < 90 + quarterAngle) {
    /*
     * |--^--------------
     * |   ^   |       |
     * |    ^  |       |
     * |      ^|       |
     * |-------o-------|
     * |       | ^     |
     * |       |   ^   |
     * --------------^--
     * */
    auto dx = tan(qDegreesToRadians(angle1 - 90)) * halfH;

    dxStart = -dx;
    dyStart = -halfH;
    dxEnd = dx;
    dyEnd = halfH;
  } else if (angle1 >= 90 + quarterAngle && angle1 < 180) {
    /*
     * |-----------------
     * |       |       |
     * |^      |       |
     * |   ^   |       |
     * |-------o-------|
     * |       | ^     |
     * |       |    ^  |
     * |       |      ^|
     * ----------------
     * */
    auto dy = tan(qDegreesToRadians(180.0 - angle1)) * halfW;
    dxStart = -halfW;
    dyStart = -dy;
    dxEnd = halfW;
    dyEnd = dy;
  } else if (angle1 >= 180 && angle1 < 180 + quarterAngle) {
    /*
     *  ----------------
     * |       |      ^|
     * |       |     ^ |
     * |       |   ^   |
     * |-------o-------|
     * |    ^  |       |
     * |  ^    |       |
     * |^      |       |
     * -----------------
     * */
    auto dy = tan(radian) * halfW;

    dxStart = -halfW;
    dyStart = dy;
    dxEnd = halfW;
    dyEnd = -dy;
  } else if (angle1 >= 180 + quarterAngle && angle1 < 270) {
    /*
     * |------------^---
     * |       |   ^   |
     * |       |  ^    |
     * |       | ^     |
     * |-------o-------|
     * |      ^|       |
     * |     ^ |       |
     * -----^-----------
     * */
    auto dx = halfW / tan(radian);

    dxStart = -dx;
    dyStart = halfH;
    dxEnd = dx;
    dyEnd = -halfH;
  } else if (angle1 >= 270 && angle1 < 270 + quarterAngle) {
    /*
     * |--^--------------
     * |   ^   |       |
     * |    ^  |       |
     * |      ^|       |
     * |-------o-------|
     * |       | ^     |
     * |       |   ^   |
     * --------------^--
     * */
    auto dx = tan(qDegreesToRadians(angle1 - 270)) * halfH;

    dxStart = dx;
    dyStart = halfH;
    dxEnd = -dx;
    dyEnd = -halfH;
  } else if (angle1 >= 270 + quarterAngle && angle1 < 360) {
    /*
     * |-----------------
     * |       |       |
     * |^      |       |
     * |   ^   |       |
     * |-------o-------|
     * |       | ^     |
     * |       |    ^  |
     * |       |      ^|
     * ----------------
     * */
    auto dy = tan(qDegreesToRadians(360 - angle1)) * halfW;

    dxStart = halfW;
    dyStart = dy;
    dxEnd = -halfW;
    dyEnd = -dy;
  }

  start = center;
  end = center;
  if (angle >= 0) {
    start.rx() += dxStart;
    start.ry() += dyStart;

    end.rx() += dxEnd;
    end.ry() += dyEnd;
  } else {
    start.rx() += dxEnd;
    start.ry() += dyEnd;

    end.rx() += dxStart;
    end.ry() += dyStart;
  }
}

void paintWin10Progress(QPaintDevice* device, const QColor& color, qreal t_) {
  QPainter p(device);

  qreal ratio = .10;
  auto ww = device->width() * ratio;
  auto hh = device->height() * ratio;
  auto margin = std::min(ww, hh);

  QRect deviceRect(0, 0, device->width(), device->height());
  auto rect = deviceRect.adjusted(margin, margin, -margin, -margin);
  auto radius = std::min(rect.width(), rect.height());
  rect.setWidth(radius);
  rect.setHeight(radius);
  rect.moveCenter(deviceRect.center());

  QPainterPath path;
  path.moveTo(rect.center().x(), rect.top());
  path.arcTo(rect, 90, -360);

  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(QBrush(color), 10, Qt::SolidLine, Qt::RoundCap));

  auto p0 = path.pointAtPercent(t_);
  p.drawPoint(p0);

  auto delta = t_ - 0.05 * (1 - t_);
  if (delta > 0) {
    auto pos = path.pointAtPercent(delta);
    p.drawPoint(pos);
  }

  delta = t_ - 0.1 * (1 - t_);
  if (delta > 0) {
    auto pos = path.pointAtPercent(delta);
    p.drawPoint(pos);
  }

  delta = t_ - 0.15 * (1 - t_);
  if (delta > 0) {
    auto pos = path.pointAtPercent(delta);
    p.drawPoint(pos);
  }

  if (t_ > 0.98) {
    p.drawPoint(path.pointAtPercent(1));
  }

  p.restore();
}

void paintBeautifulProgress(QPaintDevice* device,
                            const double A,
                            const double k,
                            const QPointF& midPos,
                            const QColor& color1,
                            const QColor& color2,
                            const QColor& color3,
                            const QColor& color4) {
  static const auto PI = 3.14159265;
  static const auto HALFPI = PI / 2.0;
  static auto count = 40;
  static auto sizeBase = 0.1;
  static auto sizeDiv = 2.0;
  static auto tick = 0;

  QPainter painter(device);
  auto p = &painter;
  p->setPen(Qt::NoPen);
  p->setRenderHint(QPainter::Antialiasing);
  p->translate(midPos);

  auto angle = tick / 8.0;
  // y=Asin(ωx+φ)+k
  // A:振幅
  // (ωx+φ):相位，反映y处的状态
  // k:反映在坐标系上图像的上移或下移
  auto radius = A * std::sin(tick / 15.0) + k;
  auto size = 0.0;
  auto halfSize = 0.0;
  QRectF rect;

  for (int i = 0; i < count; i++) {
    angle += PI / 64.0;
    radius += i / 30.0;
    size = sizeBase + i / sizeDiv;
    halfSize = size / 2.0;

    rect.setWidth(size);
    rect.setHeight(size);
    rect.moveCenter(
        QPointF(std::cos(angle) * radius, std::sin(angle) * radius));
    p->setBrush(color1);  // QColor(0x269DD9)
    p->drawRoundedRect(rect, halfSize, halfSize);

    rect.setWidth(size);
    rect.setHeight(size);
    rect.moveCenter(
        QPointF(std::cos(angle) * -radius, std::sin(angle) * -radius));
    p->setBrush(color2);  // QColor(0xD9269D)
    p->drawRoundedRect(rect, halfSize, halfSize);

    rect.setWidth(size);
    rect.setHeight(size);
    rect.moveCenter(QPointF(std::cos(angle + HALFPI) * radius,
                            std::sin(angle + HALFPI) * radius));
    p->setBrush(color3);  // QColor(0xD9D926)
    p->drawRoundedRect(rect, halfSize, halfSize);

    rect.setWidth(size);
    rect.setHeight(size);
    rect.moveCenter(QPointF(std::cos(angle + HALFPI) * -radius,
                            std::sin(angle + HALFPI) * -radius));
    p->setBrush(color4);  // QColor(0xFFFFFF)
    p->drawRoundedRect(rect, halfSize, halfSize);
  }
  tick++;
}

QPoint posInParentCenter(QWidget* child, QWidget* parent) {
  if (!child || !parent) {
    return QPoint(0, 0);
  }

  return QPoint(parent->pos().x() + (parent->width() - child->width()) / 2,
                parent->pos().y() + (parent->height() - child->height()) / 2);
}

QByteArray getFileMD5Hash(const QString& filePath) {
  if (!QFile::exists(filePath)) {
    return "";
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return "";
  }

  QCryptographicHash obj(QCryptographicHash::Md5);
  obj.addData(file.readAll());
  file.close();

  return obj.result().toHex();
}

QScrollArea* findScrollArea(QWidget* who) {
  if (!who) {
    return nullptr;
  }

  if (auto parent = who->parentWidget()) {
    if (auto target = qobject_cast<QScrollArea*>(parent)) {
      return target;
    }

    return utility_qt::findScrollArea(parent);
  }

  return nullptr;
}

}  // namespace utility_qt
