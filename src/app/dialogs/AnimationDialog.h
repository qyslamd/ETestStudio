#ifndef ETEST_APP_DIALOGS_ANIMATIONDIALOG_H_
#define ETEST_APP_DIALOGS_ANIMATIONDIALOG_H_

#include <QDialog>
#include <functional>

class WindowMover;

namespace etest::app {

class AnimationDialog : public QDialog {
  Q_OBJECT

 public:
  explicit AnimationDialog(QWidget* parent = nullptr);
  ~AnimationDialog() override;

  void removeWindowMover();

 signals:
  void hideAnimationFinished();

 protected:
  void setWidget(QWidget* widget);
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void keyPressEvent(QKeyEvent* e) override;
  virtual void actShowAnimation();

 protected slots:
  void actHideAnimation();
  void actHideAnimation(std::function<void()> func);

 protected:
  QWidget* widget_ = nullptr;
  WindowMover* mover_ = nullptr;
  int round_radius_ = 8;
};

}  // namespace etest::app

#endif  // ETEST_APP_DIALOGS_ANIMATIONDIALOG_H_
