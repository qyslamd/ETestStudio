#include "ImageViewerWidget.h"

#include <QFile>
#include <QFileInfo>
#include <QMouseEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "ThemeManager.h"

namespace etest::app {

// ── Constructor ──────────────────────────────────────────────────

ImageViewerWidget::ImageViewerWidget(const QString& filePath, QWidget* parent)
    : QWidget(parent), file_path_(filePath) {
  setupUi();
  loadFile();

  // Theme change: refresh background brush
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this](bool) {
            view_->setBackgroundBrush(ThemeManager::instance().isDarkTheme()
                                          ? QColor(60, 60, 60)
                                          : QColor(210, 210, 210));
          });
}

// ── IEditor interface ────────────────────────────────────────────

QString ImageViewerWidget::displayName() const {
  return QFileInfo(file_path_).fileName();
}

bool ImageViewerWidget::isModified() const { return false; }

bool ImageViewerWidget::save() { return false; }

bool ImageViewerWidget::saveAs(const QString& /*path*/) { return false; }

QString ImageViewerWidget::filePath() const { return file_path_; }

QString ImageViewerWidget::editorId() const { return file_path_; }

QWidget* ImageViewerWidget::widget() { return this; }

QString ImageViewerWidget::editorType() const { return QStringLiteral("image"); }

QObject* ImageViewerWidget::signalObject() { return this; }

bool ImageViewerWidget::canUndo() const { return false; }
bool ImageViewerWidget::canRedo() const { return false; }
void ImageViewerWidget::undo() {}
void ImageViewerWidget::redo() {}

// ── UI setup ─────────────────────────────────────────────────────

void ImageViewerWidget::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  scene_ = new QGraphicsScene(this);
  view_ = new QGraphicsView(scene_, this);
  view_->setRenderHint(QPainter::Antialiasing);
  view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  view_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  view_->setFrameShape(QFrame::NoFrame);
  view_->setBackgroundBrush(ThemeManager::instance().isDarkTheme()
                                ? QColor(60, 60, 60)
                                : QColor(210, 210, 210));
  layout->addWidget(view_);
}

// ── Image loading ────────────────────────────────────────────────

void ImageViewerWidget::loadFile() {
  QFile file(file_path_);
  if (!file.open(QIODevice::ReadOnly))
    return;

  QByteArray data = file.readAll();
  file.close();

  QPixmap pixmap;
  if (!pixmap.loadFromData(data))
    return;

  if (pixmap_item_) {
    scene_->removeItem(pixmap_item_);
    delete pixmap_item_;
  }

  pixmap_item_ = scene_->addPixmap(pixmap);
  scene_->setSceneRect(pixmap.rect());
}

// ── Fit to window (on first show, after layout is complete) ──────

void ImageViewerWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (!fit_done_ && pixmap_item_) {
    fitToWindow();
    fit_done_ = true;
  }
}

// ── Fit to window ────────────────────────────────────────────────

void ImageViewerWidget::fitToWindow() {
  if (!pixmap_item_)
    return;
  view_->fitInView(pixmap_item_, Qt::KeepAspectRatio);
  current_zoom_ = view_->transform().m11();
}

// ── Zoom ─────────────────────────────────────────────────────────

void ImageViewerWidget::wheelEvent(QWheelEvent* event) {
  if (!(event->modifiers() & Qt::ControlModifier)) {
    QWidget::wheelEvent(event);
    return;
  }

  qreal factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
  qreal newZoom = current_zoom_ * factor;
  if (newZoom >= 0.1 && newZoom <= 10.0) {
    current_zoom_ = newZoom;
    view_->scale(factor, factor);
  }
  event->accept();
}

// ── Pan ──────────────────────────────────────────────────────────

void ImageViewerWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton) {
    panning_ = true;
    last_pan_point_ = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void ImageViewerWidget::mouseMoveEvent(QMouseEvent* event) {
  if (panning_) {
    QPoint delta = event->pos() - last_pan_point_;
    last_pan_point_ = event->pos();
    view_->horizontalScrollBar()->setValue(
        view_->horizontalScrollBar()->value() - delta.x());
    view_->verticalScrollBar()->setValue(
        view_->verticalScrollBar()->value() - delta.y());
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void ImageViewerWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton) {
    panning_ = false;
    setCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

}  // namespace etest::app
