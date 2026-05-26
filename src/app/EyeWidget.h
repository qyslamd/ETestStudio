#ifndef ETEST_APP_EYEWIDGET_H_
#define ETEST_APP_EYEWIDGET_H_

#include <QElapsedTimer>
#include <QPoint>
#include <QWidget>

class QTimer;

class EyeWidget : public QWidget {
  Q_OBJECT

 public:
  explicit EyeWidget(QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  void tick();
  void scheduleNextBlink();

  // smooth tracking
  QPointF target_pos_;
  QPointF smooth_pos_;
  QTimer* anim_timer_ = nullptr;

  // blink
  double blink_phase_ = 0;
  int blink_remaining_ = 0;
  bool blink_closing_ = true;
  QTimer* blink_scheduler_ = nullptr;

  // idle/drowsy
  QElapsedTimer last_move_elapsed_;
  double drowsy_level_ = 0;

  // click effect
  double cross_eye_phase_ = 0;

  // derived
  double mouse_nearness_ = 0;
};

#endif  // ETEST_APP_EYEWIDGET_H_
