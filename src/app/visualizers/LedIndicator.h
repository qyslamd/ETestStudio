#pragma once

#include <QColor>
#include <QMap>
#include <QWidget>

#include "visualizers/SignalVisualizer.h"

namespace etest::app {

// LED 圆灯可视化组件：按状态值映射语义色（默认 0灰/1绿/2红，可按字段
// 语义覆盖），带可配置字段名与状态文字。用于展示帧内 1bit 状态字段
// （开/关、正常/故障、告警等），作为 SignalVisualizer 由监听器驱动。
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

 protected:
  void paintEvent(QPaintEvent* event) override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 private:
  int state_ = 0;
  QMap<int, QColor> color_map_;
  QString field_name_;
  QString state_text_;
  QString connection_id_;
  int led_size_ = 14;
};

}  // namespace etest::app
