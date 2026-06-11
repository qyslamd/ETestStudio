#include "PaintedClockWidget.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QMenu>
#include <QPainter>
#include <QTimer>

#include "minimal_clock_renderer.h"
#include "modern_clock_renderer.h"

PaintedClockWidget::PaintedClockWidget(QWidget* parent)
    : QWidget(parent), renderer_(new ModernClockRenderer) {
  clock_timer_ = new QTimer(this);
  connect(clock_timer_, &QTimer::timeout, this, [this]() { update(); });
  // 定时器默认不启动，等 showEvent 时再启动
}

void PaintedClockWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (!clock_timer_->isActive())
    clock_timer_->start(1000);
}

void PaintedClockWidget::hideEvent(QHideEvent* event) {
  QWidget::hideEvent(event);
  clock_timer_->stop();
}

PaintedClockWidget::~PaintedClockWidget() { delete renderer_; }

void PaintedClockWidget::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);
  auto* modernAction = menu.addAction(QStringLiteral("现代风格"));
  auto* minimalAction = menu.addAction(QStringLiteral("简约风格"));
  auto* result = menu.exec(event->globalPos());
  if (result == modernAction) {
    if (!dynamic_cast<ModernClockRenderer*>(renderer_))
      setRenderer(new ModernClockRenderer);
  } else if (result == minimalAction) {
    if (!dynamic_cast<MinimalClockRenderer*>(renderer_))
      setRenderer(new MinimalClockRenderer);
  }
  update();
}

void PaintedClockWidget::setRenderer(IClockRenderer* renderer) {
  if (renderer_) delete renderer_;
  renderer_ = renderer;
}

void PaintedClockWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  auto now = QDateTime::currentDateTime();
  renderer_->beginPaint(p, size());
  renderer_->drawBackground(p);
  renderer_->drawFace(p);
  renderer_->drawTicks(p);
  renderer_->drawNumbers(p);
  renderer_->drawHourHand(p, now.time().hour() % 12, now.time().minute());
  renderer_->drawMinuteHand(p, now.time().minute(), now.time().second());
  renderer_->drawSecondHand(p, now.time().second());
  renderer_->drawCenterDot(p);
  renderer_->drawDateInfo(p, now);
  renderer_->drawDigitalTime(p, now);
  renderer_->endPaint(p);
}
