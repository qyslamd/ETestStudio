#include "DigitalMeterWidget.h"

#include <QVBoxLayout>

#include "engine/MonitorManager.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造
// ══════════════════════════════════════════════════════════════════════════════

DigitalMeterWidget::DigitalMeterWidget(const QString& title, QWidget* parent)
    : SignalVisualizer(parent), title_(title) {
  initUi();
}

void DigitalMeterWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(2);

  // 标题
  title_label_ = new QLabel(title_, this);
  title_label_->setObjectName(QStringLiteral("MeterTitle"));
  layout->addWidget(title_label_);

  // 工程值（大字体）
  value_label_ = new QLabel(QStringLiteral("--"), this);
  value_label_->setObjectName(QStringLiteral("MeterValue"));
  value_label_->setAlignment(Qt::AlignCenter);
  layout->addWidget(value_label_);

  // 趋势箭头
  trend_label_ = new QLabel(QStringLiteral("→"), this);  // →
  trend_label_->setObjectName(QStringLiteral("MeterTrend"));
  trend_label_->setAlignment(Qt::AlignCenter);
  layout->addWidget(trend_label_);

  // 范围（min / max）
  range_label_ = new QLabel(QStringLiteral("min: --  max: --"), this);
  range_label_->setObjectName(QStringLiteral("MeterRange"));
  layout->addWidget(range_label_);

  // 原始值
  raw_label_ = new QLabel(QStringLiteral("原始: --"), this);
  raw_label_->setObjectName(QStringLiteral("MeterRaw"));
  layout->addWidget(raw_label_);

  // 时间戳
  ts_label_ = new QLabel(this);
  ts_label_->setObjectName(QStringLiteral("MeterTs"));
  layout->addWidget(ts_label_);

  layout->addStretch();

  setObjectName(QStringLiteral("MeterWidget"));
  setAutoFillBackground(true);
}

// ══════════════════════════════════════════════════════════════════════════════
// onSampleCaptured — 更新数值显示
// ══════════════════════════════════════════════════════════════════════════════

void DigitalMeterWidget::onSampleCaptured(
    const etest::engine::MonitorSample& sample) {
  monitor_index_ = sample.monitorIndex;
  previous_value_ = current_value_;
  current_value_ = sample.engValue;

  if (!has_data_) {
    // 首次数据：初始化 min/max
    min_value_ = current_value_;
    max_value_ = current_value_;
  } else {
    // 更新 min/max
    if (current_value_ < min_value_) {
      min_value_ = current_value_;
    }
    if (current_value_ > max_value_) {
      max_value_ = current_value_;
    }
  }

  // 时间戳
  ts_label_->setText(sample.timestamp.toString(QStringLiteral("HH:mm:ss.zzz")));

  // 原始值
  if (sample.rawFrame.isEmpty()) {
    raw_label_->setText(QStringLiteral("原始: %1").arg(sample.rawValue, 0, 'f', 2));
  } else {
    raw_label_->setText(
        QStringLiteral("原始: 0x%1")
            .arg(QString::fromLatin1(sample.rawFrame.toHex().toUpper())));
  }

  updateDisplay();
  has_data_ = true;
}

// ══════════════════════════════════════════════════════════════════════════════
// updateDisplay — 刷新工程值、趋势、范围
// ══════════════════════════════════════════════════════════════════════════════

void DigitalMeterWidget::updateDisplay() {
  // 工程值
  value_label_->setText(QStringLiteral("%1").arg(current_value_, 0, 'f', 3));

  // 趋势箭头
  if (!has_data_ || qAbs(current_value_ - previous_value_) < kEpsilon) {
    trend_label_->setText(QStringLiteral("→"));  // →
  } else if (current_value_ > previous_value_) {
    trend_label_->setText(QStringLiteral("↑"));  // ↑
  } else {
    trend_label_->setText(QStringLiteral("↓"));  // ↓
  }

  // 范围
  if (has_data_) {
    range_label_->setText(
        QStringLiteral("min: %1  max: %2")
            .arg(min_value_, 0, 'f', 3)
            .arg(max_value_, 0, 'f', 3));
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// clearData
// ══════════════════════════════════════════════════════════════════════════════

void DigitalMeterWidget::clearData() {
  value_label_->setText(QStringLiteral("--"));
  trend_label_->setText(QStringLiteral("→"));
  range_label_->setText(QStringLiteral("min: --  max: --"));
  raw_label_->setText(QStringLiteral("原始: --"));
  ts_label_->setText(QString());

  monitor_index_ = -1;
  current_value_ = 0.0;
  previous_value_ = 0.0;
  min_value_ = 0.0;
  max_value_ = 0.0;
  has_data_ = false;
}

// ══════════════════════════════════════════════════════════════════════════════
// displayedSignals
// ══════════════════════════════════════════════════════════════════════════════

QList<int> DigitalMeterWidget::displayedSignals() const {
  if (monitor_index_ >= 0) {
    return {monitor_index_};
  }
  return {};
}

}  // namespace etest::app
