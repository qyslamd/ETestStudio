#include "ValueLabelWidget.h"

#include <QVBoxLayout>

namespace etest::app {

ValueLabelWidget::ValueLabelWidget(const QString& title, QWidget* parent)
    : SignalVisualizer(parent), title_(title) {
  initUi();
}

void ValueLabelWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(4);

  // 标题（样式在 QSS 中通过 #ValueLabelTitle 控制）
  auto* title_label = new QLabel(title_, this);
  title_label->setObjectName(QStringLiteral("ValueLabelTitle"));
  layout->addWidget(title_label);

  // 工程值（大字体，样式在 QSS 中通过 #ValueLabelValue 控制）
  value_label_ = new QLabel(QStringLiteral("--"), this);
  value_label_->setObjectName(QStringLiteral("ValueLabelValue"));
  value_label_->setAlignment(Qt::AlignCenter);
  layout->addWidget(value_label_);

  // 原始值（样式在 QSS 中通过 #ValueLabelRaw 控制）
  raw_label_ = new QLabel(QStringLiteral("原始: --"), this);
  raw_label_->setObjectName(QStringLiteral("ValueLabelRaw"));
  layout->addWidget(raw_label_);

  // 时间戳（样式在 QSS 中通过 #ValueLabelTs 控制）
  ts_label_ = new QLabel(QString(), this);
  ts_label_->setObjectName(QStringLiteral("ValueLabelTs"));
  layout->addWidget(ts_label_);

  layout->addStretch();

  setObjectName(QStringLiteral("ValueLabelWidget"));
  setAutoFillBackground(true);
}

void ValueLabelWidget::onSampleCaptured(const etest::engine::MonitorSample& sample) {
  monitor_index_ = sample.monitorIndex;

  // 工程值
  value_label_->setText(QStringLiteral("%1").arg(sample.engValue, 0, 'f', 3));

  // 原始值（优先显示 rawValue，无则显示 rawFrame hex）
  if (sample.rawFrame.isEmpty()) {
    raw_label_->setText(
        QStringLiteral("原始: %1").arg(sample.rawValue, 0, 'f', 2));
  } else {
    raw_label_->setText(
        QStringLiteral("原始: 0x%1")
            .arg(QString::fromLatin1(sample.rawFrame.toHex().toUpper())));
  }

  // 时间戳
  ts_label_->setText(sample.timestamp.toString(QStringLiteral("HH:mm:ss.zzz")));
}

void ValueLabelWidget::clearData() {
  value_label_->setText(QStringLiteral("--"));
  raw_label_->setText(QStringLiteral("原始: --"));
  ts_label_->setText(QString());
  monitor_index_ = -1;
}

QList<int> ValueLabelWidget::displayedSignals() const {
  if (monitor_index_ >= 0) {
    return {monitor_index_};
  }
  return {};
}

}  // namespace etest::app
