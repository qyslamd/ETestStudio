#include "GaugeVisualizer.h"

#include <QFont>
#include <QPainter>
#include <QPolygon>
#include <QtGlobal>
#include <QtMath>

#include <QLabel>
#include <QVBoxLayout>

#include "ThemeManager.h"
#include "engine/MonitorManager.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// GaugeCanvas
// ══════════════════════════════════════════════════════════════════════════════

GaugeCanvas::GaugeCanvas(QWidget* parent) : QWidget(parent) {
  setMinimumSize(120, 120);
}

// ── 公共接口 ──

void GaugeCanvas::setValue(double value) {
  const double clamped = qBound(min_value_, value, max_value_);
  if (current_value_ != clamped) {
    current_value_ = clamped;
    update();
  }
}

void GaugeCanvas::resetValue() {
  current_value_ = min_value_;
  update();
}

void GaugeCanvas::setRange(double minValue, double maxValue) {
  min_value_ = minValue;
  max_value_ = maxValue;
  update();
}

void GaugeCanvas::setPrecision(int precision) {
  precision_ = precision;
  update();
}

void GaugeCanvas::setScaleMajor(int scaleMajor) {
  scale_major_ = scaleMajor;
  update();
}

void GaugeCanvas::setScaleMinor(int scaleMinor) {
  scale_minor_ = scaleMinor;
  update();
}

void GaugeCanvas::setStartAngle(int startAngle) {
  start_angle_ = startAngle;
  update();
}

void GaugeCanvas::setEndAngle(int endAngle) {
  end_angle_ = endAngle;
  update();
}

void GaugeCanvas::setPointerStyle(PointerStyle pointerStyle) {
  pointer_style_ = pointerStyle;
  update();
}

void GaugeCanvas::setPieStyle(PieStyle pieStyle) {
  pie_style_ = pieStyle;
  update();
}

// ── 主题色 ──

void GaugeCanvas::applyThemeColors() {
  auto& tm = etest::core_ui::ThemeManager::instance();
  outer_circle_color_ = tm.borderColor();
  inner_circle_color_ = tm.panelBackground();
  cover_circle_color_ = tm.panelBackground();
  scale_color_ = tm.textColor();
  pointer_color_ = tm.accentColor();
  center_circle_color_ = tm.textColor();
  text_color_ = tm.textColor();
  // 三色饼弧固定语义色，与 QSS status 样式同源、跨主题一致
  pie_color_start_ = QColor(0x22, 0xC5, 0x5E);
  pie_color_mid_ = QColor(0xF5, 0x9E, 0x0B);
  pie_color_end_ = QColor(0xEF, 0x44, 0x44);
}

// ── 绘制 ──

void GaugeCanvas::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)
  applyThemeColors();

  const int width = this->width();
  const int height = this->height();
  const int side = qMin(width, height);

  QPainter painter(this);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
  painter.translate(width / 2, height / 2);
  painter.scale(side / 200.0, side / 200.0);

  drawOuterCircle(&painter);
  drawInnerCircle(&painter);
  drawColorPie(&painter);
  drawCoverCircle(&painter);
  drawScale(&painter);
  drawScaleNum(&painter);

  switch (pointer_style_) {
    case PointerStyle::Circle:
      drawPointerCircle(&painter);
      break;
    case PointerStyle::Indicator:
      drawPointerIndicator(&painter);
      break;
    case PointerStyle::IndicatorR:
      drawPointerIndicatorR(&painter);
      break;
    case PointerStyle::Triangle:
      drawPointerTriangle(&painter);
      break;
  }

  drawRoundCircle(&painter);
  drawCenterCircle(&painter);
  drawValue(&painter);
}

void GaugeCanvas::drawOuterCircle(QPainter* painter) {
  const int radius = 99;
  painter->save();
  painter->setPen(Qt::NoPen);
  painter->setBrush(outer_circle_color_);
  painter->drawEllipse(-radius, -radius, radius * 2, radius * 2);
  painter->restore();
}

void GaugeCanvas::drawInnerCircle(QPainter* painter) {
  const int radius = 90;
  painter->save();
  painter->setPen(Qt::NoPen);
  painter->setBrush(inner_circle_color_);
  painter->drawEllipse(-radius, -radius, radius * 2, radius * 2);
  painter->restore();
}

