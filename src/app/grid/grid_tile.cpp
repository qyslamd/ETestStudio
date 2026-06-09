#include "grid_tile.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QHoverEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "gradientpainter.h"
#include "grid_gradient_data.h"
#include "grid_global_def.hpp"
#include "grid_tile_span.h"

namespace etest::app::grid {

GridTile::GridTile(TileSpan type, QWidget* parent)
    : QWidget(parent), type_(type) {
  initUi();
}

void GridTile::setContentWidget(QWidget* widget) {
  if (content_widget_) {
    if (layout_) {
      if (auto item = layout_->takeAt(0)) {
        content_widget_->deleteLater();
      }
    }
  }
  content_widget_ = widget;
  if (layout_) {
    layout_->insertWidget(0, widget);
  }
}

void GridTile::setNameText(const QString& name) {
  if (name_label_) {
    name_label_->setText(name);
    name_label_->setVisible(!name_label_->text().isEmpty());
  }
}

QString GridTile::nameText() const {
  return name_label_ ? name_label_->text() : QString();
}

void GridTile::setDragingState(bool draging) {
  draging_ = draging;
  if (content_widget_)
    content_widget_->setVisible(!draging_);
  if (name_label_)
    name_label_->setVisible(!draging_);
}

void GridTile::posResetAnimation(const QRect& boundary,
                                 const QPoint& pos1,
                                 const QPoint& pos2) {
  if (!pos_anime_) {
    pos_anime_ = new QPropertyAnimation(this, "pos", this);
    pos_anime_->setLoopCount(1);
    pos_anime_->setEasingCurve(QEasingCurve::OutInQuad);
    pos_anime_->setDuration(300);
  }

  enum class TriState { Horizontal, Center, Vertival };
  static auto calcAnotherPos = [](const QRect& b, const QPoint& pos,
                                  TriState state) -> QPoint {
    auto center = b.center();
    QPoint target;
    switch (state) {
      case TriState::Horizontal:
        target.rx() = pos.x() > center.x() ? pos.x() - center.x() / 2
                                           : pos.x() + center.x() / 2;
        target.ry() = pos.y();
        break;
      case TriState::Center:
        target.rx() = pos.x() > center.x() ? pos.x() - center.x() / 2
                                           : pos.x() + center.x() / 2;
        target.ry() = pos.y() > center.y() ? pos.y() - center.y() / 2
                                           : pos.y() + center.y() / 2;
        break;
      case TriState::Vertival:
        target.rx() = pos.x();
        target.ry() = pos.y() > center.y() ? pos.y() - center.y() / 2
                                           : pos.y() + center.y() / 2;
        break;
    }
    return target;
  };

  this->raise();
  pos_anime_->setStartValue(pos1);
  pos_anime_->setKeyValueAt(
      0.25, calcAnotherPos(boundary, pos1, TriState::Horizontal));
  pos_anime_->setKeyValueAt(0.5,
                            calcAnotherPos(boundary, pos2, TriState::Center));
  pos_anime_->setKeyValueAt(0.75,
                            calcAnotherPos(boundary, pos2, TriState::Vertival));
  pos_anime_->setEndValue(pos2);
  pos_anime_->start();
}

QPoint GridTile::dragUsedPos() const {
  if (content_widget_)
    return content_widget_->mapToParent(content_widget_->rect().topLeft());
  return QPoint();
}

bool GridTile::event(QEvent* event) {
  if (event->type() == QEvent::HoverEnter) {
    doMouseHoverEnterLeave(true);
  } else if (event->type() == QEvent::HoverLeave) {
    doMouseHoverEnterLeave(false);
  }
  return QWidget::event(event);
}

void GridTile::paintEvent(QPaintEvent* event) {
  QWidget::paintEvent(event);
  if (draging_)
    return;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  // 绘制完整磁贴圆角背景（覆盖全区域，包括 name_label 区域）
  QRect r = rect().adjusted(1, 1, -1, -1);
  QPainterPath path;
  path.addRoundedRect(r, Radius, Radius);

  // 仅在矩形变化时重新计算渐变
  QRectF rF(r);
  if (cached_gradient_rect_ != rF) {
    const auto& entry = kCuratedGradients[gradient_index_];
    QVector<GradientColorStop> stops;
    qreal angle;
    if (GradientPainter::ParseCssGradient(entry.css_code, angle, stops)) {
      for (auto& stop : stops) {
        stop.color.setAlphaF(stop.color.alphaF() * 0.6f);
      }
      cached_gradient_ =
          GradientPainter::CreateLinearGradient(rF, stops, angle);
      cached_gradient_rect_ = rF;
    }
  }
  p.fillPath(path, cached_gradient_);
}

void GridTile::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  calcFixedSize();
}

void GridTile::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    emit clicked();
  }
}

void GridTile::contextMenuEvent(QContextMenuEvent* event) {
  emit contextMenuRequested(event->globalPos(), type_);
}

void GridTile::initUi() {
  setAttribute(Qt::WA_Hover, true);
  setAttribute(Qt::WA_TranslucentBackground, true);

  // 从策划的渐变色库中随机选取一个
  gradient_index_ = QRandomGenerator::global()->bounded(kGradientCount);

  layout_ = new QVBoxLayout(this);
  layout_->setSpacing(4);
  layout_->setContentsMargins(0, 5, 0, 0);

  content_widget_ = new QWidget(this);
  content_widget_->setObjectName("gridTileContent");
  name_label_ = new QLabel(this);
  name_label_->setObjectName("gridTileName");
  name_label_->setAlignment(Qt::AlignCenter);

  layout_->addWidget(content_widget_);
  layout_->addWidget(name_label_);

  if (name_label_->text().isEmpty()) {
    name_label_->hide();
  }
}

void GridTile::calcFixedSize() {
  auto list = decima2HexStringList(type_);
  if (list.count() != 2)
    return;

  int a = list.at(1).toInt();
  int b = list.at(0).toInt();

  auto w = Width * a + (a - 1) * HSpacing;
  auto h = Height * b + (b - 1) * VSpacing;
  setFixedSize(w, h);

  if (content_widget_) {
    content_widget_->setFixedSize(w, h - 30);
  }
}

void GridTile::doMouseHoverEnterLeave(bool enter) {
  if (!shake_anime_) {
    shake_anime_ = new QPropertyAnimation(content_widget_, "pos", this);
    shake_anime_->setLoopCount(1);
    shake_anime_->setDuration(50);
  }

  const int off = 4;
  auto pos = this->rect().topLeft() + QPoint(0, off + this->layout_->margin());
  if (enter) {
    shake_anime_->setStartValue(pos);
    shake_anime_->setEndValue(pos + QPoint(0, -off));
  } else {
    shake_anime_->setStartValue(pos);
    shake_anime_->setEndValue(pos + QPoint(0, off));
  }
  shake_anime_->start();
}

}  // namespace etest::app::grid
