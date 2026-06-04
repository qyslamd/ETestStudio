#ifndef ETEST_APP_GRID_GRADIENT_PAINTER_H_
#define ETEST_APP_GRID_GRADIENT_PAINTER_H_

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QLinearGradient>
#include <QString>
#include <QVector>
#include <QRegularExpression>

namespace etest::app::grid {

struct GradientColorStop {
  qreal position;
  QColor color;
};

class GradientPainter : public QObject {
  Q_OBJECT

 public:
  explicit GradientPainter(QObject* parent = nullptr);

  static bool ParseCssGradient(const QString& css_code,
                               qreal& angle,
                               QVector<GradientColorStop>& color_stops);
  static void CalcStartEndPoint(const QRectF& rect,
                                qreal angle,
                                QPointF& start,
                                QPointF& end);
  static QLinearGradient CreateLinearGradient(const QRectF& rect,
                                             const QString& css_code);
  static QLinearGradient CreateLinearGradient(
      const QRectF& rect,
      const QVector<GradientColorStop>& color_stops,
      qreal angle);
  static QString CssToQss(const QString& css_code, const QRectF& rect);
  static QString GeneratePainterCode(const QString& css_code, const QRectF& rect);
  static QString GenerateCalcStartEndPointCode(qreal angle);

 private:
  static qreal ParseAngle(const QString& css_code);
  static QVector<GradientColorStop> ParseColorStops(const QString& css_code);
};

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_GRADIENT_PAINTER_H_
