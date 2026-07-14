#ifndef ETEST_APP_TUXSAVERCONTROLLER_H_
#define ETEST_APP_TUXSAVERCONTROLLER_H_

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include "widgets/TuxSaverOverlay.h"

namespace etest::app {

class TuxSaverController : public QObject {
  Q_OBJECT
 public:
  explicit TuxSaverController(QWidget* parent_widget,
                              QObject* parent = nullptr);

  void start();
  void stop();

  // 由 MainWindow::eventFilter 调用——用户活动时重置计时器
  void onUserActivity();

 signals:
  void saverActivated();
  void saverDeactivated();

 private:
  void showSaver();
  void hideSaver();

  QWidget* parent_widget_;
  TuxSaverOverlay* overlay_ = nullptr;
  QElapsedTimer idle_timer_;
  QTimer* check_timer_ = nullptr;
  int idle_timeout_ms_ = 60000;
};

}  // namespace etest::app

#endif  // ETEST_APP_TUXSAVERCONTROLLER_H_
