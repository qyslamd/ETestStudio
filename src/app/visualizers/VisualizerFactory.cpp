#include "VisualizerFactory.h"

#include "DigitalMeterWidget.h"
#include "StateLEDWidget.h"
#include "ValueLabelWidget.h"
#include "WaveformWidget.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// createVisualizerFor
// ══════════════════════════════════════════════════════════════════════════════

SignalVisualizer* createVisualizerFor(int monitorIndex,
                                       const QString& displayMode,
                                       const QString& signalType,
                                       const QString& title,
                                       QWidget* parent) {
  Q_UNUSED(monitorIndex)

  // 1. displayMode != "auto" → 按用户配置
  if (displayMode != QStringLiteral("auto")) {
    if (displayMode == QStringLiteral("waveform")) {
      return new WaveformWidget(title, parent);
    }
    if (displayMode == QStringLiteral("led")) {
      return new StateLEDWidget(title, parent);
    }
    if (displayMode == QStringLiteral("meter")) {
      return new DigitalMeterWidget(title, parent);
    }
    if (displayMode == QStringLiteral("frame")) {
      return new ValueLabelWidget(title, parent);
    }
    // 未识别的 displayMode 回退到自动推断
  }

  // 2. displayMode == "auto" → 按 signalType 推断
  if (signalType == QStringLiteral("AD") ||
      signalType == QStringLiteral("DA")) {
    return new WaveformWidget(title, parent);
  }
  if (signalType == QStringLiteral("SERIAL")) {
    // Phase 1 简化：SERIAL 默认用数字表，用户可改 displayMode=led 覆盖
    return new DigitalMeterWidget(title, parent);
  }
  // CAN / A429 等帧类信号用 ValueLabelWidget（Phase 2 改专用组件）

  // 3. 兜底
  return new ValueLabelWidget(title, parent);
}

}  // namespace etest::app
