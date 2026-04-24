#include "guidance_presentation.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtDebug>
#include "guidance_config.h"

GuidancePresentation::GuidancePresentation(QWidget* parent)
    : QWidget(parent),
      bubble_widget_(new QWidget(this)),
      title_label_(new QLabel(bubble_widget_)),
      countdown_label_(new QLabel(bubble_widget_)),
      text_label_(new QLabel(bubble_widget_)),
      skip_btn_(new QPushButton("跳过", bubble_widget_)),
      prev_btn_(new QPushButton("上一步", bubble_widget_)),
      next_btn_(new QPushButton("下一步", bubble_widget_)),
      count_down_timer_(new QTimer(this)),
      pixmap_smile_(":/resrouces/icons/grinning_squinting_face_256px.png") {
  initUi();
  initOthers();
  initSignals();
}

void GuidancePresentation::setTitle(const QString& title) {
  if (title_ != title) {
    title_ = title;
    title_label_->setText(title);
  }
}

void GuidancePresentation::setText(const QString& text) {
  if (text_ != text) {
    text_ = text;
    text_label_->setText(text);
  }
}

bool GuidancePresentation::eventFilter(QObject* obj, QEvent* event) {
  if (obj == bubble_widget_ && event->type() == QEvent::Paint) {
    QPainter p(bubble_widget_);
    p.setRenderHint(QPainter::Antialiasing);
    drawBubble(&p);
    drawCountDownOnBubble(&p);
    return true;
  }
  return QWidget::eventFilter(obj, event);
}

void GuidancePresentation::initUi() {
  bubble_widget_->setObjectName("bubble_self");
  bubble_widget_->setStyleSheet(
      "QWidget#bubble_self{background: transparent;}");
  //  bubble_->setStyleSheet("QWidget#bubble_self{border:1px solid red;}");
  bubble_widget_->setMinimumWidth(300);
  bubble_widget_->setMinimumHeight(200);

  QVBoxLayout* mainLayout = new QVBoxLayout(bubble_widget_);
  mainLayout->setContentsMargins(kLayoutMargin, kLayoutMargin, kLayoutMargin,
                                 kLayoutMargin);
  mainLayout->setSpacing(8);
  countdown_label_->setAlignment(Qt::AlignRight);
  countdown_label_->setObjectName("bubble_countdown");
  countdown_label_->setStyleSheet(
      "QLabel#bubble_countdown { background: transparent; font-size: 16px; "
      "font-weight: bold; color: #CDCDCD; }");
  countdown_label_->hide();

  title_label_->setAlignment(Qt::AlignCenter);
  title_label_->setObjectName("bubble_title");
  title_label_->setStyleSheet(
      "QLabel#bubble_title { background: transparent; font-size: 16px; "
      "font-weight: bold; color: "
      "#333333; }");

  text_label_->setObjectName("bubble_text");
  text_label_->setStyleSheet(
      "QLabel#bubble_text { background: transparent; font-size: 14px; color: "
      "#666666; }");
  text_label_->setWordWrap(true);

  QHBoxLayout* horizontalLayout = new QHBoxLayout();
  horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
  horizontalLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  skip_btn_->setObjectName("skip_btn");
  skip_btn_->setStyleSheet(
      "QPushButton#skip_btn { background: transparent; border: none; color: "
      "#999999; "
      "padding: 8px; font-size: 14px; } QPushButton#skip_btn:hover { color: "
      "#666666; }");

  horizontalLayout->addWidget(skip_btn_);

  prev_btn_->setObjectName("prev_btn");
  prev_btn_->setStyleSheet(
      "QPushButton#prev_btn { background: transparent; border: 1px solid "
      "#4CAF50; color: #4CAF50; padding: 8px 16px; border-radius: 4px; "
      "font-size: 14px; } "
      "QPushButton#prev_btn:hover { background: rgba(76, 175, 80, 0.1); }"
      "QPushButton#prev_btn:disabled{background:#adb5bd;}");
  horizontalLayout->addWidget(prev_btn_);

  next_btn_->setObjectName("next_btn");
  next_btn_->setStyleSheet(
      "QPushButton#next_btn { background: #4CAF50; border: none; color: white; "
      "padding: 8px 24px; border-radius: 4px; font-size: 14px; } "
      "QPushButton#next_btn:hover { background: #45a049; }"
      "QPushButton#next_btn:disabled{background:#adb5bd;}");

  horizontalLayout->addWidget(next_btn_);

  mainLayout->addWidget(countdown_label_);
  mainLayout->addWidget(title_label_);
  mainLayout->addWidget(text_label_);
  mainLayout->addLayout(horizontalLayout);
}

