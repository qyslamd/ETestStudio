#pragma once

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QVector>

namespace etest::topology {

enum class PathStyle { Bezier, Polyline, Straight };

/// Multi-style path routing engine for connection lines.
/// Supports Bezier, Polyline (with obstacle avoidance), and Straight styles.
class TopologyPathRouter {
 public:
  struct Context {
    QPointF sourcePos;
    QPointF targetPos;
    PathStyle style = PathStyle::Bezier;
    QVector<QRectF> obstacles;
  };

  QPainterPath route(const Context& ctx) const;

 private:
  QPainterPath routeBezier(const QPointF& src, const QPointF& dst) const;
  QPainterPath routePolyline(const QPointF& src, const QPointF& dst,
                              const QVector<QRectF>& obstacles) const;
  QPainterPath routeStraight(const QPointF& src, const QPointF& dst) const;
};

}  // namespace etest::topology
