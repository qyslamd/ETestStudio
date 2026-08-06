#include "VisualizerPaletteWidget.h"

#include <QAbstractItemView>
#include <QDrag>
#include <QIcon>
#include <QMimeData>
#include <QPixmap>
#include <QStandardItem>
#include <QStandardItemModel>

#include "visualizer/visualizers/SignalVisualizer.h"
#include "visualizer/visualizers/VisualizerFactory.h"

namespace etest::app {

namespace {
// 与 VisualizationArea 拖放 mime 一致（payload = displayMode 字符串）
const char kVisualizerMime[] = "application/x-etest-visualizer";
const int kDisplayModeRole = Qt::UserRole + 1;
const char* const kDisplayModes[] = {"waveform", "led", "meter", "gauge", "frame"};
const char* const kDisplayNames[] = {"波形", "LED", "数字表", "指针表", "帧数据"};

// 渲染 visualizer 真实空态实例为缩略图（决策：缩略图/真实渲染图）
QPixmap renderThumbnail(const QString& displayMode, const QSize& size) {
  auto* vis = etest::visualizer::createVisualizerFor(
      QString(), displayMode, QString(), QString(), nullptr);
  if (!vis) {
    return QPixmap();
  }
  vis->resize(size);
  vis->setAttribute(Qt::WA_DontShowOnScreen, true);
  QPixmap pm(size);
  pm.fill(Qt::transparent);
  vis->render(&pm);
  delete vis;
  return pm;
}
}  // namespace

VisualizerPaletteWidget::VisualizerPaletteWidget(QWidget* parent)
    : QListView(parent) {
  model_ = new QStandardItemModel(this);
  setModel(model_);
  setViewMode(QListView::ListMode);
  setDragEnabled(true);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setEditTriggers(QAbstractItemView::NoEditTriggers);

  for (int i = 0; i < 5; ++i) {
    auto* item = new QStandardItem(QString::fromUtf8(kDisplayNames[i]));
    item->setData(QString::fromLatin1(kDisplayModes[i]), kDisplayModeRole);
    item->setIcon(
        QIcon(renderThumbnail(QString::fromLatin1(kDisplayModes[i]),
                              QSize(48, 32))));
    model_->appendRow(item);
  }
}

void VisualizerPaletteWidget::setIconMode(bool icon_mode) {
  setViewMode(icon_mode ? QListView::IconMode : QListView::ListMode);
  // IconMode 下调整 icon 尺寸，网格更清晰
  setIconSize(QSize(48, 32));
}

void VisualizerPaletteWidget::startDrag(Qt::DropActions supportedActions) {
  const QModelIndex idx = currentIndex();
  if (!idx.isValid()) {
    return;
  }
  const QString displayMode = idx.data(kDisplayModeRole).toString();
  if (displayMode.isEmpty()) {
    return;
  }
  auto* mime = new QMimeData;
  mime->setData(QString::fromLatin1(kVisualizerMime), displayMode.toUtf8());
  auto* drag = new QDrag(this);
  drag->setMimeData(mime);
  const QIcon icon = idx.data(Qt::DecorationRole).value<QIcon>();
  if (!icon.isNull()) {
    drag->setPixmap(icon.pixmap(48, 32));
  }
  drag->exec(supportedActions);
}

}  // namespace etest::app
