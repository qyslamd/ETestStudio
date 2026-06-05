#include "LoadingOverlay.h"

#include <QPainter>
#include <QResizeEvent>

#include "ThemeManager.h"

namespace etest::app {

LoadingOverlay::LoadingOverlay(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    raise();

    // 脉冲定时器
    pulse_timer_ = new QTimer(this);
    connect(pulse_timer_, &QTimer::timeout, this, &LoadingOverlay::onPulseTick);

    // 淡出定时器
    fade_timer_ = new QTimer(this);
    connect(fade_timer_, &QTimer::timeout, this, &LoadingOverlay::onFadeTick);
    fade_timer_->setInterval(16);  // ~60fps

    // 超时保护
    timeout_timer_ = new QTimer(this);
    timeout_timer_->setSingleShot(true);
    connect(timeout_timer_, &QTimer::timeout, this, [this]() {
        finish();
    });

    // 安装事件过滤器拦截交互
    if (parent) {
        parent->installEventFilter(this);
        // 同步父窗口大小
        setGeometry(parent->rect());
    }
}

LoadingOverlay::~LoadingOverlay() {
    if (parent()) {
        parent()->removeEventFilter(this);
    }
}

void LoadingOverlay::startWithTimeout(int timeoutMs) {
    pulse_phase_ = 0.0;
    pulse_alpha_ = 180;
    pulse_timer_->start(30);
    show();
    raise();
    timeout_timer_->start(timeoutMs);
}

void LoadingOverlay::finish() {
    pulse_timer_->stop();
    timeout_timer_->stop();
    // 从当前脉冲亮度开始淡出，避免亮度跳跃
    fade_alpha_ = pulse_alpha_;
    fade_timer_->start();
}

void LoadingOverlay::onPulseTick() {
    // 正弦呼吸：相位 0→2π 循环，周期约 4.2s
    constexpr double kTwoPi = 6.283185307179586;
    constexpr double kPhaseStep = 0.15;
    constexpr int kAlphaBase = 190;
    constexpr int kAlphaRange = 30;
    pulse_phase_ += kPhaseStep;
    if (pulse_phase_ > kTwoPi)
        pulse_phase_ -= kTwoPi;
    double val = std::sin(pulse_phase_);
    pulse_alpha_ = kAlphaBase + static_cast<int>(val * kAlphaRange);
    update();
}

void LoadingOverlay::onFadeTick() {
    constexpr int kFadeStep = 17;
    fade_alpha_ -= kFadeStep;
    if (fade_alpha_ <= 0) {
        fade_alpha_ = 0;
        fade_timer_->stop();
        hide();
        emit finished();
        deleteLater();
        return;
    }
    update();
}

void LoadingOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    bool dark = ThemeManager::instance().isDarkTheme();

    // 当前透明度 = 脉冲值（正常态）或 fade 值（淡出态）
    int alpha = fade_timer_->isActive() ? fade_alpha_ : pulse_alpha_;

    QColor bg = dark ? QColor(30, 30, 46) : QColor(245, 245, 245);
    bg.setAlpha(alpha);
    p.fillRect(rect(), bg);

    // 脉冲光圈（绘制一个半透明圆环）
    if (!fade_timer_->isActive() || fade_alpha_ > 60) {
        QColor circleColor = dark ? QColor(100, 140, 255) : QColor(0, 120, 215);
        circleColor.setAlpha(pulse_alpha_);

        QPointF center = rect().center();
        qreal radius = 30.0;
        // 外圈
        p.setBrush(Qt::NoBrush);
        QPen pen(circleColor, 3);
        p.setPen(pen);
        p.drawEllipse(center, radius, radius);

        // 内圈（透明度更高）
        circleColor.setAlpha(pulse_alpha_ - 60);
        pen.setColor(circleColor);
        pen.setWidth(2);
        p.setPen(pen);
        p.drawEllipse(center, radius * 0.6, radius * 0.6);

        // "正在加载..." 文字
        QColor textColor = dark ? Qt::white : Qt::black;
        textColor.setAlpha(
            fade_timer_->isActive()
                ? qMin(fade_alpha_, 255)
                : 255);
        p.setPen(textColor);
        QFont font = p.font();
        font.setPointSize(11);
        p.setFont(font);
        QRect textRect(0, (int)center.y() + 50, width(), 30);
        p.drawText(textRect, Qt::AlignCenter, QStringLiteral("正在加载..."));
    }
}

bool LoadingOverlay::eventFilter(QObject* obj, QEvent* event) {
    if (isVisible()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            return true;  // 拦截
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void LoadingOverlay::resizeEvent(QResizeEvent*) {
    if (auto* p = parentWidget()) {
        setGeometry(p->rect());
    }
    QWidget::resizeEvent(nullptr);
}

}  // namespace etest::app