void GuidancePresentation::initOthers() {
  bubble_widget_->installEventFilter(this);

  anime_highlight_ = new QVariantAnimation(this);
  anime_highlight_->setDuration(kAnimeDur);
  anime_highlight_->setEasingCurve(QEasingCurve::InOutCubic);
  connect(anime_highlight_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& var) {
            anime_highlight_rect_ = var.value<QRect>();
            update();
          });
  connect(anime_highlight_, &QVariantAnimation::finished, this,
          [this] { update(); });

  anime_bubble_ = new QPropertyAnimation(this);
  anime_bubble_->setTargetObject(bubble_widget_);
  anime_bubble_->setPropertyName("pos");
  anime_bubble_->setDuration(kAnimeDur);
}

void GuidancePresentation::initSignals() {
  connect(skip_btn_, &QPushButton::clicked, this, [this] {
    count_down_timer_->stop();
    emit skipClicked();
  });
  connect(prev_btn_, &QPushButton::clicked, this, [this] {
    count_down_timer_->stop();
    emit prevClicked();
  });
  connect(next_btn_, &QPushButton::clicked, this, [this] {
    count_down_timer_->stop();
    emit nextClicked();
  });

  connect(count_down_timer_, &QTimer::timeout, this, [this] {
    static constexpr int sb = 3000 / 30;

    static int Count = 0;
    if (++Count >= sb) {
      time_percent_ = 1.0;
      count_down_timer_->stop();
      Count = 0;  // 恢复默认值
      emit nextClicked();
    } else {
      time_percent_ = (qreal)Count / (qreal)sb;

      int seconds = 5 * (1 - time_percent_);
      countdown_label_->setText(QString::number(seconds));
      bubble_widget_->update();
    }
  });
}

void GuidancePresentation::updateUi(GuidanceFlow* const flow,
                                    GuidanceStep* const step,
                                    bool canPrev,
                                    bool canNext) {
  //-------------------------------
  // 气泡
  auto index = flow->steps().indexOf(step) + 1;
  setTitle(QString("%1 (%2/%3)")
               .arg(flow->name())
               .arg(index)
               .arg(flow->stepCount()));
  setText(QString("%1.%2").arg(index).arg(step->description()));
  prev_btn_->setEnabled(canPrev);
  next_btn_->setText(canNext ? "下一步" : "完成！");

  //--------------------------------
  // 高亮区域
  pixmap_ = QPixmap();

  if (use_anime_) {
    // 计算之前设置开始值
    anime_highlight_->setStartValue(highlightRect_);
  } else {
    highlightRect_ = QRect();
  }

  std::visit(
      [this](auto&& target) {
        using T = std::decay_t<decltype(target)>;
        if constexpr (std::is_same_v<T, QWidget*>) {
          if (target) {
            QPoint gPos = target->mapToGlobal(QPoint(0, 0));
            highlightRect_ = QRect(mapFromGlobal(gPos), target->size());
          }
        } else if constexpr (std::is_same_v<T, QRect>) {
          // 得从全局映射坐标系来
          // 且必须假定传入得是全局坐标系
          highlightRect_ = QRect(mapFromGlobal(target.topLeft()),
                                 QSize(target.width(), target.height()));

        } else if constexpr (std::is_same_v<T, QPixmap>) {
          auto center = rect().center();
          highlightRect_ = QRect(center.x() - target.width() / 2,
                                 center.y() - target.height() / 2,
                                 target.width(), target.height());
          pixmap_ = target;
        }
      },
      step->target());

  if (use_anime_) {
    anime_highlight_->setEndValue(highlightRect_);
    anime_highlight_->start();
  } else {
    update();
  }

  //-------------------------------
  // 气泡位置
  changeBubblePosition();

  // 自动挡
  if (auto_mode_) {
    time_percent_ = 1.0;
    count_down_timer_->start(30);
  }
}

