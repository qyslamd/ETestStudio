#include "StartupSplashWidget.h"

#include <QFrame>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QScreen>
#include <QVBoxLayout>

namespace etest::app {

StartupSplashWidget::StartupSplashWidget(QWidget* parent) : QWidget(parent) {
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setFixedSize(480, 320);
  setObjectName(QStringLiteral("StartupSplash"));

  // 超时兜底：lazyInit 挂起超时后强制退出（替代旧 LoadingOverlay 的 10s 兜底）
  timeout_timer_ = new QTimer(this);
  timeout_timer_->setSingleShot(true);
  connect(timeout_timer_, &QTimer::timeout, this, [this]() {
    if (isVisible()) {
      hide();
      emit timeout();
    }
  });

  initUi();
}

void StartupSplashWidget::initUi() {
  // 内容容器：圆角 + 背景色由 startup.qss 绘制，替代 C++ 硬编码样式
  auto* content = new QFrame(this);
  content->setObjectName(QStringLiteral("StartupSplashContent"));

  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(content);

  auto* layout = new QVBoxLayout(content);
  layout->setContentsMargins(32, 28, 32, 24);
  layout->setSpacing(10);

  // logo（直接 QIcon 加载，禁用 AppIconProvider —— 其 resolvePath
  // 只查 svg/ 子目录，加载不到根目录图标）
  logo_label_ = new QLabel(content);
  logo_label_->setObjectName(QStringLiteral("StartupSplashLogo"));
  logo_label_->setAlignment(Qt::AlignCenter);
  logo_label_->setFixedSize(96, 96);
  logo_label_->setPixmap(
      QIcon(QStringLiteral(":/resources/icons/app_icon.svg"))
          .pixmap(96, 96));
  layout->addWidget(logo_label_, 0, Qt::AlignHCenter);

  title_label_ = new QLabel(QStringLiteral("ETestStudio"), content);
  title_label_->setObjectName(QStringLiteral("StartupSplashTitle"));
  title_label_->setAlignment(Qt::AlignCenter);
  layout->addWidget(title_label_);

  status_label_ = new QLabel(QStringLiteral("正在启动..."), content);
  status_label_->setObjectName(QStringLiteral("StartupSplashStatus"));
  status_label_->setAlignment(Qt::AlignCenter);
  status_label_->setWordWrap(true);
  layout->addWidget(status_label_);

  progress_bar_ = new QProgressBar(content);
  progress_bar_->setObjectName(QStringLiteral("StartupSplashProgressBar"));
  progress_bar_->setRange(0, 100);
  progress_bar_->setValue(0);
  progress_bar_->setTextVisible(true);
  progress_bar_->setFormat(QStringLiteral("%p%"));
  progress_bar_->setFixedHeight(16);
  layout->addWidget(progress_bar_);
}

void StartupSplashWidget::setStatusText(const QString& text) {
  status_label_->setText(text);
}

void StartupSplashWidget::setProgress(int percent) {
  progress_bar_->setValue(percent);
}

void StartupSplashWidget::finish() {
  timeout_timer_->stop();
  hide();
}

void StartupSplashWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  // 居中于主屏
  if (QScreen* screen = QGuiApplication::primaryScreen()) {
    const QRect available = screen->availableGeometry();
    move(available.center() - rect().center());
  }
  // 启动超时兜底（旧 LoadingOverlay 等价 10s；期间 processEvents 会给定时器机会）
  timeout_timer_->start(10000);
}

void StartupSplashWidget::mousePressEvent(QMouseEvent* event) {
  event->accept();
}

void StartupSplashWidget::keyPressEvent(QKeyEvent* event) {
  event->accept();
}

}  // namespace etest::app
