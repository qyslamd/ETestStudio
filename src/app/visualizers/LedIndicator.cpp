#include "LedIndicator.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QRadialGradient>
#include <QVBoxLayout>
#include <QtGlobal>

#include "engine/MonitorManager.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// LedDot — 内部自绘 LED 圆灯（多态语义色，QPainter 立体圆）
// ══════════════════════════════════════════════════════════════════════════════

class LedIndicator::LedDot : public QWidget {
 public:
  explicit LedDot(QWidget* parent = nullptr) : QWidget(parent) {}
  void setLedColor(const QColor& color) {
    color_ = color;
    update();
  }
  void setLedDiameter(int d) {
    led_size_ = d;
    setFixedSize(d, d);
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(0, 0, led_size_, led_size_);

    // 外圈：深色灯壳
    p.setPen(Qt::NoPen);
    p.setBrush(color_.darker(160));
    p.drawEllipse(r);

    // 内芯：状态色
    const QRectF inner = r.adjusted(2, 2, -2, -2);
    p.setBrush(color_);
    p.drawEllipse(inner);

    // 顶部高光：增强立体感
    QRadialGradient glow(inner.center(), inner.width() / 2.0);
    glow.setColorAt(0, QColor(255, 255, 255, 90));
    glow.setColorAt(1, QColor(255, 255, 255, 0));
    p.setBrush(glow);
    p.drawEllipse(inner);
  }

 private:
  QColor color_ = QColor(0x9E, 0x9E, 0x9E);
  int led_size_ = 16;
};

// ══════════════════════════════════════════════════════════════════════════════
// LedIndicator
// ══════════════════════════════════════════════════════════════════════════════

LedIndicator::LedIndicator(QWidget* parent) : SignalVisualizer(parent) {
  setObjectName(QStringLiteral("LedIndicator"));
  setAutoFillBackground(true);
  setDefaultColors();
  setStateText(QString());
  initUi();
}

void LedIndicator::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(4);

  // 两级标题：主标题（监听器名）+ 副标题（连接描述）
  title_label_ = new QLabel(this);
  title_label_->setObjectName(QStringLiteral("LedTitle"));
  layout->addWidget(title_label_);

  subtitle_label_ = new QLabel(this);
  subtitle_label_->setObjectName(QStringLiteral("LedSubtitle"));
  subtitle_label_->setWordWrap(true);
  layout->addWidget(subtitle_label_);

  // LED 行：圆灯 + 字段名 + 状态文字
  auto* ledRow = new QHBoxLayout();
  ledRow->setSpacing(6);

  led_dot_ = new LedDot(this);
  led_dot_->setLedDiameter(led_size_);
  ledRow->addWidget(led_dot_);

  field_label_ = new QLabel(this);
  field_label_->setObjectName(QStringLiteral("LedField"));
  ledRow->addWidget(field_label_);

  state_label_ = new QLabel(QStringLiteral("OFF"), this);
  state_label_->setObjectName(QStringLiteral("LedStateText"));
  ledRow->addWidget(state_label_);

  ledRow->addStretch();
  layout->addLayout(ledRow);

  // 统计行：脉冲计数 + 最后变化时间
  auto* metaRow = new QHBoxLayout();
  metaRow->setSpacing(12);

  pulse_label_ = new QLabel(QStringLiteral("脉冲: 0"), this);
  pulse_label_->setObjectName(QStringLiteral("LedPulse"));
  metaRow->addWidget(pulse_label_);

  ts_label_ = new QLabel(this);
  ts_label_->setObjectName(QStringLiteral("LedTs"));
  metaRow->addWidget(ts_label_);

  metaRow->addStretch();
  layout->addLayout(metaRow);

  layout->addStretch();

  setSubtitle(QString());  // 默认隐藏副标题
  refreshStateVisual();
}

// ── 状态 ──

void LedIndicator::setState(int state) {
  if (state_ != state) {
    state_ = state;
    refreshStateVisual();
  }
}

void LedIndicator::setColorForState(int state, const QColor& color) {
  color_map_[state] = color;
  refreshStateVisual();
}

void LedIndicator::setDefaultColors() {
  color_map_.clear();
  color_map_[0] = QColor(0x9E, 0x9E, 0x9E);  // 灰：关
  color_map_[1] = QColor(0x4C, 0xAF, 0x50);  // 绿：开
  color_map_[2] = QColor(0xF4, 0x43, 0x36);  // 红：告警
}

void LedIndicator::setFieldName(const QString& name) {
  field_name_ = name;
  field_label_->setText(name);
}

void LedIndicator::setStateText(const QString& text) {
  state_text_ = text;
  refreshStateVisual();
}

void LedIndicator::setLedSize(int size) {
  led_size_ = size;
  if (led_dot_) {
    led_dot_->setLedDiameter(size);
  }
}

QColor LedIndicator::stateColor() const {
  return color_map_.value(state_, QColor(0x9E, 0x9E, 0x9E));
}

void LedIndicator::refreshStateVisual() {
  if (led_dot_) {
    led_dot_->setLedColor(stateColor());
  }
  if (state_label_) {
    state_label_->setText(state_text_.isEmpty()
                              ? (state_ >= 1 ? QStringLiteral("ON")
                                             : QStringLiteral("OFF"))
                              : state_text_);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// SignalVisualizer 接口
// ══════════════════════════════════════════════════════════════════════════════

void LedIndicator::onSampleCaptured(
    const etest::engine::MonitorSample& sample) {
  connection_id_ = sample.connectionId;
  const int st = qRound(sample.engValue);
  setState(st);

  // 状态统计：上升沿（OFF→ON）计脉冲，状态变化记时间戳
  const bool on = st >= 1;
  if (on != previous_on_) {
    if (!previous_on_ && on) {
      ++pulse_count_;
      pulse_label_->setText(QStringLiteral("脉冲: %1").arg(pulse_count_));
    }
    last_change_ts_ = sample.timestamp;
    ts_label_->setText(
        last_change_ts_.toString(QStringLiteral("HH:mm:ss.zzz")));
    previous_on_ = on;
  }
}

void LedIndicator::clearData() {
  connection_id_.clear();
  previous_on_ = false;
  pulse_count_ = 0;
  last_change_ts_ = QDateTime();
  setState(0);
  pulse_label_->setText(QStringLiteral("脉冲: 0"));
  ts_label_->setText(QString());
}

QList<QString> LedIndicator::displayedSignals() const {
  if (!connection_id_.isEmpty()) {
    return {connection_id_};
  }
  return {};
}

void LedIndicator::setTitle(const QString& title) {
  title_label_->setText(title);
}

void LedIndicator::setSubtitle(const QString& subtitle) {
  if (subtitle.isEmpty()) {
    subtitle_label_->hide();
  } else {
    subtitle_label_->setText(subtitle);
    subtitle_label_->show();
  }
}

}  // namespace etest::app