void GaugeCanvas::drawColorPie(QPainter* painter) {
  const int radius = 60;
  painter->save();
  painter->setPen(Qt::NoPen);

  const QRectF rect(-radius, -radius, radius * 2, radius * 2);

  if (pie_style_ == PieStyle::Three) {
    // 计算总范围角度，按固定比例切三色（绿/琥珀/红）
    const double angleAll = 360.0 - start_angle_ - end_angle_;
    const double angleStart = angleAll * 0.7;
    const double angleMid = angleAll * 0.15;
    const double angleEnd = angleAll * 0.15;

    // 增加偏移量使得看起来没有脱节
    const int offset = 3;

    // 起点对齐 value=0（90 + start_angle_），随值域顺时针绿→琥珀→红，
    // 保证色环覆盖范围与刻度弧一致、顶部无断口
    const double base = 90.0 + start_angle_;

    painter->setBrush(pie_color_start_);
    painter->drawPie(rect, base * 16, angleStart * 16);

    painter->setBrush(pie_color_mid_);
    painter->drawPie(rect, (base + angleStart) * 16 + offset, angleMid * 16);

    painter->setBrush(pie_color_end_);
    painter->drawPie(rect,
                     (base + angleStart + angleMid) * 16 + offset * 2,
                     angleEnd * 16);
  } else if (pie_style_ == PieStyle::Current) {
    // 当前值圆环：值范围内着起始色，剩余部分着结束色。
    // 起点同样对齐 value=0（90 + start_angle_），与刻度/指针/三色弧一致。
    const double angleAll = 360.0 - start_angle_ - end_angle_;
    const double angleCurrent =
        angleAll * ((current_value_ - min_value_) / (max_value_ - min_value_));
    const double angleOther = angleAll - angleCurrent;

    const double base = 90.0 + start_angle_;

    painter->setBrush(pie_color_start_);
    painter->drawPie(rect, base * 16, angleCurrent * 16);

    painter->setBrush(pie_color_end_);
    painter->drawPie(rect, (base + angleCurrent) * 16, angleOther * 16);
  }

  painter->restore();
}

void GaugeCanvas::drawCoverCircle(QPainter* painter) {
  const int radius = 50;
  painter->save();
  painter->setPen(Qt::NoPen);
  painter->setBrush(cover_circle_color_);
  painter->drawEllipse(-radius, -radius, radius * 2, radius * 2);
  painter->restore();
}

void GaugeCanvas::drawScale(QPainter* painter) {
  const int radius = 72;
  painter->save();

  painter->rotate(start_angle_);
  const int steps = scale_major_ * scale_minor_;
  const double angleStep = (360.0 - start_angle_ - end_angle_) / steps;

  QPen pen;
  pen.setColor(scale_color_);
  pen.setCapStyle(Qt::RoundCap);

  for (int i = 0; i <= steps; i++) {
    if (i % scale_minor_ == 0) {
      pen.setWidthF(1.5);
      painter->setPen(pen);
      painter->drawLine(0, radius - 10, 0, radius);
    } else {
      pen.setWidthF(0.5);
      painter->setPen(pen);
      painter->drawLine(0, radius - 5, 0, radius);
    }
    painter->rotate(angleStep);
  }

  painter->restore();
}

void GaugeCanvas::drawScaleNum(QPainter* painter) {
  const int radius = 82;
  painter->save();
  painter->setPen(scale_color_);

  const double startRad = (360 - start_angle_ - 90) * (M_PI / 180);
  const double deltaRad =
      (360 - start_angle_ - end_angle_) * (M_PI / 180) / scale_major_;

  for (int i = 0; i <= scale_major_; i++) {
    const double sina = qSin(startRad - i * deltaRad);
    const double cosa = qCos(startRad - i * deltaRad);
    const double value =
        1.0 * i * ((max_value_ - min_value_) / scale_major_) + min_value_;

    const QString strValue =
        QString("%1").arg(static_cast<double>(value), 0, 'f', precision_);
    const double textWidth = fontMetrics().horizontalAdvance(strValue);
    const double textHeight = fontMetrics().height();
    const int x = radius * cosa - textWidth / 2;
    const int y = -radius * sina + textHeight / 4;
    painter->drawText(x, y, strValue);
  }

  painter->restore();
}

void GaugeCanvas::drawPointerCircle(QPainter* painter) {
  const int radius = 6;
  const int offset = 30;
  painter->save();
  painter->setPen(Qt::NoPen);
  painter->setBrush(pointer_color_);

  painter->rotate(start_angle_);
  const double degRotate =
      (360.0 - start_angle_ - end_angle_) / (max_value_ - min_value_) *
      (current_value_ - min_value_);
  painter->rotate(degRotate);
  painter->drawEllipse(-radius, radius + offset, radius * 2, radius * 2);

  painter->restore();
}

