#include "window_mover.h"
#include <QMouseEvent>
#include <QObject>

WindowMover::WindowMover(QWidget *target, QObject *parent)
    : QObject(parent), m_target(target) {
  target->installEventFilter(this);
}

bool WindowMover::eventFilter(QObject *obj, QEvent *event) {
  if (obj == m_target) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

    switch (event->type()) {
    case QEvent::MouseButtonPress:
      if (mouseEvent->button() == Qt::LeftButton) {
        m_dragging = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_dragPosition = mouseEvent->globalPosition().toPoint() -
                         m_target->frameGeometry().topLeft();
#else
        m_dragPosition =
            mouseEvent->globalPos() - m_target->frameGeometry().topLeft();
#endif
        return true;
      }
      break;

    case QEvent::MouseMove:
      if (m_dragging && (mouseEvent->buttons() & Qt::LeftButton)) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QPoint newPos = mouseEvent->globalPosition().toPoint() - m_dragPosition;
#else
        QPoint newPos = mouseEvent->globalPos() - m_dragPosition;
#endif
        if (auto* parent = m_target->parentWidget()) {
          int maxX = parent->width() - m_target->width();
          int maxY = parent->height() - m_target->height();
          newPos.setX(qBound(0, newPos.x(), maxX));
          newPos.setY(qBound(0, newPos.y(), maxY));
        }
        m_target->move(newPos);
        return true;
      }
      break;

    case QEvent::MouseButtonRelease:
      if (mouseEvent->button() == Qt::LeftButton) {
        m_dragging = false;
        return true;
      }
      break;

    default:
      break;
    }
  }
  return QObject::eventFilter(obj, event);
}
