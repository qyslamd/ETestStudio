#ifndef ETEST_APP_VISUALIZERS_VALUE_LABEL_WIDGET_H_
#define ETEST_APP_VISUALIZERS_VALUE_LABEL_WIDGET_H_

#include <QLabel>
#include <QPair>

#include "SignalVisualizer.h"
#include "engine/MonitorManager.h"  // MonitorSample

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// ValueLabelWidget — 通用 fallback 可视化组件
// ══════════════════════════════════════════════════════════════════════════════
// 以标签形式展示工程值和原始值。适合 CAN/A429/未识别信号类型。
// 作为所有已识别信号类型的兜底方案。
// ══════════════════════════════════════════════════════════════════════════════
class ValueLabelWidget : public SignalVisualizer {
  Q_OBJECT

 public:
  explicit ValueLabelWidget(const QString& title, QWidget* parent = nullptr);

  void onSampleCaptured(const etest::engine::MonitorSample& sample) override;
  void clearData() override;
  QList<int> displayedSignals() const override;

 private:
  void initUi();

  QString title_;
  QLabel* value_label_ = nullptr;    // 工程值大字体
  QLabel* raw_label_ = nullptr;      // 原始值
  QLabel* ts_label_ = nullptr;       // 最后采样时间
  int monitor_index_ = -1;
};

}  // namespace etest::app

#endif  // ETEST_APP_VISUALIZERS_VALUE_LABEL_WIDGET_H_
