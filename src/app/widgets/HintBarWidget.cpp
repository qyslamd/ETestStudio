#include "HintBarWidget.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace etest::app {

HintBarWidget::HintBarWidget(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("HintBar"));
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
}

void HintBarWidget::postHint(const QString& text,
                              const QString& actionLabel,
                              std::function<void()> action) {
  HintData data{text, actionLabel, std::move(action)};
  if (active_hints_.size() < kMaxVisible) {
    auto* item = createHintItem(data);
    active_hints_.append({data, item});
    rebuild();
  } else {
    pending_queue_.enqueue(std::move(data));
  }
}

void HintBarWidget::clearAll() {
  pending_queue_.clear();
  for (auto& entry : active_hints_) {
    entry.container->deleteLater();
  }
  active_hints_.clear();
  rebuild();
}

QWidget* HintBarWidget::createHintItem(const HintData& data) {
  static constexpr int kMaxTextWidth = 260;

  auto* container = new QWidget(this);
  container->setObjectName(QStringLiteral("HintItem"));
  container->setFixedHeight(kItemHeight);

  auto* layout = new QHBoxLayout(container);
  layout->setContentsMargins(8, 0, 4, 0);
  layout->setSpacing(6);

  auto* indicator = new QLabel(QStringLiteral("●"), container);
  indicator->setObjectName(QStringLiteral("HintIndicator"));
  indicator->setFixedWidth(14);

  auto* textLabel = new QLabel(container);
  textLabel->setObjectName(QStringLiteral("HintText"));

  QFontMetrics fm(textLabel->font());
  QString elided =
      fm.elidedText(data.text, Qt::ElideRight, kMaxTextWidth);
  textLabel->setText(elided);
  textLabel->setFixedHeight(kItemHeight);

  QToolButton* actionBtn = nullptr;
  if (data.action) {
    actionBtn = new QToolButton(container);
    actionBtn->setText(data.actionLabel.isEmpty()
                           ? QStringLiteral("操作")
                           : data.actionLabel);
    actionBtn->setObjectName(QStringLiteral("HintActionBtn"));
    actionBtn->setFixedSize(40, 20);
    actionBtn->setCursor(Qt::PointingHandCursor);
    connect(actionBtn, &QAbstractButton::clicked, this,
            [action = data.action] { action(); });
  }

  auto* closeBtn = new QToolButton(container);
  closeBtn->setText(QStringLiteral("✕"));
  closeBtn->setObjectName(QStringLiteral("HintCloseBtn"));
  closeBtn->setFixedSize(20, 20);
  closeBtn->setCursor(Qt::PointingHandCursor);

  connect(closeBtn, &QAbstractButton::clicked, this, [this, container]() {
    for (int i = 0; i < active_hints_.size(); ++i) {
      if (active_hints_[i].container == container) {
        dismissHint(i);
        break;
      }
    }
  });

  layout->addWidget(indicator);
  layout->addWidget(textLabel, 1);
  if (data.action) {
    layout->addWidget(actionBtn);
  }
  layout->addWidget(closeBtn);

  return container;
}

void HintBarWidget::dismissHint(int index) {
  active_hints_[index].container->deleteLater();
  active_hints_.removeAt(index);

  if (!pending_queue_.isEmpty()) {
    HintData data = pending_queue_.dequeue();
    auto* item = createHintItem(data);
    active_hints_.append({data, item});
  }

  rebuild();
}

void HintBarWidget::rebuild() {
  auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
  if (!layout) return;

  while (layout->count() > 0) {
    layout->removeItem(layout->itemAt(0));
  }

  for (auto& entry : active_hints_) {
    layout->addWidget(entry.container);
  }

  if (!active_hints_.isEmpty()) {
    layout->addStretch();
  }

  setVisible(!active_hints_.isEmpty());
}

}  // namespace etest::app
