#include "libui/dock_title_bar/DockTitleBar.h"

#include <QDockWidget>
#include <QHBoxLayout>
#include <QIcon>

#include "ThemeManager.h"

namespace etest::ui {

DockTitleBar::DockTitleBar(const QString& title,
                           QDockWidget* dockWidget,
                           QWidget* parent)
    : QWidget(parent), dock_widget_(dockWidget) {
  auto* lay = new QHBoxLayout(this);
  lay->setContentsMargins(12, 0, 4, 0);
  lay->setSpacing(0);

  title_label_ = new QLabel(title, this);
  title_label_->setObjectName(QStringLiteral("dockTitleBarLabel"));
  QFont f = title_label_->font();
  f.setPointSize(9);
  title_label_->setFont(f);
  lay->addWidget(title_label_);
  lay->addStretch();

  float_btn_ = new QToolButton(this);
  float_btn_->setObjectName(QStringLiteral("dockFloatButton"));
  float_btn_->setFixedSize(24, 24);
  float_btn_->setAutoRaise(true);
  float_btn_->setToolTip(QStringLiteral("浮动/停靠"));
  float_btn_->setCursor(Qt::ArrowCursor);
  connect(float_btn_, &QAbstractButton::clicked, this, [this]() {
    dock_widget_->setFloating(!dock_widget_->isFloating());
  });
  lay->addWidget(float_btn_);

  close_btn_ = new QToolButton(this);
  close_btn_->setObjectName(QStringLiteral("dockCloseButton"));
  close_btn_->setFixedSize(24, 24);
  close_btn_->setAutoRaise(true);
  close_btn_->setToolTip(QStringLiteral("关闭"));
  close_btn_->setCursor(Qt::ArrowCursor);
  connect(close_btn_, &QAbstractButton::clicked, dock_widget_, &QDockWidget::close);
  lay->addWidget(close_btn_);

  setFixedHeight(32);
  updateIcons();

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

  float_btn_->setIcon(
      QIcon(QStringLiteral(":/resources/icons/svg/dock_float%1.svg").arg(suffix)));
  float_btn_->setIconSize(QSize(20, 20));

  close_btn_->setIcon(
      QIcon(QStringLiteral(":/resources/icons/svg/dock_close%1.svg").arg(suffix)));
  close_btn_->setIconSize(QSize(20, 20));
}

}  // namespace etest::ui
