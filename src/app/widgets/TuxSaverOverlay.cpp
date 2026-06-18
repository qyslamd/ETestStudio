#include "TuxSaverOverlay.h"

#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "SaverWidgetBase.h"
#include "TuxSaverWidget.h"

TuxSaverOverlay::TuxSaverOverlay(QWidget* parent)
    : QWidget(parent) {
  setVisible(false);

  // ── TuxSaverWidget ──
  saver_ = new TuxSaverWidget(this);
  saver_->setIdleThreshold(0);

  // ── Close button ──
  close_btn_ = new QPushButton(QStringLiteral("✕"), this);
  close_btn_->setFixedSize(32, 32);
  close_btn_->setStyleSheet(
      QStringLiteral("QPushButton {"
                     "  color: rgba(255,255,255,160);"
                     "  background: rgba(255,255,255,25);"
                     "  border: 1px solid rgba(255,255,255,40);"
                     "  font-size: 16px;"
                     "  border-radius: 16px;"
                     "}"
                     "QPushButton:hover {"
                     "  background: rgba(255,80,80,130);"
                     "  color: white;"
                     "  border-color: rgba(255,80,80,200);"
                     "}"));

  connect(close_btn_, &QPushButton::clicked, this, [this]() {
    deactivate();
    emit closed();
  });
}

void TuxSaverOverlay::activate() {
  auto* p = parentWidget();
  if (!p) return;
  // 截取当前 MainWindow 画面作为背景
  background_ = p->grab();
  setGeometry(p->rect());
  saver_->setGeometry(rect());
  saver_->onActivate();
  repositionCloseButton();
  show();
  raise();
  saver_->raise();
  close_btn_->raise();
}

void TuxSaverOverlay::deactivate() {
  saver_->onDeactivate();
  hide();
  background_ = QPixmap();
}

void TuxSaverOverlay::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  repositionCloseButton();
  saver_->setGeometry(rect());
}

void TuxSaverOverlay::repositionCloseButton() {
  close_btn_->move(width() - close_btn_->width() - 12, 12);
}

void TuxSaverOverlay::paintEvent(QPaintEvent*) {
  QPainter p(this);
  if (!background_.isNull()) {
    p.drawPixmap(rect(), background_, background_.rect());
    // 半透明暗色遮罩，让企鹅更突出
    p.fillRect(rect(), QColor(0, 0, 0, 100));
  } else {
    p.fillRect(rect(), QColor(0x18, 0x18, 0x1E));
  }
}
