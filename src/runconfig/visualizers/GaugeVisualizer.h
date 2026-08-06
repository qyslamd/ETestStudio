#pragma once

#include <QColor>
#include <QList>
#include <QWidget>

#include "SignalVisualizer.h"

class QLabel;
class QPainter;
class QPaintEvent;

namespace etest::runconfig {

// ══════════════════════════════════════════════════════════════════════════════
// GaugeCanvas — 模拟仪表盘自绘子画布
// ══════════════════════════════════════════════════════════════════════════════
// 由 GaugeVisualizer 持有，负责 QPainter 绘制盘面/刻度/指针/数值。
// 颜色每次绘制时从 ThemeManager 语义色刷新（方案决策 1 映射）。
// ══════════════════════════════════════════════════════════════════════════════
class GaugeCanvas : public QWidget {
  Q_OBJECT

 public:
  // 饼图（彩色弧）样式
  enum class PieStyle {
    Three,    // 三色圆环
    Current,  // 当前值圆环
  };

  // 指针样式
  enum class PointerStyle {
    Circle,       // 圆形指示器
    Indicator,    // 指针指示器
    IndicatorR,   // 圆角指针指示器
    Triangle,     // 三角形指示器
  };

  explicit GaugeCanvas(QWidget* parent = nullptr);

  // 设置当前值（钳制在 [min, max]），值变化才触发重绘
  void setValue(double value);
  // 归零到量程起点
  void resetValue();

  void setRange(double minValue, double maxValue);
  void setPrecision(int precision);
  void setScaleMajor(int scaleMajor);
  void setScaleMinor(int scaleMinor);
  void setStartAngle(int startAngle);
  void setEndAngle(int endAngle);

  void setPointerStyle(PointerStyle pointerStyle);
  void setPieStyle(PieStyle pieStyle);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void applyThemeColors();
  void drawOuterCircle(QPainter* painter);
  void drawInnerCircle(QPainter* painter);
  void drawColorPie(QPainter* painter);
  void drawCoverCircle(QPainter* painter);
  void drawScale(QPainter* painter);
  void drawScaleNum(QPainter* painter);
  void drawPointerCircle(QPainter* painter);
  void drawPointerIndicator(QPainter* painter);
  void drawPointerIndicatorR(QPainter* painter);
  void drawPointerTriangle(QPainter* painter);
  void drawRoundCircle(QPainter* painter);
  void drawCenterCircle(QPainter* painter);
  void drawValue(QPainter* painter);

  double min_value_ = 0.0;
  double max_value_ = 100.0;
  double current_value_ = 0.0;
  int precision_ = 0;

  int scale_major_ = 10;
  int scale_minor_ = 10;
  int start_angle_ = 45;
  int end_angle_ = 45;

  QColor outer_circle_color_;
  QColor inner_circle_color_;
  QColor pie_color_start_;
  QColor pie_color_mid_;
  QColor pie_color_end_;
  QColor cover_circle_color_;
  QColor scale_color_;
  QColor pointer_color_;
  QColor center_circle_color_;
  QColor text_color_;

  PieStyle pie_style_ = PieStyle::Three;
  PointerStyle pointer_style_ = PointerStyle::Indicator;
};

// ══════════════════════════════════════════════════════════════════════════════
// GaugeVisualizer — 指针表可视化组件
// ══════════════════════════════════════════════════════════════════════════════
// 结构对齐 WaveformWidget：根控件（QSS 背景）→ 标题 QLabel + GaugeCanvas 子画布。
// onSampleCaptured 将 engValue 写入画布驱动指针。
// ══════════════════════════════════════════════════════════════════════════════
class GaugeVisualizer : public SignalVisualizer {
  Q_OBJECT

 public:
  explicit GaugeVisualizer(const QString& title, QWidget* parent = nullptr);

  void onSampleCaptured(const etest::engine::MonitorSample& sample) override;
  void clearData() override;
  QList<QString> displayedSignals() const override;

  void setTitle(const QString& title) override;
  void setSubtitle(const QString& subtitle) override;
  void setSubtitleState(const QString& state) override;
  QSize sizeHint() const override;

 private:
  void initUi();

  QString title_;
  QLabel* title_label_ = nullptr;
  QLabel* subtitle_label_ = nullptr;
  GaugeCanvas* canvas_ = nullptr;

  QString connection_id_;
};

}  // namespace etest::runconfig
