#include "TuxSaverController.h"

#include <QCoreApplication>
#include <QWidget>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

using namespace etest::core::config;

namespace etest::app {

TuxSaverController::TuxSaverController(QWidget* parent_widget,
                                       QObject* parent)
    : QObject(parent),
      parent_widget_(parent_widget) {}

void TuxSaverController::start() {
  overlay_ = new TuxSaverOverlay(parent_widget_);
  connect(overlay_, &TuxSaverOverlay::closed, this, [this]() {
    idle_timer_.restart();
  });
  idle_timer_.start();
  check_timer_ = new QTimer(this);
  connect(check_timer_, &QTimer::timeout, this, [this]() {
    if (!overlay_->isVisible() &&
        ConfigManager::instance().get<bool>(CONFIG_TUXSAVER_ENABLED,
                                            CONFIG_TUXSAVER_DEFAULT_ENABLED)) {
      int timeoutMs =
          ConfigManager::instance().get<int>(CONFIG_TUXSAVER_IDLE_TIMEOUT,
                                             CONFIG_TUXSAVER_DEFAULT_TIMEOUT) *
          1000;
      if (idle_timer_.elapsed() > timeoutMs) {
        overlay_->activate();
      }
    }
  });
  check_timer_->start(1000);
}

void TuxSaverController::stop() {
  if (overlay_ && overlay_->isVisible()) {
    overlay_->deactivate();
  }
  if (check_timer_) {
    check_timer_->stop();
  }
}

void TuxSaverController::onUserActivity() {
  idle_timer_.restart();
  if (overlay_ && overlay_->isVisible() &&
      !ConfigManager::instance().get<bool>(
          CONFIG_TUXSAVER_ENABLED, CONFIG_TUXSAVER_DEFAULT_ENABLED)) {
    overlay_->deactivate();
  }
}

}  // namespace etest::app
