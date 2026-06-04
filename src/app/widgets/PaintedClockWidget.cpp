#include "PaintedClockWidget.h"

#include <QDateTime>
#include <QPainter>
#include <QTimer>
#include <QtMath>

PaintedClockWidget::PaintedClockWidget(QWidget* parent) : QWidget(parent) {
  auto* timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, [this]() { update(); });
  timer->start(1000);
}

void PaintedClockWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  auto side = qMin(width(), height());
  p.setViewport((width() - side) / 2, (height() - side) / 2, side, side);
  p.setWindow(-100, -100, 200, 200);

  auto now = QDateTime::currentDateTime();
  int h = now.time().hour() % 12;
  int m = now.time().minute();
  int s = now.time().second();

  // clock face
  p.setPen(QPen(QColor(255, 255, 255, 200), 2));
  p.drawEllipse(-98, -98, 196, 196);

  // hour ticks
  for (int i = 0; i < 12; i++) {
    double a = i * 30 * M_PI / 180.0;
    int x1 = 80 * cos(a - M_PI / 2);
    int y1 = 80 * sin(a - M_PI / 2);
    int x2 = 90 * cos(a - M_PI / 2);
    int y2 = 90 * sin(a - M_PI / 2);
    p.drawLine(x1, y1, x2, y2);
  }

  // hour hand
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(255, 255, 255, 220));
  p.rotate(30.0 * h + 0.5 * m);
  p.drawRect(-3, -40, 6, 40);
  p.restore();

  // minute hand
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(255, 255, 255, 180));
  p.rotate(6.0 * m + 0.1 * s);
  p.drawRect(-2, -55, 4, 55);
  p.restore();

  // second hand
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(255, 107, 107));
  p.rotate(6.0 * s);
  p.drawRect(-1, -60, 2, 60);
  p.restore();

  // center dot
  p.setBrush(QColor(255, 107, 107));
  p.drawEllipse(-4, -4, 8, 8);

  // digital time at bottom
  p.setPen(QColor(255, 255, 255, 200));
  QFont f = p.font();
  f.setPixelSize(20);
  f.setBold(true);
  p.setFont(f);
  p.drawText(QRect(-80, 30, 160, 30), Qt::AlignCenter,
             QDateTime::currentDateTime().toString("HH:mm:ss"));
}


