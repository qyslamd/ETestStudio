#pragma once

#include <QToolButton>

namespace etest::app {

class HintPopup;

/// QAB 消息提示按钮。有未读消息时图标显示小红点，点击弹出 HintPopup。
class HintButton : public QToolButton {
  Q_OBJECT
 public:
  explicit HintButton(QWidget* parent = nullptr);

 private:
  HintPopup* popup_ = nullptr;

  void reloadIcon();
};

}  // namespace etest::app
