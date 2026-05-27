#ifndef ETEST_APP_EYEWIDGET_H_
#define ETEST_APP_EYEWIDGET_H_

#include <QColor>
#include <QElapsedTimer>
#include <QPoint>
#include <QWidget>

class QTimer;

class EyeWidget : public QWidget {
  Q_OBJECT

 public:
  explicit EyeWidget(QWidget* parent = nullptr);

  // 颜色设置接口，用于适配主题
  void setBgColor(const QColor& c);
  void setScleraColor(const QColor& c);
  void setOutlineColor(const QColor& c);
  void setPupilColor(const QColor& c);
  void setEyebrowColor(const QColor& c);

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

  // customizable colors (dark theme defaults)
  QColor bg_color_{0x1e, 0x1e, 0x2e};
  QColor sclera_color_{0xff, 0xff, 0xff};
  QColor outline_color_{0xcc, 0xcc, 0xcc};
  QColor pupil_color_{0x2c, 0x2c, 0x2c};
  QColor eyebrow_color_{0x88, 0x88, 0x99};
};

#endif  // ETEST_APP_EYEWIDGET_H_
