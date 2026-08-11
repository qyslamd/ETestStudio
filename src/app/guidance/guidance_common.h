#pragma once

#include <QColor>
#include <QGraphicsDropShadowEffect>

class QWidget;

namespace etest::app {

// 为 widget 统一添加阴影效果。
// 从 examples/guidance-demo 的 utility_qt 收窄迁移而来，仅保留 createGraphicsShadow。
QGraphicsDropShadowEffect* createGraphicsShadow(
    QWidget* widget,
    const QColor& color = QColor(105, 105, 105, 200),
    int blurRadius = 9);

}  // namespace etest::app
