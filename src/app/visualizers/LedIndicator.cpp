#include "LedIndicator.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QRadialGradient>
#include <QtGlobal>

#include "engine/MonitorManager.h"

namespace etest::app {

LedIndicator::LedIndicator(QWidget* parent) : SignalVisualizer(parent) {
  // 背景色走 QSS（#LedIndicator），与其它 visualizer 卡片一致
  setObjectName(QStringLiteral("LedIndicator"));
  setAutoFillBackground(true);
  setDefaultColors();
  setStateText(QStringLiteral("OFF"));
}

void LedIndicator::setState(int state) {
  if (state_ != state) {
    state_ = state;
    update();
  }
}

void LedIndicator::setColorForState(int state, const QColor& color) {
  color_map_[state] = color;
  update();
}

void LedIndicator::setDefaultColors() {
  color_map_.clear();
  color_map_[0] = QColor(0x9E, 0x9E, 0x9E);  // 灰：关
  color_map_[1] = QColor(0x4C, 0xAF, 0x50);  // 绿：开
  color_map_[2] = QColor(0xF4, 0x43, 0x36);  // 红：告警
}

void LedIndicator::setFieldName(const QString& name) {
  field_name_ = name;
  updateGeometry();
  update();
}

void LedIndicator::setStateText(const QString& text) {
  state_text_ = text;
  update();
}

void LedIndicator::setLedSize(int size) {
  led_size_ = size;
  updateGeometry();
  update();
}

// ══════════════════════════════════════════════════════════════════════════════
// SignalVisualizer 接口
// ══════════════════════════════════════════════════════════════════════════════

void LedIndicator::onSampleCaptured(
    const etest::engine::MonitorSample& sample) {
  connection_id_ = sample.connectionId;
  // 状态值 = 工程值取整（0/1/2 对应 灰/绿/红），颜色映射由 setColorForState 配置
  setState(qRound(sample.engValue));
}

void LedIndicator::clearData() {
  connection_id_.clear();
  setState(0);
}

QList<QString> LedIndicator::displayedSignals() const {
  if (!connection_id_.isEmpty()) {
    return {connection_id_};
  }
  return {};
}

void LedIndicator::setTitle(const QString& title) {
  setFieldName(title);
}

void LedIndicator::setSubtitle(const QString& subtitle) {
  Q_UNUSED(subtitle)  // 紧凑组件，不显示副标题
}

// ══════════════════════════════════════════════════════════════════════════════
// 绘制
// ══════════════════════════════════════════════════════════════════════════════

QSize LedIndicator::sizeHint() const {
  QFontMetrics fm(font());
  int textW = fm.horizontalAdvance(field_name_) +
              fm.horizontalAdvance(state_text_);
  return QSize(led_size_ + 6 + textW + 8, qMax(led_size_ + 4, fm.height() + 4));
}

QSize LedIndicator::minimumSizeHint() const {
  return QSize(led_size_ + 6, led_size_ + 4);
}

void LedIndicator::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // LED 圆点：外圈灯壳 + 内芯状态色 + 顶部高光
  const qreal y = (height() - led_size_) / 2.0;
  const QRectF ledRect(0, y, led_size_, led_size_);
  const QColor color = color_map_.value(state_, QColor(0x9E, 0x9E, 0x9E));

  // 外圈：深色灯壳
  p.setPen(Qt::NoPen);
  p.setBrush(color.darker(160));
  p.drawEllipse(ledRect);

  // 内芯：状态色
  const QRectF inner = ledRect.adjusted(2, 2, -2, -2);
  p.setBrush(color);
  p.drawEllipse(inner);

  // 顶部高光：左上小半透明亮斑，增强立体感
  QRadialGradient glow(inner.center(), inner.width() / 2.0);
  glow.setColorAt(0, QColor(255, 255, 255, 90));
  glow.setColorAt(1, QColor(255, 255, 255, 0));
  p.setBrush(glow);
  p.drawEllipse(inner);

  // 文字：字段名（粗体，主题文字色）+ 状态文字（普通，状态色）
  const int textX = led_size_ + 6;
  QFont f = font();
  f.setPixelSize(11);

  int nameW = 0;
  if (!field_name_.isEmpty()) {
    f.setBold(true);
    p.setFont(f);
    QFontMetrics fm(f);
    nameW = fm.horizontalAdvance(field_name_);
    p.setPen(palette().color(QPalette::Text));
    p.drawText(QRect(textX, 0, nameW, height()),
               Qt::AlignVCenter | Qt::AlignLeft, field_name_);
  }

  if (!state_text_.isEmpty()) {
    f.setBold(false);
    p.setFont(f);
    const int sx = textX + nameW + 8;
    p.setPen(color);
    p.drawText(QRect(sx, 0, width() - sx, height()),
               Qt::AlignVCenter | Qt::AlignLeft, state_text_);
  }
}

}  // namespace etest::app
