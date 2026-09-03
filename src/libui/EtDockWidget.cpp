#include "EtDockWidget.h"
#include <QDockWidget>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QStyleOption>
#include "ThemeManager.h"

namespace etest::ui {

DockTitleBar::DockTitleBar(const QString& title,
                           QDockWidget* dockWidget,
                           QWidget* parent)
    : QFrame(parent), dock_widget_(dockWidget) {
  initUi(title);
  initSignals();
}

void DockTitleBar::initUi(const QString& title) {
  title_label_ = new QLabel(title, this);
  title_label_->setObjectName(QStringLiteral("dockTitleBarLabel"));
  QFont f = title_label_->font();
  f.setPointSize(9);
  title_label_->setFont(f);

  float_btn_ = new QToolButton(this);
  float_btn_->setObjectName(QStringLiteral("dockFloatButton"));
  float_btn_->setFixedSize(24, 24);
  float_btn_->setAutoRaise(true);
  float_btn_->setToolTip(QStringLiteral("浮动/停靠"));
  float_btn_->setCursor(Qt::ArrowCursor);

  close_btn_ = new QToolButton(this);
  close_btn_->setObjectName(QStringLiteral("dockCloseButton"));
  close_btn_->setFixedSize(24, 24);
  close_btn_->setAutoRaise(true);
  close_btn_->setToolTip(QStringLiteral("关闭"));
  close_btn_->setCursor(Qt::ArrowCursor);

  // setFixedHeight(32);
  auto* lay = new QHBoxLayout(this);
  lay->setContentsMargins(9, 2, 2, 2);
  lay->setSpacing(0);
  lay->addStretch(0);
  lay->addWidget(title_label_, 1);
  lay->addStretch(0);
  lay->addWidget(float_btn_, 0);
  lay->addWidget(close_btn_, 0);
  updateIcons();
}
void DockTitleBar::initSignals() {
  connect(float_btn_, &QAbstractButton::clicked, this,
          [this]() { dock_widget_->setFloating(!dock_widget_->isFloating()); });

  connect(close_btn_, &QAbstractButton::clicked, dock_widget_,
          &QDockWidget::close);

  // React to theme changes
  connect(&etest::core_ui::ThemeManager::instance(),
          &etest::core_ui::ThemeManager::themeChanged, this,
          [this](bool) { updateIcons(); });
}

void DockTitleBar::setTitle(const QString& title) {
  title_label_->setText(title);
}

void DockTitleBar::updateIcons() {
  bool dark = etest::core_ui::ThemeManager::instance().isDarkTheme();
  QString suffix = dark ? QStringLiteral("_dark") : QStringLiteral("_light");

  float_btn_->setIcon(QIcon(
      QStringLiteral(":/resources/icons/svg/dock_float%1.svg").arg(suffix)));
  float_btn_->setIconSize(QSize(20, 20));

  close_btn_->setIcon(QIcon(
      QStringLiteral(":/resources/icons/svg/dock_close%1.svg").arg(suffix)));
  close_btn_->setIconSize(QSize(20, 20));
}

void DockTitleBar::onDockWidgetFeaturesChanged(
    QDockWidget::DockWidgetFeatures features) {
  float_btn_->setVisible(features.testFlag(QDockWidget::DockWidgetFloatable));
  close_btn_->setVisible(features.testFlag(QDockWidget::DockWidgetClosable));
}

EtDockWidget::EtDockWidget(const QString& title, QWidget* parent)
    : QDockWidget(title, parent) {
  // 复用 DockTitleBar 作为标题栏（标题文字 + 浮动/关闭按钮）
  title_bar_ = new DockTitleBar(title, this, this);
  setTitleBarWidget(title_bar_);

  connect(this, &QDockWidget::featuresChanged, title_bar_,
          &DockTitleBar::onDockWidgetFeaturesChanged);
  setFeatures(QDockWidget::DockWidgetClosable);
}

EtDockWidget::~EtDockWidget() = default;

void EtDockWidget::setTitle(const QString& title) {
  setWindowTitle(title);
  if (title_bar_) {
    title_bar_->setTitle(title);
  }
}

}  // namespace etest::ui
