#include "guidance_presentation.h"

#include <QEasingCurve>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include "ThemeManager.h"
#include "guidance_config.h"

namespace etest::app {

using etest::core_ui::ThemeManager;

GuidancePresentation::GuidancePresentation(QWidget* parent)
    : QWidget(parent),
      bubble_widget_(new QWidget(this)),
      title_label_(new QLabel(bubble_widget_)),
      countdown_label_(new QLabel(bubble_widget_)),
      text_label_(new QLabel(bubble_widget_)),
      skip_btn_(new QPushButton(QStringLiteral("跳过"), bubble_widget_)),
      prev_btn_(new QPushButton(QStringLiteral("上一步"), bubble_widget_)),
      next_btn_(new QPushButton(QStringLiteral("下一步"), bubble_widget_)),
      count_down_timer_(new QTimer(this)) {
  // 覆盖 viewport 的半透明遮罩，让主窗口内容透出（遮罩底色自绘）。
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);
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
  bubble_widget_->setObjectName(QStringLiteral("bubble_self"));
  // 气泡底由自绘（drawBubble）完成，背景保持透明，让遮罩/主窗口内容透出。
  bubble_widget_->setAutoFillBackground(false);
  bubble_widget_->setAttribute(Qt::WA_TranslucentBackground);
  bubble_widget_->setMinimumWidth(300);
  bubble_widget_->setMinimumHeight(200);

  QVBoxLayout* mainLayout = new QVBoxLayout(bubble_widget_);
  mainLayout->setContentsMargins(kLayoutMargin, kLayoutMargin, kLayoutMargin,
                                 kLayoutMargin);
  mainLayout->setSpacing(8);

  countdown_label_->setAlignment(Qt::AlignRight);
  countdown_label_->setObjectName(QStringLiteral("bubble_countdown"));
  countdown_label_->hide();

  title_label_->setAlignment(Qt::AlignCenter);
  title_label_->setObjectName(QStringLiteral("bubble_title"));

  text_label_->setObjectName(QStringLiteral("bubble_text"));
  text_label_->setWordWrap(true);

  QHBoxLayout* horizontalLayout = new QHBoxLayout();
  horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
  horizontalLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  skip_btn_->setObjectName(QStringLiteral("bubble_skip"));
  horizontalLayout->addWidget(skip_btn_);

  prev_btn_->setObjectName(QStringLiteral("bubble_prev"));
  horizontalLayout->addWidget(prev_btn_);

  next_btn_->setObjectName(QStringLiteral("bubble_next"));
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
    // 倒计时 5 秒自动前进（与文案 5..0 一致）：5000ms / 30ms 每次 tick。
    static constexpr int kCountDownMaxTicks = 5000 / 30;

    if (++count_down_tick_ >= kCountDownMaxTicks) {
      time_percent_ = 1.0;
      count_down_timer_->stop();
      count_down_tick_ = 0;  // 复位，避免下次续接残留值
      emit nextClicked();
    } else {
      time_percent_ = static_cast<qreal>(count_down_tick_) /
                      static_cast<qreal>(kCountDownMaxTicks);

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
    count_down_tick_ = 0;
    count_down_timer_->start(30);
  }
}

void GuidancePresentation::setIsAutoMode(bool autoMode) {
  auto_mode_ = autoMode;
  countdown_label_->setVisible(auto_mode_);
  // 每次切换都复位计数并停表，避免下次重启从残留值继续。
  count_down_tick_ = 0;
  if (!auto_mode_) {
    count_down_timer_->stop();
  }
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
  QColor bar = accentColor();
  bar.setAlpha(245);
  p.fillPath(path, QBrush(bar));
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
  p.fillPath(path, QBrush(bubbleColor()));
}

void GuidancePresentation::drawHighlight(QPainter* painter) {
  QRect target_rect = use_anime_ ? anime_highlight_rect_ : highlightRect_;

  auto& p = *painter;
  if (target_rect.isNull()) {
    p.fillRect(rect(), maskColor());
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
  p.fillPath(maskPath, maskColor());

  // 绘制一个虚线外框（accent 色）
  p.save();
  p.setPen(QPen(accentColor(), borderWidth_, Qt::DashLine, Qt::RoundCap,
                Qt::RoundJoin));
  p.drawRoundedRect(highlightWithMargin, 8, 8);
  p.restore();

  // 如果有图片，在区域内绘制图片
  if (!pixmap_.isNull()) {
    p.drawPixmap(target_rect, pixmap_, pixmap_.rect());
  }
}

void GuidancePresentation::mousePressEvent(QMouseEvent* event) {
  // 高亮目标区域：引导期间目标不响应鼠标（设计文档 D11），点击仅拦截不退出。
  QRect target_rect = use_anime_ ? anime_highlight_rect_ : highlightRect_;
  if (target_rect.adjusted(-margin_, -margin_, margin_, margin_)
          .contains(event->pos())) {
    return;
  }

  // 遮罩空白：退出引导（设计文档 3.7 / D5）。
  count_down_timer_->stop();
  emit skipClicked();
  QWidget::mousePressEvent(event);
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

QColor GuidancePresentation::maskColor() const {
  // 遮罩按设计文档 3.8 固定为半透明黑（default 0.50 / vscode 0.55），
  // alpha 随明暗主题微调，无单个硬编码色值。
  const int alpha = ThemeManager::instance().isDarkTheme() ? 140 : 128;
  return QColor(0, 0, 0, alpha);
}

QColor GuidancePresentation::bubbleColor() const {
  QColor color = ThemeManager::instance().panelBackground();
  color.setAlpha(245);
  return color;
}

QColor GuidancePresentation::accentColor() const {
  return ThemeManager::instance().accentColor();
}

}  // namespace etest::app
