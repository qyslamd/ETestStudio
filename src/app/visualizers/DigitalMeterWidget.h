#ifndef ETEST_APP_VISUALIZERS_DIGITAL_METER_WIDGET_H_
#define ETEST_APP_VISUALIZERS_DIGITAL_METER_WIDGET_H_

#include <QLabel>
#include <QPair>

#include "SignalVisualizer.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// DigitalMeterWidget — 数值型信号大数字表
// ══════════════════════════════════════════════════════════════════════════════
// 显示名称 + 大字体工程值 + 趋势箭头 + 原始值 + 最小/最大值。
// 适合 CAN/A429 数值信号展示。
// 所有样式通过 QSS #objectName 控制。
// ══════════════════════════════════════════════════════════════════════════════
class DigitalMeterWidget : public SignalVisualizer {
  Q_OBJECT

 public:
  explicit DigitalMeterWidget(const QString& title, QWidget* parent = nullptr);

  void onSampleCaptured(const etest::engine::MonitorSample& sample) override;
  void clearData() override;
  QList<int> displayedSignals() const override;

 private:
  void initUi();
  void updateDisplay();

  QString title_;
  QLabel* title_label_ = nullptr;
  QLabel* value_label_ = nullptr;     // 大字体工程值
  QLabel* trend_label_ = nullptr;     // 趋势箭头 ↑ ↓ →
  QLabel* range_label_ = nullptr;     // "min: 0.00  max: 100.00"
  QLabel* raw_label_ = nullptr;       // 原始值
  QLabel* ts_label_ = nullptr;        // 时间戳

  int monitor_index_ = -1;
  double current_value_ = 0.0;
  double min_value_ = 0.0;
  double max_value_ = 0.0;
  double previous_value_ = 0.0;
  bool has_data_ = false;

  static constexpr double kEpsilon = 1e-9;
};

}  // namespace etest::app

#endif  // ETEST_APP_VISUALIZERS_DIGITAL_METER_WIDGET_H_
