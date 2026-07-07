#pragma once

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "api/IEditor.h"

namespace etest::app {

class ImageViewerWidget : public QWidget, public IEditor {
  Q_OBJECT

 public:
  explicit ImageViewerWidget(const QString& filePath,
                             QWidget* parent = nullptr);

  // IEditor interface
  QString displayName() const override;
  bool isModified() const override;
  bool save() override;
  bool saveAs(const QString& path) override;
  QString filePath() const override;
  QString editorId() const override;
  QWidget* widget() override;
  QString editorType() const override;
  QObject* signalObject() override;

  // Undo/Redo
  bool canUndo() const override;
  bool canRedo() const override;
  void undo() override;
  void redo() override;

 private:
  void initUi();
  void loadFile();
  void fitToWindow();

  QString file_path_;
  QGraphicsScene* scene_;
  QGraphicsView* view_;
  QGraphicsPixmapItem* pixmap_item_ = nullptr;
  qreal current_zoom_ = 1.0;
  bool panning_ = false;
  QPoint last_pan_point_;
  bool fit_done_ = false;

 protected:
  void showEvent(QShowEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
};

}  // namespace etest::app
