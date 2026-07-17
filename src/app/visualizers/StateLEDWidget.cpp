#include "StateLEDWidget.h"

#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>

#include "engine/MonitorManager.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造
// ══════════════════════════════════════════════════════════════════════════════

StateLEDWidget::StateLEDWidget(const QString& title, QWidget* parent)
    : SignalVisualizer(parent), title_(title) {
  initUi();
}

void StateLEDWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(4);

  // 标题
  auto* title_label = new QLabel(title_, this);
  title_label->setObjectName(QStringLiteral("LEDTitle"));
  layout->addWidget(title_label);

  // LED + 状态水平行
  auto* ledRow = new QHBoxLayout();
  ledRow->setSpacing(6);

  // LED 圆点（用 QLabel 固定大小 + QSS border-radius 模拟圆形）
  led_label_ = new QLabel(this);
  led_label_->setObjectName(QStringLiteral("LEDDot"));
  led_label_->setFixedSize(20, 20);
  ledRow->addWidget(led_label_);

  // ON/OFF 文本
  state_label_ = new QLabel(QStringLiteral("OFF"), this);
  state_label_->setObjectName(QStringLiteral("LEDStateText"));
  ledRow->addWidget(state_label_);

  ledRow->addStretch();
  layout->addLayout(ledRow);

  // 脉冲计数
  pulse_label_ = new QLabel(QStringLiteral("脉冲: 0"), this);
  pulse_label_->setObjectName(QStringLiteral("LEDPulse"));
  layout->addWidget(pulse_label_);

  // 最后变化时间
  ts_label_ = new QLabel(this);
  ts_label_->setObjectName(QStringLiteral("LEDTs"));
  layout->addWidget(ts_label_);

  layout->addStretch();

  // 初始：OFF（红色）
  updateLED(false);
}

// ══════════════════════════════════════════════════════════════════════════════
// onSampleCaptured — 更新 LED 状态
// ══════════════════════════════════════════════════════════════════════════════

void StateLEDWidget::onSampleCaptured(
    const etest::engine::MonitorSample& sample) {
  monitor_index_ = sample.monitorIndex;
  channel_index_ = sample.channelIndex;

  bool current_on = (sample.engValue >= 0.5);

  // 状态变化时才更新 LED 外观、脉冲计数和时间戳
  if (current_on != previous_on_) {
    // 检测上升沿（OFF → ON）
    if (!previous_on_ && current_on) {
      ++pulse_count_;
      pulse_label_->setText(QStringLiteral("脉冲: %1").arg(pulse_count_));
    }

    last_change_ts_ = sample.timestamp;
    ts_label_->setText(last_change_ts_.toString(QStringLiteral("HH:mm:ss.zzz")));

    previous_on_ = current_on;
    updateLED(current_on);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// updateLED — 更新 LED 外观和文本
// ══════════════════════════════════════════════════════════════════════════════

void StateLEDWidget::updateLED(bool on) {
  if (on) {
    led_label_->setProperty("ledState", QStringLiteral("on"));
    state_label_->setText(QStringLiteral("ON"));
  } else {
    led_label_->setProperty("ledState", QStringLiteral("off"));
    state_label_->setText(QStringLiteral("OFF"));
  }

  // 强制刷新 QSS（确保 property 变化生效）
  led_label_->style()->unpolish(led_label_);
  led_label_->style()->polish(led_label_);
}

// ══════════════════════════════════════════════════════════════════════════════
// clearData — 重置
// ══════════════════════════════════════════════════════════════════════════════

void StateLEDWidget::clearData() {
  previous_on_ = false;
  pulse_count_ = 0;
  last_change_ts_ = QDateTime();
  monitor_index_ = -1;
  channel_index_ = -1;

  updateLED(false);
  pulse_label_->setText(QStringLiteral("脉冲: 0"));
  ts_label_->setText(QString());
}

// ══════════════════════════════════════════════════════════════════════════════
// displayedSignals
// ══════════════════════════════════════════════════════════════════════════════

QList<QPair<int, int>> StateLEDWidget::displayedSignals() const {
  if (monitor_index_ >= 0 && channel_index_ >= 0) {
    return {{monitor_index_, channel_index_}};
  }
  return {};
}

}  // namespace etest::app
