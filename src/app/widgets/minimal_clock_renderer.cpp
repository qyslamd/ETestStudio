#include "minimal_clock_renderer.h"

#include <QtMath>

#include "ThemeManager.h"

using etest::core_ui::ThemeManager;

// 面板半径，所有字号表达为它的百分比
static constexpr int kFaceRadius = 98;

void MinimalClockRenderer::beginPaint(QPainter& painter,
                                      const QSize& widgetSize) const {
  int side = qMin(widgetSize.width(), widgetSize.height());
  painter.setViewport((widgetSize.width() - side) / 2,
                      (widgetSize.height() - side) / 2, side, side);
  painter.setWindow(-100, -100, 200, 200);
}

void MinimalClockRenderer::drawBackground(QPainter& p) const {
  p.setPen(QPen(ThemeManager::instance().clockHandColor(), 2));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(-98, -98, 196, 196);
}

void MinimalClockRenderer::drawFace(QPainter&) const {}

void MinimalClockRenderer::drawTicks(QPainter& p) const {
  p.setPen(QPen(ThemeManager::instance().clockHandColor(), 2));
  for (int i = 0; i < 12; i++) {
    double a = i * 30.0 * M_PI / 180.0;
    int x1 = static_cast<int>(80 * cos(a - M_PI / 2));
    int y1 = static_cast<int>(80 * sin(a - M_PI / 2));
    int x2 = static_cast<int>(90 * cos(a - M_PI / 2));
    int y2 = static_cast<int>(90 * sin(a - M_PI / 2));
    p.drawLine(x1, y1, x2, y2);
  }
}

void MinimalClockRenderer::drawNumbers(QPainter&) const {}

void MinimalClockRenderer::drawHourHand(QPainter& p, int hour,
                                        int minute) const {
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(ThemeManager::instance().clockHandColor());
  p.rotate(30.0 * hour + 0.5 * minute);
  p.drawRect(-3, -40, 6, 40);
  p.restore();
}

void MinimalClockRenderer::drawMinuteHand(QPainter& p, int minute,
                                          int second) const {
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(ThemeManager::instance().clockHandColor());
  p.rotate(6.0 * minute + 0.1 * second);
  p.drawRect(-2, -55, 4, 55);
  p.restore();
}

void MinimalClockRenderer::drawSecondHand(QPainter& p, int second) const {
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(ThemeManager::instance().clockAccentColor());
  p.rotate(6.0 * second);
  p.drawRect(-1, -60, 2, 60);
  p.restore();
}

void MinimalClockRenderer::drawCenterDot(QPainter& p) const {
  p.setBrush(ThemeManager::instance().clockAccentColor());
  p.drawEllipse(-4, -4, 8, 8);
}

void MinimalClockRenderer::drawDateInfo(QPainter&, const QDateTime&) const {}

void MinimalClockRenderer::drawDigitalTime(QPainter& p,
                                           const QDateTime& now) const {
  p.setPen(ThemeManager::instance().clockHandColor());
  QFont f = p.font();
  f.setPixelSize(static_cast<int>(kFaceRadius * 0.20));
  f.setBold(true);
  p.setFont(f);
  p.drawText(QRect(-80, 30, 160, 30), Qt::AlignCenter,
             now.toString("HH:mm:ss"));
}
