#ifndef ETEST_APP_MINIMAL_CLOCK_RENDERER_H_
#define ETEST_APP_MINIMAL_CLOCK_RENDERER_H_

#include "clock_renderer.h"

class MinimalClockRenderer : public IClockRenderer {
 public:
  void beginPaint(QPainter& painter, const QSize& widgetSize) const override;
  void drawBackground(QPainter&) const override;
  void drawFace(QPainter&) const override;
  void drawTicks(QPainter&) const override;
  void drawNumbers(QPainter&) const override;
  void drawHourHand(QPainter&, int hour, int minute) const override;
  void drawMinuteHand(QPainter&, int minute, int second) const override;
  void drawSecondHand(QPainter&, int second) const override;
  void drawCenterDot(QPainter&) const override;
  void drawDateInfo(QPainter&, const QDateTime&) const override;
  void drawDigitalTime(QPainter&, const QDateTime&) const override;
};

#endif  // ETEST_APP_MINIMAL_CLOCK_RENDERER_H_
