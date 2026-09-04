#pragma once

#include <QFrame>

namespace etest::ui {

/// @brief 简单继承，用于样式表中通配选择器，方便设置圆角边框样式
class RoundQFrame : public QFrame {
  Q_OBJECT
 public:
  explicit RoundQFrame(QWidget* parent = nullptr);
  ~RoundQFrame() override;
};

}  // namespace etest::ui
