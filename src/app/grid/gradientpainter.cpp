#include "gradientpainter.h"
#include <QtMath>
#include <QColor>
#include <QRegularExpression>

namespace etest::app::grid {

qreal GradientPainter::ParseAngle(const QString& css_code) {
  QRegularExpression re(R"((\d+(?:\.\d+)?)\s*deg)");
  QRegularExpressionMatch match = re.match(css_code);
  if (match.hasMatch()) {
    return match.capturedRef(1).toDouble();
  }

  QRegularExpression to_top_re(R"(to\s+top\b)",
                               QRegularExpression::CaseInsensitiveOption);
  if (css_code.contains(to_top_re)) {
    return 0;
  }

  QRegularExpression to_bottom_re(R"(to\s+bottom\b)",
                                 QRegularExpression::CaseInsensitiveOption);
  if (css_code.contains(to_bottom_re)) {
    return 180;
  }

  QRegularExpression to_left_re(R"(to\s+left\b)",
                               QRegularExpression::CaseInsensitiveOption);
  if (css_code.contains(to_left_re)) {
    return 270;
  }

  QRegularExpression to_right_re(R"(to\s+right\b)",
                                QRegularExpression::CaseInsensitiveOption);
  if (css_code.contains(to_right_re)) {
    return 90;
  }

  return 180;
}

QVector<GradientColorStop> GradientPainter::ParseColorStops(
    const QString& css_code) {
  QVector<GradientColorStop> stops;

  QRegularExpression color_re(
      R"((#[0-9a-fA-F]{3,8}|rgba?\s*\([^)]+\))\s*([\d.]+%)?)");
  QRegularExpressionMatchIterator it = color_re.globalMatch(css_code);

  int index = 0;
  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    GradientColorStop stop;
    stop.color = QColor(match.captured(1));

    QString pos_str = match.captured(2).trimmed();
    if (!pos_str.isEmpty()) {
      stop.position = pos_str.chopped(1).toDouble() / 100.0;
    } else {
      if (stops.isEmpty()) {
        stop.position = 0;
      } else if (!it.hasNext() && index >= 2) {
        stop.position = 1;
      } else {
        stop.position = -1;
      }
    }
    stops.append(stop);
    index++;
  }

  int valid_count = 0;
  for (auto& stop : stops) {
    if (stop.position >= 0) {
      valid_count++;
    }
  }

  if (valid_count < 2 && stops.size() >= 2) {
    if (stops.first().position < 0) {
      stops.first().position = 0;
    }
    if (stops.last().position < 0) {
      stops.last().position = 1;
    }
  }

  return stops;
}

bool GradientPainter::ParseCssGradient(const QString& css_code,
                                     qreal& angle,
                                     QVector<GradientColorStop>& color_stops) {
  angle = ParseAngle(css_code);
  color_stops = ParseColorStops(css_code);
  return !color_stops.isEmpty();
}

void GradientPainter::CalcStartEndPoint(const QRectF& rect,
                                        qreal angle,
                                        QPointF& start,
                                        QPointF& end) {
  qreal angle1 = qAbs(angle);
  if (angle1 > 360) {
    int integer_angle = static_cast<int>(angle1);
    qreal float_remain = angle1 - integer_angle;
    auto remain = integer_angle % 360;
    angle1 = remain + float_remain;
  }

  auto half_w = rect.width() / 2.0;
  auto half_h = rect.height() / 2.0;
  auto center = rect.center();

  auto quarter_angle = qRadiansToDegrees(qAtan2(half_h, half_w));
  auto radian = qDegreesToRadians(angle1);

  qreal dx_start = 0, dy_start = 0, dx_end = 0, dy_end = 0;

  if (angle1 >= 0 && angle1 < quarter_angle) {
    auto dy = qTan(radian) * half_w;
    dx_start = half_w;
    dy_start = -dy;
    dx_end = -half_w;
    dy_end = dy;
  } else if (angle1 >= quarter_angle && angle1 < 90) {
    auto dx = half_w / qTan(radian);
    dx_start = dx;
    dy_start = -half_h;
    dx_end = -dx;
    dy_end = half_h;
  } else if (angle1 >= 90 && angle1 < 90 + quarter_angle) {
    auto dx = qTan(qDegreesToRadians(angle1 - 90)) * half_h;
    dx_start = -dx;
    dy_start = -half_h;
    dx_end = dx;
    dy_end = half_h;
  } else if (angle1 >= 90 + quarter_angle && angle1 < 180) {
    auto dy = qTan(qDegreesToRadians(180.0 - angle1)) * half_w;
    dx_start = -half_w;
    dy_start = -dy;
    dx_end = half_w;
    dy_end = dy;
  } else if (angle1 >= 180 && angle1 < 180 + quarter_angle) {
    auto dy = qTan(radian) * half_w;
    dx_start = -half_w;
    dy_start = dy;
    dx_end = half_w;
    dy_end = -dy;
  } else if (angle1 >= 180 + quarter_angle && angle1 < 270) {
    auto dx = half_w / qTan(radian);
    dx_start = -dx;
    dy_start = half_h;
    dx_end = dx;
    dy_end = -half_h;
  } else if (angle1 >= 270 && angle1 < 270 + quarter_angle) {
    auto dx = qTan(qDegreesToRadians(angle1 - 270)) * half_h;
    dx_start = dx;
    dy_start = half_h;
    dx_end = -dx;
    dy_end = -half_h;
  } else if (angle1 >= 270 + quarter_angle && angle1 < 360) {
    auto dy = qTan(qDegreesToRadians(360 - angle1)) * half_w;
    dx_start = half_w;
    dy_start = dy;
    dx_end = -half_w;
    dy_end = -dy;
  }

  start = center;
  end = center;
  if (angle >= 0) {
    start.rx() += dx_start;
    start.ry() += dy_start;
    end.rx() += dx_end;
    end.ry() += dy_end;
  } else {
    start.rx() += dx_end;
    start.ry() += dy_end;
    end.rx() += dx_start;
    end.ry() += dy_start;
  }
}

