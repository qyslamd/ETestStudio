#define _USE_MATH_DEFINES
#include <cmath>

#include "TopologyPathRouter.h"

namespace etest::topology {

QPainterPath TopologyPathRouter::route(const Context& ctx) const {
  switch (ctx.style) {
    case PathStyle::Bezier:
      return routeBezier(ctx.sourcePos, ctx.targetPos);
    case PathStyle::Polyline:
      return routePolyline(ctx.sourcePos, ctx.targetPos, ctx.obstacles);
    case PathStyle::Straight:
      return routeStraight(ctx.sourcePos, ctx.targetPos);
  }
  return routeBezier(ctx.sourcePos, ctx.targetPos);
}

QPainterPath TopologyPathRouter::routeBezier(const QPointF& src,
                                              const QPointF& dst) const {
  QPainterPath p;
  p.moveTo(src);

  qreal dx = qAbs(dst.x() - src.x());
  qreal cpOffset = qMax(dx * 0.5, 50.0);
  QPointF cp1, cp2;
  if (dst.x() > src.x()) {
    cp1 = src + QPointF(cpOffset, 0);
    cp2 = dst - QPointF(cpOffset, 0);
  } else {
    cp1 = src - QPointF(cpOffset, 0);
    cp2 = dst + QPointF(cpOffset, 0);
  }
  p.cubicTo(cp1, cp2, dst);
  return p;
}

QPainterPath TopologyPathRouter::routeStraight(const QPointF& src,
                                                const QPointF& dst) const {
  QPainterPath p;
  p.moveTo(src);
  p.lineTo(dst);
  return p;
}

QPainterPath TopologyPathRouter::routePolyline(
    const QPointF& src,
    const QPointF& dst,
    const QVector<QRectF>& obstacles) const {
  QPainterPath path;
  path.moveTo(src);

  constexpr qreal kTurning = 15.0;

  // Find an x-position for the vertical segment that avoids obstacles.
  // Start with the midpoint then adjust if blocked.
  qreal midX = (src.x() + dst.x()) / 2.0;
  bool needDetour = false;
  qreal detourY = 0;

  // The potential vertical segment at midX between src.y and dst.y
  qreal segMinY = qMin(src.y(), dst.y());
  qreal segMaxY = qMax(src.y(), dst.y());
  QRectF vertSegment(midX - 4, segMinY - 4, 8, segMaxY - segMinY + 8);

  for (const auto& obs : obstacles) {
    if (!vertSegment.intersects(obs))
      continue;

    // This obstacle blocks the direct vertical segment — detour around it.
    needDetour = true;
    qreal spaceAbove = obs.top() - segMinY;
    qreal spaceBelow = segMaxY - obs.bottom();

    // Use whichever side has more clearance.
    // When multiple obstacles are present, accumulate the detour offset.
    if (spaceAbove >= spaceBelow && spaceAbove > 0) {
      qreal candidate = obs.top() - kTurning;
      if (detourY == 0 || candidate < detourY)
        detourY = candidate;
    } else if (spaceBelow > 0) {
      qreal candidate = obs.bottom() + kTurning;
      if (detourY == 0 || candidate > detourY)
        detourY = candidate;
    }
  }

  if (needDetour) {
    // Z-shaped detour: src → midX → detourY → dst.x → dst
    path.lineTo(midX, src.y());
    path.lineTo(midX, detourY);
    path.lineTo(dst.x(), detourY);
  } else {
    // Simple L-shaped: src → midX → midX,dst.y → dst
    path.lineTo(midX, src.y());
    path.lineTo(midX, dst.y());
  }
  path.lineTo(dst);

  return path;
}

}  // namespace etest::topology
