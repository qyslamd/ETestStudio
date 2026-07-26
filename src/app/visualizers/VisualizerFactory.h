#ifndef ETEST_APP_VISUALIZERS_VISUALIZER_FACTORY_H_
#define ETEST_APP_VISUALIZERS_VISUALIZER_FACTORY_H_

#include <QString>
#include <QWidget>

namespace etest::app {

class SignalVisualizer;

// ══════════════════════════════════════════════════════════════════════════════
// createVisualizerFor — 按 displayMode + signalType 创建可视化组件
// ══════════════════════════════════════════════════════════════════════════════
// 映射规则：
//   1. displayMode != "auto" → 按用户配置创建：
//        "waveform" → WaveformWidget
//        "led"      → StateLEDWidget
//        "meter"    → DigitalMeterWidget
//        "frame"    → ValueLabelWidget
//   2. displayMode == "auto" → 按 signalType 推断：
//        "AD" / "DA"  → WaveformWidget
//        "SERIAL"     → DigitalMeterWidget（Phase 1 简化）
//        其他         → ValueLabelWidget
//   3. 兜底 → ValueLabelWidget
// ══════════════════════════════════════════════════════════════════════════════
SignalVisualizer* createVisualizerFor(int monitorIndex,
                                       const QString& displayMode,
                                       const QString& signalType,
                                       const QString& title,
                                       QWidget* parent = nullptr);

}  // namespace etest::app

#endif  // ETEST_APP_VISUALIZERS_VISUALIZER_FACTORY_H_