QLinearGradient GradientPainter::CreateLinearGradient(
    const QRectF& rect, const QString& css_code) {
  qreal angle;
  QVector<GradientColorStop> color_stops;

  if (!ParseCssGradient(css_code, angle, color_stops)) {
    return QLinearGradient();
  }

  QPointF start, end;
  CalcStartEndPoint(rect, angle, start, end);

  QLinearGradient gradient(start, end);
  gradient.setCoordinateMode(QGradient::ObjectBoundingMode);

  for (const auto& stop : color_stops) {
    if (stop.position >= 0 && stop.position <= 1) {
      gradient.setColorAt(stop.position, stop.color);
    }
  }

  return gradient;
}

QLinearGradient GradientPainter::CreateLinearGradient(
    const QRectF& rect,
    const QVector<GradientColorStop>& color_stops,
    qreal angle) {
  QPointF start, end;
  CalcStartEndPoint(rect, angle, start, end);

  QLinearGradient gradient(start, end);
  gradient.setCoordinateMode(QGradient::ObjectBoundingMode);

  for (const auto& stop : color_stops) {
    if (stop.position >= 0 && stop.position <= 1) {
      gradient.setColorAt(stop.position, stop.color);
    }
  }

  return gradient;
}

QString GradientPainter::CssToQss(const QString& css_code,
                                  const QRectF& rect) {
  qreal angle;
  QVector<GradientColorStop> color_stops;

  if (!ParseCssGradient(css_code, angle, color_stops)) {
    return QString();
  }

  QPointF start, end;
  CalcStartEndPoint(rect, angle, start, end);

  qreal x1 = start.x() / rect.width();
  qreal y1 = start.y() / rect.height();
  qreal x2 = end.x() / rect.width();
  qreal y2 = end.y() / rect.height();

  QString qss = QString("background: qlineargradient(x1:%1, y1:%2, x2:%3, y2:%4")
                    .arg(x1, 0, 'f', 2)
                    .arg(y1, 0, 'f', 2)
                    .arg(x2, 0, 'f', 2)
                    .arg(y2, 0, 'f', 2);

  for (const auto& stop : color_stops) {
    if (stop.position >= 0 && stop.position <= 1) {
      qss += QString(", stop:%1 rgba(%2,%3,%4, %5)")
                 .arg(stop.position, 0, 'f', 2)
                 .arg(stop.color.red())
                 .arg(stop.color.green())
                 .arg(stop.color.blue())
                 .arg(stop.color.alphaF(), 0, 'f', 2);
    }
  }

  qss += ");";
  return qss;
}

QString GradientPainter::GeneratePainterCode(const QString& css_code,
                                         const QRectF& rect) {
  qreal angle;
  QVector<GradientColorStop> color_stops;

  if (!ParseCssGradient(css_code, angle, color_stops)) {
    return QString();
  }

  QString code;
  code += "QLinearGradient gradient;\n";
  code += "QPointF start, end;\n";
  code += "CalcStartEndPoint(rect, " + QString::number(angle, 'f', 2) + ", start, end);\n";
  code += "gradient.setStart(start);\n";
  code += "gradient.setFinalStop(end);\n";

  for (const auto& stop : color_stops) {
    if (stop.position >= 0 && stop.position <= 1) {
      code += "gradient.setColorAt(" +
              QString::number(stop.position, 'f', 2) + ", QColor(\"" +
              stop.color.name(QColor::HexRgb) + "\"));\n";
    }
  }

  code += "\n";
  code += "QBrush brush(gradient);\n";
  code += "QPen pen(brush);\n";

  return code;
}

