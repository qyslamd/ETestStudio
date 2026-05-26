#pragma once

#include <QElapsedTimer>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

class PenguinOverlay : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal walkPos READ walkPos WRITE setWalkPos)
  Q_PROPERTY(qreal bodyBob READ bodyBob WRITE setBodyBob)
  Q_PROPERTY(qreal wingAngle READ wingAngle WRITE setWingAngle)
 public:
  explicit PenguinOverlay(bool fromLeft);
  ~PenguinOverlay() override = default;

  qreal walkPos() const { return walkPos_; }
  void setWalkPos(qreal v);
  qreal bodyBob() const { return bodyBob_; }
  void setBodyBob(qreal v);
  qreal wingAngle() const { return wingAngle_; }
  void setWingAngle(qreal v);

 signals:
  void done();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  void drawPenguin(QPainter& p, const QPointF& origin, qreal scale);
  void startWalk();
  void flee();
  void onWalkFinished();

  bool fromLeft_ = true;
  bool fleeing_ = false;

  qreal walkPos_ = 0;
  qreal bodyBob_ = 0;
  qreal wingAngle_ = 0;

  qreal floorY_ = 0;

  QPropertyAnimation* walkAnim_;
  QPropertyAnimation* bobAnim_;
  QPropertyAnimation* wingAnim_;
};
