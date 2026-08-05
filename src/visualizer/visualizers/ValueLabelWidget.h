#pragma once

#include <QLabel>
#include <QPair>

#include "SignalVisualizer.h"
#include "engine/MonitorManager.h"  // MonitorSample

namespace etest::visualizer {

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
  QList<QString> displayedSignals() const override;

  void setTitle(const QString& title) override;
  void setSubtitle(const QString& subtitle) override;

 private:
  void initUi();

  QString title_;
  QLabel* title_label_ = nullptr;    // 主标题
  QLabel* subtitle_label_ = nullptr; // 副标题（连接描述）
  QLabel* value_label_ = nullptr;    // 工程值大字体
  QLabel* raw_label_ = nullptr;      // 原始值
  QLabel* ts_label_ = nullptr;       // 最后采样时间
  QString connection_id_;
};

}  // namespace etest::visualizer

