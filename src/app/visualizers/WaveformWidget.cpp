#include "WaveformWidget.h"

#include <QFont>
#include <QVBoxLayout>

#include <qcustomplot.h>

#include "core_ui/ThemeManager.h"
#include "engine/MonitorManager.h"
#include "logger/Logger.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造
// ══════════════════════════════════════════════════════════════════════════════

WaveformWidget::WaveformWidget(const QString& title, QWidget* parent)
    : SignalVisualizer(parent) {
  initUi();

  title_label_->setText(title);
  setSubtitle(QString());  // 默认隐藏副标题

  // 设置字体
  QFont titleFont = title_label_->font();
  titleFont.setBold(true);
  title_label_->setFont(titleFont);

  // ── QCustomPlot 基本配置 ──
  custom_plot_->setNotAntialiasedElement(QCP::aeAll);
  custom_plot_->setOpenGl(false);

  // X 轴：时间
  QSharedPointer<QCPAxisTickerDateTime> dateTicker(
      new QCPAxisTickerDateTime);
  dateTicker->setDateTimeFormat(QStringLiteral("HH:mm:ss"));
  custom_plot_->xAxis->setTicker(dateTicker);
  custom_plot_->xAxis->setLabel(QStringLiteral("时间"));
  custom_plot_->xAxis->setRange(0, time_window_);

  // Y 轴：工程值
  custom_plot_->yAxis->setLabel(QStringLiteral("工程值"));
  custom_plot_->yAxis->setRange(-1, 1);

  // 允许用户交互
  custom_plot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

  applyTheme();

  connect(&etest::core_ui::ThemeManager::instance(),
          &etest::core_ui::ThemeManager::themeChanged, this,
          [this]() { applyTheme(); });

  // 连接鼠标滚轮缩放（调整时间窗口）
  connect(custom_plot_->xAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
          this, [this](const QCPRange& range) {
    double newWindow = range.size();
    if (newWindow > 0.5 && newWindow < 3600.0) {
      time_window_ = newWindow;
    }
  });
}

void WaveformWidget::setTitle(const QString& title) {
  title_label_->setText(title);
}

void WaveformWidget::setSubtitle(const QString& subtitle) {
  if (subtitle.isEmpty()) {
    subtitle_label_->hide();
  } else {
    subtitle_label_->setText(subtitle);
    subtitle_label_->show();
  }
}

void WaveformWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(2);

  // 标题（两级：主标题 + 副标题连接描述，决策 14）
  title_label_ = new QLabel(this);
  title_label_->setObjectName(QStringLiteral("WaveformTitle"));
  layout->addWidget(title_label_);

  subtitle_label_ = new QLabel(this);
  subtitle_label_->setObjectName(QStringLiteral("WaveformSubtitle"));
  subtitle_label_->setWordWrap(true);
  layout->addWidget(subtitle_label_);

  // QCustomPlot
  custom_plot_ = new QCustomPlot(this);
  custom_plot_->setObjectName(QStringLiteral("WaveformPlot"));
  layout->addWidget(custom_plot_, 1);

  setObjectName(QStringLiteral("WaveformWidget"));
  setAutoFillBackground(true);
}

// ══════════════════════════════════════════════════════════════════════════════
// onSampleCaptured — 追加采样点到对应迹线
// ══════════════════════════════════════════════════════════════════════════════

void WaveformWidget::onSampleCaptured(
    const etest::engine::MonitorSample& sample) {
  int idx = findTraceIndex(sample.connectionId);
  if (idx < 0) {
    LOG_DEBUG("WAVEFORM", "丢弃数据: cid={} eng={} (traces_={}, 无对应迹线)",
              sample.connectionId.toStdString(), sample.engValue,
              traces_.size());
    return;
  }

  Trace& trace = traces_[idx];

  // 时间 key：秒级时间戳
  double key = sample.timestamp.toMSecsSinceEpoch() / 1000.0;
  double value = sample.engValue;

  // 追加数据
  trace.keys.append(key);
  trace.values.append(value);

  // 限制最大点数
  if (trace.keys.size() > kMaxPoints) {
    trace.keys.remove(0, trace.keys.size() - kMaxPoints);
    trace.values.remove(0, trace.values.size() - kMaxPoints);
  }

  // 更新 graph 数据
  trace.graph->setData(trace.keys, trace.values);

  // 更新 X 轴范围
  last_key_ = key;
  custom_plot_->xAxis->setRange(last_key_ - time_window_, last_key_);

  // 自动调整 Y 轴范围
  custom_plot_->yAxis->rescale(true);
  custom_plot_->yAxis->setRange(
      custom_plot_->yAxis->range().lower * 1.05,
      custom_plot_->yAxis->range().upper * 1.05);

  custom_plot_->replot();
}

