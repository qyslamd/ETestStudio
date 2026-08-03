#include "StateLEDWidget.h"

#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>
#include <QVariant>

#include "engine/MonitorManager.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造
// ══════════════════════════════════════════════════════════════════════════════

StateLEDWidget::StateLEDWidget(const QString& title, QWidget* parent)
    : SignalVisualizer(parent), title_(title) {
  initUi();
  title_label_->setText(title_);
  setSubtitle(QString());  // 默认隐藏副标题
}

void StateLEDWidget::setTitle(const QString& title) {
  title_label_->setText(title);
}

void StateLEDWidget::setSubtitle(const QString& subtitle) {
  if (subtitle.isEmpty()) {
    subtitle_label_->hide();
  } else {
    subtitle_label_->setText(subtitle);
    subtitle_label_->show();
  }
}

void StateLEDWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(4);

  // 标题（两级：主标题 + 副标题连接描述，决策 14）
  title_label_ = new QLabel(title_, this);
  title_label_->setObjectName(QStringLiteral("LEDTitle"));
  layout->addWidget(title_label_);

  subtitle_label_ = new QLabel(this);
  subtitle_label_->setObjectName(QStringLiteral("LEDSubtitle"));
  subtitle_label_->setWordWrap(true);
  layout->addWidget(subtitle_label_);

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

  setObjectName(QStringLiteral("LEDWidget"));
  setAutoFillBackground(true);

  // 初始：OFF（红色）
  updateLED(false);
}

// ══════════════════════════════════════════════════════════════════════════════
// onSampleCaptured — 更新 LED 状态
// ══════════════════════════════════════════════════════════════════════════════

void StateLEDWidget::onSampleCaptured(
    const etest::engine::MonitorSample& sample) {
  connection_id_ = sample.connectionId;

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
  connection_id_.clear();

  updateLED(false);
  pulse_label_->setText(QStringLiteral("脉冲: 0"));
  ts_label_->setText(QString());
}

// ══════════════════════════════════════════════════════════════════════════════
// displayedSignals
// ══════════════════════════════════════════════════════════════════════════════

QList<QString> StateLEDWidget::displayedSignals() const {
  if (!connection_id_.isEmpty()) {
    return {connection_id_};
  }
  return {};
}

}  // namespace etest::app
