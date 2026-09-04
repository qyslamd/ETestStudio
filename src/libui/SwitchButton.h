#pragma once

#include <QAbstractButton>
#include <QBrush>

class QVariantAnimation;

namespace etest::ui {

// Fluent 风格纯滑块开关：胶囊底 + 白色圆形滑块（无文字、无凹槽）。
// 配色通过 setOnBackground/setOffBackground 注入，配合 ThemeManager 语义色使用。
class SwitchButton : public QAbstractButton {
  Q_OBJECT
 public:
  explicit SwitchButton(QWidget* parent = nullptr);

  void setOnBackground(const QBrush& brush);
  void setOffBackground(const QBrush& brush);

 protected:
  void paintEvent(QPaintEvent* event) override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 private:
  void fillRoundRect(QPainter* p, const QRectF& rect, const QBrush& brush);
  void startKnobAnimation(bool checked);

  QBrush background_on_ = QColor(0x10, 0x7C, 0x10);
  QBrush background_off_ = QColor(0xD0, 0xD0, 0xDD);
  QVariantAnimation* knob_animation_ = nullptr;
  qreal knob_position_ = 0.0;  // 0 = off，1 = on，由动画驱动
};

}  // namespace etest::ui
