#include "grid_animator.h"

#include <QPropertyAnimation>
#include <QWidget>

namespace etest::app::grid {

GridAnimator::GridAnimator(QObject* parent)
    : QParallelAnimationGroup(parent) {
  setLoopCount(1);
  connect(this, &QAnimationGroup::finished, this, &QAnimationGroup::clear);
}

void GridAnimator::executeAnimation(
    const QList<std::tuple<QWidget*, QPoint, QPoint>>& list) {
  if (list.isEmpty()) return;
  if (state() == QAbstractAnimation::Running) return;
  for (auto item : list) {
    auto obj = std::get<0>(item);
    auto pos1 = std::get<1>(item);
    auto pos2 = std::get<2>(item);
    auto animation = new QPropertyAnimation(obj, "pos", this);
    animation->setKeyValueAt(0, pos1);
    animation->setKeyValueAt(1, pos2);
    animation->setDuration(300);
    addAnimation(animation);
  }
  start();
}

}  // namespace etest::app::grid
