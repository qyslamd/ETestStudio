#ifndef ETEST_APP_VISUALIZERS_WAVEFORM_WIDGET_H_
#define ETEST_APP_VISUALIZERS_WAVEFORM_WIDGET_H_

#include <QColor>
#include <QLabel>
#include <QList>
#include <QPair>

#include "SignalVisualizer.h"

class QCustomPlot;
class QCPGraph;

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// WaveformWidget — AD/DA 滚动波形图（基于 QCustomPlot）
// ══════════════════════════════════════════════════════════════════════════════
// 以 QCustomPlot 为渲染引擎，支持多迹线叠加（addTrace / removeTrace）。
// X 轴为时间轴（QCPAxisTickerDateTime），Y 轴为工程值。
// 鼠标滚轮缩放时间窗口，悬停显示十字线数值。
// ══════════════════════════════════════════════════════════════════════════════
class WaveformWidget : public SignalVisualizer {
  Q_OBJECT

 public:
  explicit WaveformWidget(const QString& title, QWidget* parent = nullptr);

  void onSampleCaptured(const etest::engine::MonitorSample& sample) override;
  void clearData() override;
  QList<QPair<int, int>> displayedSignals() const override;

  // ── 添加/移除迹线（同一坐标系叠加不同信号） ──
  void addTrace(int monitorIndex, int channelIndex, const QColor& color);
  void removeTrace(int monitorIndex, int channelIndex);

  // ── 设置时间窗口（秒） ──
  void setTimeWindow(double seconds) { time_window_ = qMax(1.0, seconds); }
  double timeWindow() const { return time_window_; }

 private:
  struct Trace {
    int monitorIndex = -1;
    int channelIndex = -1;
    QColor color;
    QCPGraph* graph = nullptr;
    QVector<double> keys;
    QVector<double> values;
  };

  void initUi();
  void applyTheme();
  int findTraceIndex(int monitorIndex, int channelIndex) const;
  void updateAxes();

  QLabel* title_label_ = nullptr;
  QCustomPlot* custom_plot_ = nullptr;
  QList<Trace> traces_;
  double time_window_ = 30.0;
  double last_key_ = 0.0;
  static constexpr int kMaxPoints = 10000;
};

}  // namespace etest::app

#endif  // ETEST_APP_VISUALIZERS_WAVEFORM_WIDGET_H_
