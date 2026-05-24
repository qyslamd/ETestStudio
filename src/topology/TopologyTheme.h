#pragma once

#include <QColor>

namespace etest::topology {

struct TopologyColors {
  // Scene
  QColor sceneBackground;
  // UUT (product)
  QColor uutFill;
  QColor uutBorder;
  // Device
  QColor deviceFill;
  QColor deviceBorder;
  // Monitor
  QColor monitorFill;
  QColor monitorBorder;
  // Connection
  QColor connectionLine;
  QColor connectionHover;
  QColor connectionSelected;
  // Port direction
  QColor directionInput;
  QColor directionOutput;
  QColor directionBidirectional;
  // Shadow
  QColor shadowDark;
  QColor shadowLight;
  // Text
  QColor textPrimary;
  QColor textSecondary;
  // Legend
  QColor legendBackground;
  QColor legendText;
  QColor legendBorder;
  // Resize handle
  QColor resizeHandleFill;
  QColor resizeHandleBorder;
};

const TopologyColors& topologyColors();

}  // namespace etest::topology
