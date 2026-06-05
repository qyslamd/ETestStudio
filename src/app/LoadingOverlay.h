#pragma once

#include <QTimer>
#include <QWidget>

namespace etest::app {

/// 主窗口懒加载期间的脉冲覆盖层
/// 盖在 v_splitter_ 内容区上，Ribbon 和活动栏保持可见
class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget* parent);
    ~LoadingOverlay() override;

    /// 开始脉冲动效并设置 10 秒超时
    void startWithTimeout(int timeoutMs = 10000);

    /// 淡出动画，结束后 deleteLater
    void finish();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void onPulseTick();
    void onFadeTick();

    int pulse_alpha_ = 180;          // 当前脉冲透明度
    double pulse_phase_ = 0.0;       // 正弦相位
    QTimer* pulse_timer_ = nullptr;   // 脉冲驱动

    int fade_alpha_ = 255;           // 淡出递减
    QTimer* fade_timer_ = nullptr;    // 淡出驱动
    QTimer* timeout_timer_ = nullptr; // 超时保护
};

}  // namespace etest::app
