#ifndef ETEST_APP_GRID_GRID_ANIMATOR_H_
#define ETEST_APP_GRID_GRID_ANIMATOR_H_

#include <QList>
#include <QParallelAnimationGroup>
#include <QPoint>
#include <QObject>
#include <tuple>

class QWidget;

namespace etest::app::grid {

class GridAnimator : public QParallelAnimationGroup {
  Q_OBJECT
 public:
  explicit GridAnimator(QObject* parent = nullptr);
  void executeAnimation(const QList<std::tuple<QWidget*, QPoint, QPoint>>& list);
};

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_GRID_ANIMATOR_H_