// ══════════════════════════════════════════════════════════════════════════════
// clearData — 清空所有迹线数据
// ══════════════════════════════════════════════════════════════════════════════

void WaveformWidget::clearData() {
  for (int i = 0; i < traces_.size(); ++i) {
    traces_[i].keys.clear();
    traces_[i].values.clear();
    if (traces_[i].graph) {
      traces_[i].graph->setData(traces_[i].keys, traces_[i].values);
    }
  }
  last_key_ = 0.0;
  custom_plot_->yAxis->setRange(-1, 1);
  custom_plot_->replot();
}

// ══════════════════════════════════════════════════════════════════════════════
// displayedSignals — 返回当前所有迹线的标识列表
// ══════════════════════════════════════════════════════════════════════════════

QList<QString> WaveformWidget::displayedSignals() const {
  QList<QString> result;
  result.reserve(traces_.size());
  for (int i = 0; i < traces_.size(); ++i) {
    result.append(traces_[i].connectionId);
  }
  return result;
}

// ══════════════════════════════════════════════════════════════════════════════
// addTrace — 添加新迹线
// ══════════════════════════════════════════════════════════════════════════════

void WaveformWidget::addTrace(const QString& connectionId,
                              const QColor& color) {
  if (findTraceIndex(connectionId) >= 0) {
    return;
  }

  Trace trace;
  trace.connectionId = connectionId;
  trace.color = color;

  trace.graph = custom_plot_->addGraph();
  trace.graph->setPen(QPen(color, 1.5));
  trace.graph->setAntialiased(true);
  trace.graph->setLineStyle(QCPGraph::lsLine);
  trace.graph->setScatterStyle(QCPScatterStyle::ssNone);

  traces_.append(trace);
}

// ══════════════════════════════════════════════════════════════════════════════
// removeTrace — 移除迹线
// ══════════════════════════════════════════════════════════════════════════════

void WaveformWidget::removeTrace(const QString& connectionId) {
  for (int i = 0; i < traces_.size(); ++i) {
    if (traces_[i].connectionId == connectionId) {
      custom_plot_->removeGraph(traces_[i].graph);
      traces_.removeAt(i);
      custom_plot_->replot();
      return;
    }
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// helper
// ══════════════════════════════════════════════════════════════════════════════

int WaveformWidget::findTraceIndex(const QString& connectionId) const {
  for (int i = 0; i < traces_.size(); ++i) {
    if (traces_[i].connectionId == connectionId) {
      return i;
    }
  }
  return -1;
}

void WaveformWidget::updateAxes() {
  custom_plot_->xAxis->setRange(last_key_ - time_window_, last_key_);
  custom_plot_->yAxis->rescale(true);
  custom_plot_->replot();
}

void WaveformWidget::applyTheme() {
  auto& tm = etest::core_ui::ThemeManager::instance();

  QColor bg = tm.panelBackground();
  QColor plotBg = tm.windowBackground();
  QColor axisClr = tm.textColor();
  QColor tickClr = tm.secondaryTextColor();
  QColor gridClr = tm.borderColor();
  QColor subGridClr = tm.borderColor().lighter(120);

  custom_plot_->setBackground(QBrush(bg));
  custom_plot_->axisRect()->setBackground(QBrush(plotBg));

  for (auto* axis : {custom_plot_->xAxis, custom_plot_->yAxis,
                     custom_plot_->xAxis2, custom_plot_->yAxis2}) {
    axis->setBasePen(QPen(axisClr, 1));
    axis->setTickPen(QPen(tickClr, 1));
    axis->setSubTickPen(QPen(tickClr, 1));
    axis->setTickLabelColor(tickClr);
    axis->setLabelColor(axisClr);
    axis->grid()->setPen(QPen(gridClr, 1));
    axis->grid()->setSubGridPen(QPen(subGridClr, 1));
    axis->grid()->setZeroLinePen(QPen(axisClr, 1));
  }

  custom_plot_->replot();
}

}  // namespace etest::app
