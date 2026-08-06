#pragma once

#include <QListView>

class QStandardItemModel;

namespace etest::runconfig {

// VisualizerPaletteWidget — 可视化组件选择面板（拖放源）
// QListView 列 5 种 visualizer（ListMode 列表 / IconMode 网格可切），
// item 存 displayMode（UserRole）+ 真实渲染缩略图；拖出时 mime 携带
// displayMode（application/x-etest-visualizer），由可视化区 drop 接收。
class VisualizerPaletteWidget : public QListView {
  Q_OBJECT

 public:
  explicit VisualizerPaletteWidget(QWidget* parent = nullptr);

  // 切换列表/网格视图（IconMode = 网格）
  void setIconMode(bool icon_mode);

 protected:
  void startDrag(Qt::DropActions supportedActions) override;

 private:
  QStandardItemModel* model_ = nullptr;
};

}  // namespace etest::runconfig
