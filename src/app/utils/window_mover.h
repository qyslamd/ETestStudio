#ifndef ETEST_APP_UTILS_WINDOW_MOVER_H_
#define ETEST_APP_UTILS_WINDOW_MOVER_H_

#include <QWidget>

class WindowMover : public QObject {
  Q_OBJECT
 public:
  WindowMover(QWidget* target, QObject* parent = nullptr);

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  QWidget* m_target;
  QPoint m_dragPosition;
  bool m_dragging = false;
};

#endif  // ETEST_APP_UTILS_WINDOW_MOVER_H_
