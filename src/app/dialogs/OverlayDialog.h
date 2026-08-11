#pragma once

#include <QColor>
#include <QDialog>
#include <functional>

class QGraphicsDropShadowEffect;

namespace etest::app {

// 无边框遮罩卡片覆盖层：覆盖父窗口 + 遮罩 + 居中卡片（阴影/圆角），
// 跨平台统一外观（Linux 下不依赖原生标题栏/边框）。显示/关闭即时，无飞入动画。
// 遮罩色默认灰（205,205,205,170），派生类可用 setMaskColor 覆盖（如引导用黑色）。
class OverlayDialog : public QDialog {
  Q_OBJECT

 public:
  explicit OverlayDialog(QWidget* parent = nullptr);
  ~OverlayDialog() override;

  void setMaskColor(const QColor& color);
  QColor maskColor() const;

 signals:
  void hideAnimationFinished();

 protected:
  void setWidget(QWidget* widget);
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void keyPressEvent(QKeyEvent* e) override;

 protected slots:
  void actHideAnimation();
  void actHideAnimation(std::function<void()> func);

 protected:
  QWidget* widget_ = nullptr;
  int round_radius_ = 8;
  QGraphicsDropShadowEffect* shadowEffect_ = nullptr;
  QColor mask_color_ = QColor(205, 205, 205, 170);
};

}  // namespace etest::app