QString GradientPainter::GenerateCalcStartEndPointCode(qreal angle) {
  QString code;
  code += "void CalcStartEndPoint(const QRectF& rect, qreal angle, QPointF& start, QPointF& end) {\n";
  code += "    qreal angle1 = qAbs(angle);\n";
  code += "    if (angle1 > 360) {\n";
  code += "        int integerAngle = static_cast<int>(angle1);\n";
  code += "        qreal floatRemain = angle1 - integerAngle;\n";
  code += "        auto remain = integerAngle % 360;\n";
  code += "        angle1 = remain + floatRemain;\n";
  code += "    }\n";
  code += "\n";
  code += "    auto halfW = rect.width() / 2.0;\n";
  code += "    auto halfH = rect.height() / 2.0;\n";
  code += "    auto center = rect.center();\n";
  code += "\n";
  code += "    auto quarterAngle = qRadiansToDegrees(qAtan2(halfH, halfW));\n";
  code += "    auto radian = qDegreesToRadians(angle1);\n";
  code += "\n";
  code += "    qreal dxStart = 0, dyStart = 0, dxEnd = 0, dyEnd = 0;\n";
  code += "\n";
  code += "    if (angle1 >= 0 && angle1 < quarterAngle) {\n";
  code += "        auto dy = qTan(radian) * halfW;\n";
  code += "        dxStart = halfW; dyStart = -dy;\n";
  code += "        dxEnd = -halfW; dyEnd = dy;\n";
  code += "    } else if (angle1 >= quarterAngle && angle1 < 90) {\n";
  code += "        auto dx = halfW / qTan(radian);\n";
  code += "        dxStart = dx; dyStart = -halfH;\n";
  code += "        dxEnd = -dx; dyEnd = halfH;\n";
  code += "    } else if (angle1 >= 90 && angle1 < 90 + quarterAngle) {\n";
  code += "        auto dx = qTan(qDegreesToRadians(angle1 - 90)) * halfH;\n";
  code += "        dxStart = -dx; dyStart = -halfH;\n";
  code += "        dxEnd = dx; dyEnd = halfH;\n";
  code += "    } else if (angle1 >= 90 + quarterAngle && angle1 < 180) {\n";
  code += "        auto dy = qTan(qDegreesToRadians(180.0 - angle1)) * halfW;\n";
  code += "        dxStart = -halfW; dyStart = -dy;\n";
  code += "        dxEnd = halfW; dyEnd = dy;\n";
  code += "    } else if (angle1 >= 180 && angle1 < 180 + quarterAngle) {\n";
  code += "        auto dy = qTan(radian) * halfW;\n";
  code += "        dxStart = -halfW; dyStart = dy;\n";
  code += "        dxEnd = halfW; dyEnd = -dy;\n";
  code += "    } else if (angle1 >= 180 + quarterAngle && angle1 < 270) {\n";
  code += "        auto dx = halfW / qTan(radian);\n";
  code += "        dxStart = -dx; dyStart = halfH;\n";
  code += "        dxEnd = dx; dyEnd = -halfH;\n";
  code += "    } else if (angle1 >= 270 && angle1 < 270 + quarterAngle) {\n";
  code += "        auto dx = qTan(qDegreesToRadians(angle1 - 270)) * halfH;\n";
  code += "        dxStart = dx; dyStart = halfH;\n";
  code += "        dxEnd = -dx; dyEnd = -halfH;\n";
  code += "    } else if (angle1 >= 270 + quarterAngle && angle1 < 360) {\n";
  code += "        auto dy = qTan(qDegreesToRadians(360 - angle1)) * halfW;\n";
  code += "        dxStart = halfW; dyStart = dy;\n";
  code += "        dxEnd = -halfW; dyEnd = -dy;\n";
  code += "    }\n";
  code += "\n";
  code += "    start = center;\n";
  code += "    end = center;\n";
  code += "    if (angle >= 0) {\n";
  code += "        start.rx() += dxStart;\n";
  code += "        start.ry() += dyStart;\n";
  code += "        end.rx() += dxEnd;\n";
  code += "        end.ry() += dyEnd;\n";
  code += "    } else {\n";
  code += "        start.rx() += dxEnd;\n";
  code += "        start.ry() += dyEnd;\n";
  code += "        end.rx() += dxStart;\n";
  code += "        end.ry() += dyStart;\n";
  code += "    }\n";
  code += "}\n";

  return code;
}

}  // namespace etest::app::grid
