#include "WelcomeWidget.h"

#include <QStackedWidget>
#include <QVBoxLayout>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "logger/Logger.h"
#include "welcome/v1/WelcomeV1Widget.h"
#include "welcome/v2/WelcomeV2Widget.h"

namespace etest::app {

using namespace etest::core::config;

WelcomeWidget::WelcomeWidget(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

void WelcomeWidget::initUi() {
  stack_ = new QStackedWidget(this);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(stack_);

  v1_ = new WelcomeV1Widget(this);
  v2_ = new WelcomeV2Widget(this);
  stack_->addWidget(v1_);
  stack_->addWidget(v2_);

  auto& cfg = ConfigManager::instance();
  const QString version = cfg.get<QString>(
      CONFIG_WELCOME_VERSION, QString::fromLatin1(CONFIG_WELCOME_DEFAULT_VERSION));
  stack_->setCurrentIndex(version == QStringLiteral("v1") ? 0 : 1);
}

void WelcomeWidget::initSignals() {
  // v1 信号转发
  connect(v1_, &WelcomeV1Widget::newProjectRequested, this,
          &WelcomeWidget::newProjectRequested);
  connect(v1_, &WelcomeV1Widget::openProjectRequested, this,
          &WelcomeWidget::openProjectRequested);
  connect(v1_, &WelcomeV1Widget::projectOpenRequested, this,
          &WelcomeWidget::projectOpenRequested);
  // v2 信号转发
  connect(v2_, &WelcomeV2Widget::newProjectRequested, this,
          &WelcomeWidget::newProjectRequested);
  connect(v2_, &WelcomeV2Widget::openProjectRequested, this,
          &WelcomeWidget::openProjectRequested);
  connect(v2_, &WelcomeV2Widget::createFileRequested, this,
          &WelcomeWidget::createFileRequested);
  connect(v2_, &WelcomeV2Widget::projectOpenRequested, this,
          &WelcomeWidget::projectOpenRequested);
  connect(v2_, &WelcomeV2Widget::settingsRequested, this,
          &WelcomeWidget::settingsRequested);

  // 版本热切换：改 CONFIG_WELCOME_VERSION 立即生效
  auto& cfg = ConfigManager::instance();
  connect(&cfg, &ConfigManager::configChanged, this,
          [this](const QString& key) {
            if (key == QString::fromLatin1(CONFIG_WELCOME_VERSION)) {
              switchVersion();
            }
          });
}

void WelcomeWidget::refreshRecentProjects() {
  if (stack_ && stack_->currentWidget() == v1_) {
    v1_->refreshRecentProjects();
  } else if (v2_) {
    v2_->refreshRecentProjects();
  }
}

void WelcomeWidget::switchVersion() {
  auto& cfg = ConfigManager::instance();
  const QString version = cfg.get<QString>(
      CONFIG_WELCOME_VERSION, QString::fromLatin1(CONFIG_WELCOME_DEFAULT_VERSION));
  const int index = version == QStringLiteral("v1") ? 0 : 1;
  if (!stack_ || stack_->currentIndex() == index) {
    return;
  }
  LOG_INFO("WELCOME", "切换 Welcome 版本 [version={}]", version.toStdString());
  stack_->setCurrentIndex(index);
  // 新激活版本补一次刷新，保证最近项目数据新鲜
  if (stack_->currentWidget() == v1_) {
    v1_->refreshRecentProjects();
  } else if (v2_) {
    v2_->refreshRecentProjects();
  }
}

}  // namespace etest::app
