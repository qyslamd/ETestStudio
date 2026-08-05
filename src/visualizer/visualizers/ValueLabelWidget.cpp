#include "ValueLabelWidget.h"

#include <QVBoxLayout>

namespace etest::visualizer {

ValueLabelWidget::ValueLabelWidget(const QString& title, QWidget* parent)
    : SignalVisualizer(parent), title_(title) {
  initUi();
  title_label_->setText(title_);
  setSubtitle(QString());  // 默认隐藏副标题
}

void ValueLabelWidget::setTitle(const QString& title) {
  title_label_->setText(title);
}

void ValueLabelWidget::setSubtitle(const QString& subtitle) {
  if (subtitle.isEmpty()) {
    subtitle_label_->hide();
  } else {
    subtitle_label_->setText(subtitle);
    subtitle_label_->show();
  }
}

void ValueLabelWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(4);

  // 标题（两级：主标题 + 副标题连接描述，决策 14；样式在 QSS 中通过 #ValueLabelTitle 控制）
  title_label_ = new QLabel(title_, this);
  title_label_->setObjectName(QStringLiteral("ValueLabelTitle"));
  layout->addWidget(title_label_);

  subtitle_label_ = new QLabel(this);
  subtitle_label_->setObjectName(QStringLiteral("ValueLabelSubtitle"));
  subtitle_label_->setWordWrap(true);
  layout->addWidget(subtitle_label_);

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
  connection_id_ = sample.connectionId;

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
  connection_id_.clear();
}

QList<QString> ValueLabelWidget::displayedSignals() const {
  if (!connection_id_.isEmpty()) {
    return {connection_id_};
  }
  return {};
}

}  // namespace etest::visualizer
