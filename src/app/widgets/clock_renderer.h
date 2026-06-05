#ifndef ETEST_APP_CLOCK_RENDERER_H_
#define ETEST_APP_CLOCK_RENDERER_H_

#include <QDateTime>
#include <QPainter>
#include <QSize>

class IClockRenderer {
 public:
  virtual ~IClockRenderer() = default;

  virtual void beginPaint(QPainter& painter, const QSize& widgetSize) const = 0;
  virtual void endPaint(QPainter& painter) const {}

  virtual void drawBackground(QPainter&) const = 0;
  virtual void drawFace(QPainter&) const = 0;
  virtual void drawTicks(QPainter&) const = 0;
  virtual void drawNumbers(QPainter&) const = 0;
  virtual void drawHourHand(QPainter&, int hour, int minute) const = 0;
  virtual void drawMinuteHand(QPainter&, int minute, int second) const = 0;
  virtual void drawSecondHand(QPainter&, int second) const = 0;
  virtual void drawCenterDot(QPainter&) const = 0;
  virtual void drawDateInfo(QPainter&, const QDateTime&) const = 0;
  virtual void drawDigitalTime(QPainter&, const QDateTime&) const = 0;
};

#endif  // ETEST_APP_CLOCK_RENDERER_H_
