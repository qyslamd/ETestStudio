#ifndef ETEST_APP_VISUALIZERS_SIGNAL_VISUALIZER_H_
#define ETEST_APP_VISUALIZERS_SIGNAL_VISUALIZER_H_

#include <QPair>
#include <QWidget>

namespace etest::engine {
struct MonitorSample;
}  // namespace etest::engine

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// SignalVisualizer — 信号可视化组件抽象基类
// ══════════════════════════════════════════════════════════════════════════════
// 所有具体的可视化组件（WaveformWidget / StateLEDWidget / DigitalMeterWidget
// / ValueLabelWidget / CANFrameWidget / A429LabelWidget）均继承此类。
// 通过 MonitorManager::subscribe 接收采样数据，由 onSampleCaptured 驱动更新。
// ══════════════════════════════════════════════════════════════════════════════
class SignalVisualizer : public QWidget {
  Q_OBJECT

 public:
  explicit SignalVisualizer(QWidget* parent = nullptr);
  ~SignalVisualizer() override = default;

  // ── 接收一个采样点 ──
  // 由 MonitorManager 的订阅回调调用，子类在此方法中更新 UI。
  virtual void onSampleCaptured(const etest::engine::MonitorSample& sample) = 0;

  // ── 清空所有数据（如运行停止或重新开始） ──
  virtual void clearData() = 0;

  // ── 返回当前展示的信号标识列表（monitorIndex 列表） ──
  virtual QList<int> displayedSignals() const = 0;
};

}  // namespace etest::app

#endif  // ETEST_APP_VISUALIZERS_SIGNAL_VISUALIZER_H_
