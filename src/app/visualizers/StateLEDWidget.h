#ifndef ETEST_APP_VISUALIZERS_STATE_LED_WIDGET_H_
#define ETEST_APP_VISUALIZERS_STATE_LED_WIDGET_H_

#include <QDateTime>
#include <QLabel>
#include <QPair>

#include "SignalVisualizer.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// StateLEDWidget — 串口/开关量 LED 指示灯
// ══════════════════════════════════════════════════════════════════════════════
// 显示名称 + LED 圆点（绿/红）+ ON/OFF 文本。
// engValue >= 0.5 判定为 ON，检测上升沿统计脉冲计数。
// 所有样式通过 QSS #objectName 选择器控制（禁止 C++ setStyleSheet）。
// ══════════════════════════════════════════════════════════════════════════════
class StateLEDWidget : public SignalVisualizer {
  Q_OBJECT

 public:
  explicit StateLEDWidget(const QString& title, QWidget* parent = nullptr);

  void onSampleCaptured(const etest::engine::MonitorSample& sample) override;
  void clearData() override;
  QList<QPair<int, int>> displayedSignals() const override;

 private:
  void initUi();
  void updateLED(bool on);

  QString title_;
  QLabel* led_label_ = nullptr;       // LED 圆点（用 QLabel 背景圆角模拟）
  QLabel* state_label_ = nullptr;     // "ON" / "OFF"
  QLabel* pulse_label_ = nullptr;     // "脉冲: 123"
  QLabel* ts_label_ = nullptr;        // 最后变化时间

  int monitor_index_ = -1;
  int channel_index_ = -1;
  bool previous_on_ = false;
  int pulse_count_ = 0;
  QDateTime last_change_ts_;
};

}  // namespace etest::app

#endif  // ETEST_APP_VISUALIZERS_STATE_LED_WIDGET_H_
