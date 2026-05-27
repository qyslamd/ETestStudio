#pragma once

#include <QColor>
#include <QTimer>
#include <QWidget>

class PenguinWidget : public QWidget {
  Q_OBJECT
 public:
  explicit PenguinWidget(QWidget* parent = nullptr);
  ~PenguinWidget() override;

  /// 在父控件内随机位置出现
  void appear();
  /// 强制消失（逃跑动画后 deleteLater）
  void dismiss();

  /// 当前状态中文名（用于 UI 状态栏）
  QString stateName() const;

 signals:
  void dismissed();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void enterEvent(QEvent* event) override;

 private:
  void tick();
  void pickNewTarget();
  void startFlee();
  void scheduleNextBlink();
  void drawPenguin(QPainter& p, const QPointF& center, qreal dir);

  // animation state
  enum State { HIDDEN, IDLE, WALKING, FLEEING };
  State state_ = HIDDEN;

  // position (center of widget, in parent coordinates)
  QPointF pos_;
  QPointF target_;
  double walk_phase_ = 0;
  qreal walk_speed_ = 1.0;  // multiplier, 1.0 normal, 4.0 fleeing

  // timers
  QTimer* anim_timer_ = nullptr;   // 50ms animation tick
  QTimer* pause_timer_ = nullptr;  // pause between walks
  QTimer* blink_timer_ = nullptr;  // random blink

  // blink animation
  double blink_value_ = 0;  // 0=open, 1=closed
  bool blink_closing_ = true;

  // parent stage dimensions (cached for bounds checking)
  QSizeF stage_size_;
};
