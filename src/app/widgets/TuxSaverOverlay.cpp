#include "TuxSaverOverlay.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "SaverWidgetBase.h"
#include "TuxSaverWidget.h"
#include "WisdomWidget.h"

TuxSaverOverlay::TuxSaverOverlay(QWidget* parent)
    : QWidget(parent) {
  setVisible(false);

  initModes();

  // ── Close button ──
  close_btn_ = new QPushButton(QStringLiteral("✕"), this);
  close_btn_->setFixedSize(32, 32);
  close_btn_->setObjectName(QStringLiteral("saverCloseBtn"));

  connect(close_btn_, &QPushButton::clicked, this, [this]() {
    deactivate();
    emit closed();
  });

  // ── Mode switch buttons ──
  prev_btn_ = new QPushButton(QStringLiteral("◀"), this);
  prev_btn_->setFixedSize(28, 28);
  prev_btn_->setObjectName(QStringLiteral("saverPrevBtn"));
  prev_btn_->setToolTip(QStringLiteral("上一个屏保模式"));

  next_btn_ = new QPushButton(QStringLiteral("▶"), this);
  next_btn_->setFixedSize(28, 28);
  next_btn_->setObjectName(QStringLiteral("saverNextBtn"));
  next_btn_->setToolTip(QStringLiteral("下一个屏保模式"));

  connect(prev_btn_, &QPushButton::clicked, this, [this]() { switchMode(-1); });
  connect(next_btn_, &QPushButton::clicked, this, [this]() { switchMode(1); });
}

void TuxSaverOverlay::initModes() {
  auto* tux = new TuxSaverWidget(this);
  tux->setIdleThreshold(0);
  modes_.append(tux);

  modes_.append(new WisdomWidget(this));

  currentMode_ = 0;
  saver_ = modes_[0];
}

void TuxSaverOverlay::switchMode(int direction) {
  if (modes_.isEmpty()) return;

  // Hide old
  saver_->onDeactivate();
  saver_->hide();

  // Switch
  currentMode_ = (currentMode_ + direction + modes_.size()) % modes_.size();
  saver_ = modes_[currentMode_];

  // Show new
  saver_->setGeometry(rect());
  saver_->show();
  saver_->raise();
  saver_->onActivate();
}

void TuxSaverOverlay::activate() {
  auto* p = parentWidget();
  if (!p) return;
  background_ = p->grab();
  setGeometry(p->rect());
  saver_->setGeometry(rect());
  saver_->onActivate();
  repositionButtons();
  show();
  raise();
  saver_->raise();
  close_btn_->raise();
  prev_btn_->raise();
  next_btn_->raise();
}

void TuxSaverOverlay::deactivate() {
  saver_->onDeactivate();
  hide();
  background_ = QPixmap();
}

void TuxSaverOverlay::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  repositionButtons();
  saver_->setGeometry(rect());
}

void TuxSaverOverlay::repositionButtons() {
  const int margin = 12;
  int right = width() - margin;

  close_btn_->move(right - close_btn_->width(), margin);
  right -= close_btn_->width() + 8;
  next_btn_->move(right - next_btn_->width(), margin + 2);
  right -= next_btn_->width() + 4;
  prev_btn_->move(right - prev_btn_->width(), margin + 2);
}

void TuxSaverOverlay::paintEvent(QPaintEvent*) {
  QPainter p(this);
  if (!background_.isNull()) {
    p.drawPixmap(rect(), background_, background_.rect());
    p.fillRect(rect(), QColor(0, 0, 0, 100));
  } else {
    p.fillRect(rect(), QColor(0x18, 0x18, 0x1E));
  }
}