void GaugeCanvas::drawPointerIndicator(QPainter* painter) {
  const int radius = 75;
  painter->save();
  painter->setOpacity(0.8);
  painter->setPen(Qt::NoPen);
  painter->setBrush(pointer_color_);

  QPolygon pts;
  pts.setPoints(3, -5, 0, 5, 0, 0, radius);

  painter->rotate(start_angle_);
  const double degRotate =
      (360.0 - start_angle_ - end_angle_) / (max_value_ - min_value_) *
      (current_value_ - min_value_);
  painter->rotate(degRotate);
  painter->drawConvexPolygon(pts);

  painter->restore();
}

void GaugeCanvas::drawPointerIndicatorR(QPainter* painter) {
  const int radius = 75;
  painter->save();
  painter->setOpacity(1.0);

  QPen pen;
  pen.setWidth(1);
  pen.setColor(pointer_color_);
  painter->setPen(pen);
  painter->setBrush(pointer_color_);

  QPolygon pts;
  pts.setPoints(3, -5, 0, 5, 0, 0, radius);

  painter->rotate(start_angle_);
  const double degRotate =
      (360.0 - start_angle_ - end_angle_) / (max_value_ - min_value_) *
      (current_value_ - min_value_);
  painter->rotate(degRotate);
  painter->drawConvexPolygon(pts);

  // 与三角形重叠的圆角直线，形成圆角指针
  pen.setCapStyle(Qt::RoundCap);
  pen.setWidthF(4);
  painter->setPen(pen);
  painter->drawLine(0, 0, 0, radius);

  painter->restore();
}

void GaugeCanvas::drawPointerTriangle(QPainter* painter) {
  const int radius = 10;
  const int offset = 38;
  painter->save();
  painter->setPen(Qt::NoPen);
  painter->setBrush(pointer_color_);

  QPolygon pts;
  pts.setPoints(3, -5, 0 + offset, 5, 0 + offset, 0, radius + offset);

  painter->rotate(start_angle_);
  const double degRotate =
      (360.0 - start_angle_ - end_angle_) / (max_value_ - min_value_) *
      (current_value_ - min_value_);
  painter->rotate(degRotate);
  painter->drawConvexPolygon(pts);

  painter->restore();
}

void GaugeCanvas::drawRoundCircle(QPainter* painter) {
  const int radius = 18;
  painter->save();
  painter->setOpacity(0.8);
  painter->setPen(Qt::NoPen);
  painter->setBrush(pointer_color_);
  painter->drawEllipse(-radius, -radius, radius * 2, radius * 2);
  painter->restore();
}

void GaugeCanvas::drawCenterCircle(QPainter* painter) {
  const int radius = 15;
  painter->save();
  painter->setPen(Qt::NoPen);
  painter->setBrush(center_circle_color_);
  painter->drawEllipse(-radius, -radius, radius * 2, radius * 2);
  painter->restore();
}

void GaugeCanvas::drawValue(QPainter* painter) {
  const int radius = 100;
  painter->save();
  painter->setPen(text_color_);

  QFont font;
  font.setPixelSize(18);
  painter->setFont(font);

  const QRectF textRect(-radius, -radius, radius * 2, radius * 2);
  const QString strValue = QString("%1")
                               .arg(static_cast<double>(current_value_), 0, 'f',
                                    precision_);
  painter->drawText(textRect, Qt::AlignCenter, strValue);

  painter->restore();
}

// ══════════════════════════════════════════════════════════════════════════════
// GaugeVisualizer
// ══════════════════════════════════════════════════════════════════════════════

GaugeVisualizer::GaugeVisualizer(const QString& title, QWidget* parent)
    : SignalVisualizer(parent), title_(title) {
  initUi();
}

void GaugeVisualizer::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(2);

  title_label_ = new QLabel(title_, this);
  title_label_->setObjectName(QStringLiteral("GaugeTitle"));
  layout->addWidget(title_label_);

  canvas_ = new GaugeCanvas(this);
  canvas_->setObjectName(QStringLiteral("GaugeCanvas"));
  layout->addWidget(canvas_, 1);

  setObjectName(QStringLiteral("GaugeWidget"));
  setAutoFillBackground(true);
}

void GaugeVisualizer::onSampleCaptured(
    const etest::engine::MonitorSample& sample) {
  monitor_index_ = sample.monitorIndex;
  canvas_->setValue(sample.engValue);
}

void GaugeVisualizer::clearData() {
  canvas_->resetValue();
  monitor_index_ = -1;
}

QList<int> GaugeVisualizer::displayedSignals() const {
  if (monitor_index_ >= 0) {
    return {monitor_index_};
  }
  return {};
}

}  // namespace etest::app
