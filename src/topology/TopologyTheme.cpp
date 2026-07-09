#include "TopologyTheme.h"

#include <QApplication>
#include <QWidget>

#include "ThemeManager.h"

namespace etest::topology {

const TopologyColors& topologyColors() {
  static TopologyColors light;
  static TopologyColors dark;
  static bool initialized = false;

  if (!initialized) {
    // ── Light ────────────────────────────────────────────────────
    light.sceneBackground = QColor(250, 250, 250);
    light.uutFill = QColor(189, 215, 238);
    light.uutBorder = QColor(66, 133, 244);
    light.deviceFill = QColor(255, 228, 181);
    light.deviceBorder = QColor(230, 145, 56);
    light.monitorFill = QColor(232, 224, 240);
    light.monitorBorder = QColor(155, 110, 234);
    light.connectionLine = QColor(80, 80, 80);
    light.connectionHover = QColor(41, 98, 255);
    light.connectionSelected = QColor(220, 80, 0);
    light.directionInput = QColor(66, 133, 244);
    light.directionOutput = QColor(52, 168, 83);
    light.directionBidirectional = QColor(255, 0, 255);
    light.shadowDark = QColor(0, 0, 0, 40);
    light.shadowLight = QColor(0, 0, 0, 20);
    light.textPrimary = Qt::black;
    light.textSecondary = QColor(100, 100, 100);
    light.legendBackground = QColor(255, 255, 255, 200);
    light.legendText = Qt::black;
    light.legendBorder = Qt::gray;
    light.resizeHandleFill = QColor(255, 255, 255);
    light.resizeHandleBorder = QColor(66, 133, 244);

    // ── Dark (VS Code-inspired) ──────────────────────────────────
    dark.sceneBackground = QColor(30, 30, 30);
    dark.uutFill = QColor(26, 58, 92);
    dark.uutBorder = QColor(86, 156, 214);
    dark.deviceFill = QColor(60, 42, 28);
    dark.deviceBorder = QColor(215, 186, 125);
    dark.monitorFill = QColor(61, 46, 92);
    dark.monitorBorder = QColor(197, 134, 192);
    dark.connectionLine = QColor(136, 136, 136);
    dark.connectionHover = QColor(86, 156, 214);
    dark.connectionSelected = QColor(244, 135, 113);
    dark.directionInput = QColor(86, 156, 214);
    dark.directionOutput = QColor(78, 201, 176);
    dark.directionBidirectional = QColor(197, 134, 192);
    dark.shadowDark = QColor(0, 0, 0, 80);
    dark.shadowLight = QColor(0, 0, 0, 40);
    dark.textPrimary = QColor(204, 204, 204);
    dark.textSecondary = QColor(136, 136, 136);
    dark.legendBackground = QColor(51, 51, 51, 220);
    dark.legendText = QColor(204, 204, 204);
    dark.legendBorder = QColor(85, 85, 85);
    dark.resizeHandleFill = QColor(60, 60, 60);
    dark.resizeHandleBorder = QColor(86, 156, 214);

    initialized = true;
  }

  return etest::core_ui::ThemeManager::instance().isDarkTheme() ? dark : light;
}

}  // namespace etest::topology
