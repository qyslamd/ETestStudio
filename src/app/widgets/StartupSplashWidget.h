#pragma once

#include <QTimer>
#include <QWidget>

class QLabel;
class QProgressBar;

namespace etest::app {

// 启动 Splash 屏幕：覆盖 main() 重初始化、MainWindow 构造与 lazyInit 全部
// 启动阶段，提供 logo + 状态文本 + 进度条（百分比）。无边框置顶窗口，
// 主窗口在 lazyInit 完成后由 MainWindow::revealAfterSplash() 统一显示，
// 本组件负责隐藏自身。
// 样式走独立 startup.qss（主题加载前无法使用主题 QSS），由调用方在 main()
// 中通过 setStyleSheet 加载资源。
class StartupSplashWidget : public QWidget {
  Q_OBJECT

 public:
  explicit StartupSplashWidget(QWidget* parent = nullptr);

  // 更新当前步骤状态文本
  void setStatusText(const QString& text);
  // 更新进度条，percent 范围 0-100
  void setProgress(int percent);
  // 隐藏 splash（主窗口显示由 MainWindow 控制，次序见方案 finish() 节）
  void finish();

 signals:
  // 超时兜底：lazyInit 挂起超时后发出，调用方应强制 reveal 主窗口
  void timeout();

 protected:
  void showEvent(QShowEvent* event) override;
  // 拦截鼠标/键盘，防止穿透到尚未初始化的主窗口
  void mousePressEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void initUi();

  QLabel* logo_label_ = nullptr;
  QLabel* title_label_ = nullptr;
  QLabel* status_label_ = nullptr;
  QProgressBar* progress_bar_ = nullptr;
  QTimer* timeout_timer_ = nullptr;  // 启动超时兜底
};

}  // namespace etest::app