void GuidancePresentation::setIsAutoMode(bool autoMode) {
  auto_mode_ = autoMode;
  countdown_label_->setVisible(auto_mode_);
}

void GuidancePresentation::drawCountDownOnBubble(QPainter* painter) {
  if (!auto_mode_) {
    return;
  }

  auto& p = *painter;

  int margin = kLayoutMargin - 9;
  QRectF rect = bubble_widget_->rect().marginsRemoved(
      QMargins(margin, margin, margin, margin));

  rect.setWidth(rect.width() * (1 - time_percent_));

  QPainterPath path;
  path.addRoundedRect(rect, cornerRadius_, cornerRadius_);
  p.fillPath(path, QBrush(QColor(212, 252, 121, 245)));
}

void GuidancePresentation::drawBubble(QPainter* painter) {
  auto& p = *painter;

  int margin = kLayoutMargin - 9;
  QRectF bubbleRect = bubble_widget_->rect().marginsRemoved(
      QMargins(margin, margin, margin, margin));

  QPainterPath path;
  path.addRoundedRect(bubbleRect, cornerRadius_, cornerRadius_);
  path.closeSubpath();

  QPainterPath arrow;
  int centerX = bubble_widget_->rect().center().x();
  int centerY = bubble_widget_->rect().center().y();

  switch (position_) {
    case Position::Left:
      arrow.moveTo(bubble_widget_->width() - margin, centerY - margin);
      arrow.lineTo(bubble_widget_->width() - margin, centerY + margin);
      arrow.lineTo(bubble_widget_->width(), centerY);
      break;
    case Position::Top:
      arrow.moveTo(centerX - margin, bubble_widget_->height() - margin);
      arrow.lineTo(centerX + margin, bubble_widget_->height() - margin);
      arrow.lineTo(centerX, bubble_widget_->height());
      break;
    case Position::Right:
      arrow.moveTo(margin, centerY - margin);
      arrow.lineTo(margin, centerY + margin);
      arrow.lineTo(0, centerY);
      break;
    case Position::Bottom:
      arrow.moveTo(centerX - margin, margin);
      arrow.lineTo(centerX + margin, margin);
      arrow.lineTo(centerX, 0);
      break;
    default:
      break;
  }
  arrow.closeSubpath();

  path.addPath(arrow);
  p.fillPath(path, QBrush(QColor(255, 255, 255, 245)));
}

void GuidancePresentation::drawHighlight(QPainter* painter) {
  QRect target_rect = use_anime_ ? anime_highlight_rect_ : highlightRect_;

  auto& p = *painter;
  if (target_rect.isNull()) {
    p.fillRect(rect(), maskColor_);
    return;
  }

  QRectF highlightWithMargin =
      target_rect.adjusted(-margin_, -margin_, margin_, margin_);

  QPainterPath fullPath;
  fullPath.addRect(rect());

  QPainterPath highlightPath;
  highlightPath.addRoundedRect(highlightWithMargin, 8, 8);

  // 通过挖孔的方式突出高亮
  QPainterPath maskPath = fullPath - highlightPath;
  p.fillPath(maskPath, maskColor_);

  // 绘制一个虚线外框
  p.save();
  p.setPen(QPen(QColor(0x4CAF50), borderWidth_, Qt::DashLine, Qt::RoundCap,
                Qt::RoundJoin));
  p.drawRoundedRect(highlightWithMargin, 8, 8);
  p.restore();

  // 如果有图片，在区域内绘制图片
  if (!pixmap_.isNull()) {
    p.drawPixmap(target_rect, pixmap_, pixmap_.rect());
  }
}

