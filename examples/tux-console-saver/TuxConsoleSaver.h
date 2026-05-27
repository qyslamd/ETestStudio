#pragma once

#include "TuxSaverWidget.h"

/// MobaXterm Tux Console Saver 风格 demo
/// 终端背景 + 一只随机动画的 Tux 企鹅
class TuxConsoleSaver : public TuxSaverWidget {
  Q_OBJECT
 public:
  explicit TuxConsoleSaver(QWidget* parent = nullptr);

 protected:
  void drawBackground(QPainter& p) const override;

 private:
  void drawConsoleBackground(QPainter& p) const;
  void drawFloor(QPainter& p) const;
};
