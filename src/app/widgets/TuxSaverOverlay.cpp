#include "TuxSaverOverlay.h"

#include <QPainter>
#include <QToolButton>

#include "AppIconProvider.h"
#include "ConfigDefs.h"
#include "SaverWidgetBase.h"
#include "TuxSaverWidget.h"
#include "WisdomWidget.h"
#include "config/ConfigManager.h"

using namespace etest::core::config;
using namespace etest::app;

TuxSaverOverlay::TuxSaverOverlay(QWidget* parent) : QWidget(parent) {
  setVisible(false);

  last_mode_ = saverModeFromConfig();
  initSaver();

  // ── Close button ──
  close_btn_ = new QToolButton(this);
  close_btn_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("close")));
  close_btn_->setIconSize(QSize(16, 16));
  close_btn_->setFixedSize(32, 32);
  close_btn_->setObjectName(QStringLiteral("saverCloseBtn"));
  close_btn_->setCursor(Qt::PointingHandCursor);
  close_btn_->setToolTip(QStringLiteral("关闭屏保"));

  connect(close_btn_, &QAbstractButton::clicked, this, [this]() {
    deactivate();
    emit closed();
  });
}

QString TuxSaverOverlay::saverModeFromConfig() const {
  return ConfigManager::instance().get<QString>(
      CONFIG_TUXSAVER_MODE, QString::fromLatin1(CONFIG_TUXSAVER_DEFAULT_MODE));
}

void TuxSaverOverlay::initSaver() {
  delete saver_;
  saver_ = nullptr;

  if (last_mode_ == QStringLiteral("wisdom")) {
    saver_ = new WisdomWidget(this);
  } else {
    auto* tux = new TuxSaverWidget(this);
    tux->setIdleThreshold(0);
    saver_ = tux;
  }
  saver_->hide();
}

void TuxSaverOverlay::activate() {
  // Recreate saver only if the mode changed in settings
  QString mode = saverModeFromConfig();
  if (mode != last_mode_ || !saver_) {
    last_mode_ = mode;
    initSaver();
  }

  auto* p = parentWidget();
  if (!p)
    return;
  background_ = p->grab();
  setGeometry(p->rect());
  saver_->setGeometry(rect());
  saver_->show();
  saver_->onActivate();
  close_btn_->move(width() - close_btn_->width() - 12, 12);
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
  if (saver_)
    saver_->setGeometry(rect());
  close_btn_->move(width() - close_btn_->width() - 12, 12);
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
