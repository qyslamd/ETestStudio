#include "modern_clock_renderer.h"

#include <QPainterPath>
#include <QtMath>

void ModernClockRenderer::beginPaint(QPainter& painter,
                                     const QSize& widgetSize) const {
  int side = qMin(widgetSize.width(), widgetSize.height());
  painter.setViewport((widgetSize.width() - side) / 2,
                      (widgetSize.height() - side) / 2, side, side);
  painter.setWindow(-200, -200, 400, 400);
}

void ModernClockRenderer::drawBackground(QPainter& p) const {
  // outer border ring
  p.setPen(QPen(QColor(0x33, 0x33, 0x33), 10));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(-195, -195, 390, 390);
}

void ModernClockRenderer::drawFace(QPainter& p) const {
  // face fill
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0xF6, 0xF6, 0xF6));
  p.drawEllipse(-190, -190, 380, 380);
}

void ModernClockRenderer::drawTicks(QPainter& p) const {
  p.setPen(QPen(QColor(0x55, 0x55, 0x55), 2));

  for (int i = 0; i < 60; i++) {
    p.save();
    p.rotate(i * 6.0);
    // tick starts near the edge and points inward
    if (i % 5 == 0) {
      // hour tick — longer, thicker
      p.setPen(QPen(QColor(0x33, 0x33, 0x33), 3));
      p.drawLine(175, 0, 188, 0);
    } else {
      // minute tick
      p.setPen(QPen(QColor(0x55, 0x55, 0x55), 2));
      p.drawLine(180, 0, 188, 0);
    }
    p.restore();
  }
}

void ModernClockRenderer::drawNumbers(QPainter& p) const {
  QFont f = p.font();
  f.setPixelSize(22);
  f.setFamily(QStringLiteral("fantasy"));
  p.setFont(f);

  static const int radii = 135;
  for (int i = 1; i <= 12; i++) {
    double angle = i * 30.0 * M_PI / 180.0;
    double x = radii * qSin(angle);
    double y = -radii * qCos(angle);

    QRectF r(x - 16, y - 16, 32, 32);
    if (i % 3 == 0) {
      QFont bold_f = f;
      bold_f.setPixelSize(28);
      p.setFont(bold_f);
      p.setPen(QColor(0x33, 0x33, 0x33));
    } else {
      p.setFont(f);
      p.setPen(QColor(0x55, 0x55, 0x55));
    }
    p.drawText(r, Qt::AlignCenter, QString::number(i));
  }
}

void ModernClockRenderer::drawHourHand(QPainter& p, int hour,
                                       int minute) const {
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0x33, 0x33, 0x33));
  p.rotate(30.0 * hour + 0.5 * minute);
  // draw from center downward (past center for counter-balance)
  QPainterPath path;
  path.moveTo(-6, 10);
  path.lineTo(-4, -75);
  path.lineTo(0, -85);
  path.lineTo(4, -75);
  path.lineTo(6, 10);
  path.closeSubpath();
  p.drawPath(path);
  p.restore();
}

void ModernClockRenderer::drawMinuteHand(QPainter& p, int minute,
                                         int second) const {
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0x33, 0x33, 0x33));
  p.rotate(6.0 * minute + 0.1 * second);
  QPainterPath path;
  path.moveTo(-4, 12);
  path.lineTo(-3, -110);
  path.lineTo(0, -120);
  path.lineTo(3, -110);
  path.lineTo(4, 12);
  path.closeSubpath();
  p.drawPath(path);
  p.restore();
}

void ModernClockRenderer::drawSecondHand(QPainter& p, int second) const {
  p.save();
  p.setPen(QPen(QColor(0xFF, 0x66, 0x00), 2));
  p.rotate(6.0 * second);
  p.drawLine(0, 20, 0, -155);
  p.restore();
}

void ModernClockRenderer::drawCenterDot(QPainter& p) const {
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0x33, 0x33, 0x33));
  p.drawEllipse(-7, -7, 14, 14);
}

void ModernClockRenderer::drawDateInfo(QPainter& p,
                                       const QDateTime& now) const {
  static const char* week_day[] = {"Sun.", "Mon.", "Tues.", "Wed.",
                                   "Thur.", "Fri.", "Sat."};
  QString dateStr =
      QStringLiteral("%1-%2-%3 %4")
          .arg(now.date().year())
          .arg(now.date().month(), 2, 10, QLatin1Char('0'))
          .arg(now.date().day(), 2, 10, QLatin1Char('0'))
          .arg(week_day[now.date().dayOfWeek() % 7]);

  QFont f = p.font();
  f.setPixelSize(14);
  f.setBold(true);
  p.setFont(f);
  p.setPen(QColor(0x55, 0x55, 0x55));
  p.drawText(QRect(-80, 25, 160, 20), Qt::AlignCenter, dateStr);
}

void ModernClockRenderer::drawDigitalTime(QPainter& p,
                                          const QDateTime& now) const {
  int h = now.time().hour();
  int m = now.time().minute();
  int s = now.time().second();

  QFont f = p.font();
  f.setPixelSize(16);
  f.setBold(true);
  p.setFont(f);
  p.setPen(Qt::white);

  auto drawDigit = [&](int val, int x) {
    QRect bg(x, 60, 22, 28);
    p.fillRect(bg, QColor(0x55, 0x55, 0x55));
    p.drawText(bg, Qt::AlignCenter,
               QStringLiteral("%1").arg(val, 2, 10, QLatin1Char('0')));
  };

  int totalW = 22 * 3 + 4;
  int startX = -totalW / 2;
  drawDigit(h, startX);
  drawDigit(m, startX + 26);
  drawDigit(s, startX + 52);
}
