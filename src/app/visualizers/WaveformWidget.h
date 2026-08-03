#ifndef ETEST_APP_VISUALIZERS_WAVEFORM_WIDGET_H_
#define ETEST_APP_VISUALIZERS_WAVEFORM_WIDGET_H_

#include <QColor>
#include <QLabel>
#include <QList>

#include "SignalVisualizer.h"

class QCustomPlot;
class QCPGraph;

namespace etest::app {

class WaveformWidget : public SignalVisualizer {
  Q_OBJECT

 public:
  explicit WaveformWidget(const QString& title, QWidget* parent = nullptr);

  void onSampleCaptured(const etest::engine::MonitorSample& sample) override;
  void clearData() override;
  QList<QString> displayedSignals() const override;

  void setTitle(const QString& title) override;
  void setSubtitle(const QString& subtitle) override;

  void addTrace(const QString& connectionId, const QColor& color);
  void removeTrace(const QString& connectionId);

  void setTimeWindow(double seconds) { time_window_ = qMax(1.0, seconds); }
  double timeWindow() const { return time_window_; }

 private:
  struct Trace {
    QString connectionId;
    QColor color;
    QCPGraph* graph = nullptr;
    QVector<double> keys;
    QVector<double> values;
  };

  void initUi();
  void applyTheme();
  int findTraceIndex(const QString& connectionId) const;
  void updateAxes();

  QLabel* title_label_ = nullptr;
  QLabel* subtitle_label_ = nullptr;
  QCustomPlot* custom_plot_ = nullptr;
  QList<Trace> traces_;
  double time_window_ = 30.0;
  double last_key_ = 0.0;
  static constexpr int kMaxPoints = 10000;
};

}  // namespace etest::app

#endif  // ETEST_APP_VISUALIZERS_WAVEFORM_WIDGET_H_
