#pragma once

#include <QString>

class QGraphicsScene;

namespace etest::topology {

/// 将拓扑场景渲染输出为不同格式的文件
/// 支持：PNG（位图）、SVG（矢量图）、PDF（矢量文档）
/// 格式由文件扩展名自动识别（.png / .svg / .pdf）
bool renderSceneToFile(QGraphicsScene* scene, const QString& filePath);

}  // namespace etest::topology