void GuidancePresentation::changeBubblePosition() {
  int spacing = kLayoutMargin - 15;

  DirectionInfo infoLeft = evaluateDirectionSpace(Position::Left);
  DirectionInfo infoRight = evaluateDirectionSpace(Position::Right);
  DirectionInfo infoTop = evaluateDirectionSpace(Position::Top);
  DirectionInfo infoBottom = evaluateDirectionSpace(Position::Bottom);

  Position bestPos = Position::Right;

  if (infoLeft.canPlace && infoRight.canPlace) {
    if (infoLeft.spaceWidth > infoRight.spaceWidth) {
      bestPos = Position::Left;
    } else {
      bestPos = Position::Right;
    }
  } else if (infoLeft.canPlace) {
    bestPos = Position::Left;
  } else if (infoRight.canPlace) {
    bestPos = Position::Right;
  } else if (infoTop.canPlace && infoBottom.canPlace) {
    if (infoTop.spaceHeight > infoBottom.spaceHeight) {
      bestPos = Position::Top;
    } else {
      bestPos = Position::Bottom;
    }
  } else if (infoTop.canPlace) {
    bestPos = Position::Top;
  } else if (infoBottom.canPlace) {
    bestPos = Position::Bottom;
  } else {
    int maxSpace = qMax(qMax(infoLeft.spaceWidth, infoRight.spaceWidth),
                        qMax(infoTop.spaceHeight, infoBottom.spaceHeight));
    if (maxSpace == infoLeft.spaceWidth) {
      bestPos = Position::Left;
    } else if (maxSpace == infoRight.spaceWidth) {
      bestPos = Position::Right;
    } else if (maxSpace == infoTop.spaceHeight) {
      bestPos = Position::Top;
    } else {
      bestPos = Position::Bottom;
    }
  }

  int x = 0, y = 0;

  QRect targetRect = highlightRect_;
  QSize viewportSize = this->size();
  QSize bubbleSize = bubble_widget_->size();
  switch (bestPos) {
    case Position::Left:
      x = targetRect.left() - bubbleSize.width() - spacing;
      y = targetRect.center().y() - bubbleSize.height() / 2;
      break;
    case Position::Right:
      x = targetRect.right() + spacing;
      y = targetRect.center().y() - bubbleSize.height() / 2;
      break;
    case Position::Top:
      x = targetRect.center().x() - bubbleSize.width() / 2;
      y = targetRect.top() - bubbleSize.height() - spacing;
      break;
    case Position::Bottom:
      x = targetRect.center().x() - bubbleSize.width() / 2;
      y = targetRect.bottom() + spacing;
      break;
    default:
      break;
  }

  const int margin = 20;
  x = qMax(margin, qMin(x, viewportSize.width() - bubbleSize.width() - margin));
  y = qMax(margin,
           qMin(y, viewportSize.height() - bubbleSize.height() - margin));

  if (!use_anime_) {
    bubble_widget_->move(x, y);
  } else {
    anime_bubble_->setStartValue(bubble_widget_->pos());
    anime_bubble_->setEndValue(QPoint(x, y));
    anime_bubble_->start();
  }
  position_ = bestPos;
}

GuidancePresentation::DirectionInfo
GuidancePresentation::evaluateDirectionSpace(Position pos) {
  const int spacing = 15;
  DirectionInfo info;

  QRect targetRect = highlightRect_;
  QSize viewportSize = this->size();
  QSize bubbleSize = bubble_widget_->size();

  switch (pos) {
    case Left: {
      info.spaceWidth = targetRect.left();
      info.spaceHeight = viewportSize.height();

      if (bubbleSize.height() <= viewportSize.height() - spacing * 2 &&
          bubbleSize.width() + spacing <= targetRect.left()) {
        info.canPlace = true;
      }
    } break;
    case Top: {
      info.spaceWidth = viewportSize.width();
      info.spaceHeight = targetRect.top();

      if (bubbleSize.width() <= viewportSize.width() - spacing * 2 &&
          bubbleSize.height() + spacing <= targetRect.top()) {
        info.canPlace = true;
      }
    } break;
    case Right: {
      info.spaceWidth = viewportSize.width() - targetRect.right();
      info.spaceHeight = viewportSize.height();

      if (bubbleSize.height() <= viewportSize.height() - spacing * 2 &&
          bubbleSize.width() + spacing <=
              viewportSize.width() - targetRect.right()) {
        info.canPlace = true;
      }
    } break;
    case Bottom: {
      info.spaceWidth = viewportSize.width();
      info.spaceHeight = viewportSize.height() - targetRect.bottom();

      if (bubbleSize.width() <= viewportSize.width() - spacing * 2 &&
          bubbleSize.height() + spacing <=
              viewportSize.height() - targetRect.bottom()) {
        info.canPlace = true;
      }
    } break;
    default:
      break;
  }
  return info;
}

void GuidancePresentation::paintEvent(QPaintEvent* event) {
  QWidget::paintEvent(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  drawHighlight(&p);
}
