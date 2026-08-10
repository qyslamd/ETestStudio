#pragma once

#include <QDialog>
#include <functional>

class QGraphicsDropShadowEffect;

namespace etest::app {

// 无边框遮罩卡片覆盖层：覆盖父窗口 + 灰色遮罩 + 居中卡片（阴影/圆角），
// 跨平台统一外观（Linux 下不依赖原生标题栏/边框）。显示/关闭即时，无飞入动画。
class OverlayDialog : public QDialog {
  Q_OBJECT

 public:
  explicit OverlayDialog(QWidget* parent = nullptr);
  ~OverlayDialog() override;

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
};

}  // namespace etest::app
