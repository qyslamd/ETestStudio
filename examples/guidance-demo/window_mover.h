#pragma once
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
