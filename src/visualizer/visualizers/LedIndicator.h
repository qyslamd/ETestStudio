#pragma once

#include <QColor>
#include <QDateTime>
#include <QMap>
#include <QWidget>

#include "visualizers/SignalVisualizer.h"

class QLabel;

namespace etest::visualizer {

// LED 圆灯可视化组件（合并自 StateLEDWidget）：
// 按状态值映射语义色（默认 0灰/1绿/2红，可按字段语义覆盖），展示帧内
// 1bit 状态字段（开/关、正常/故障、告警）。两级标题（监听器名 + 连接
// 描述），带脉冲计数（上升沿）与最后变化时间戳，纯展示无交互。
class LedIndicator : public SignalVisualizer {
  Q_OBJECT

 public:
  explicit LedIndicator(QWidget* parent = nullptr);

  void setState(int state);
  int state() const { return state_; }

  // 状态值 → 颜色映射（默认 0灰/1绿/2红，故障类字段可覆盖为 0绿/1红）
  void setColorForState(int state, const QColor& color);
  void setDefaultColors();

  void setFieldName(const QString& name);
  QString fieldName() const { return field_name_; }
  void setStateText(const QString& text);
  QString stateText() const { return state_text_; }

  void setLedSize(int size);
  int ledSize() const { return led_size_; }

  // ── SignalVisualizer ──
  void onSampleCaptured(const etest::engine::MonitorSample& sample) override;
  void clearData() override;
  QList<QString> displayedSignals() const override;
  void setTitle(const QString& title) override;
  void setSubtitle(const QString& subtitle) override;

 private:
  void initUi();
  void refreshStateVisual();
  QColor stateColor() const;

  int state_ = 0;
  QMap<int, QColor> color_map_;
  QString field_name_;
  QString state_text_;
  QString connection_id_;
  int led_size_ = 32;

  // 状态统计（吸收 StateLEDWidget）：脉冲上升沿 + 最后变化时间
  bool previous_on_ = false;
  int pulse_count_ = 0;
  QDateTime last_change_ts_;

  class LedDot;
  LedDot* led_dot_ = nullptr;
  QLabel* title_label_ = nullptr;
  QLabel* subtitle_label_ = nullptr;
  QLabel* field_label_ = nullptr;
  QLabel* state_label_ = nullptr;
  QLabel* pulse_label_ = nullptr;
  QLabel* ts_label_ = nullptr;
};

}  // namespace etest::visualizer
