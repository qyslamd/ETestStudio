#pragma once

#include <QColor>
#include <QPixmap>
#include <QWidget>

class QEvent;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPropertyAnimation;
class QPushButton;
class QTimer;
class QVariantAnimation;

namespace etest::app {

class GuidanceFlow;
class GuidanceStep;

// 演示层：覆盖整个 viewport 的半透明遮罩 + 目标高亮 + 气泡导航。
// 自绘层（遮罩/气泡底/箭头/高亮框）颜色读 ThemeManager token，明暗主题自动适配；
// 气泡内部子控件（title/text/countdown/skip/prev/next）只设 objectName，由 QSS 覆盖。
class GuidancePresentation : public QWidget {
  Q_OBJECT
 public:
  explicit GuidancePresentation(QWidget* parent = nullptr);

  void updateUi(GuidanceFlow* const flow,
                GuidanceStep* const step,
                bool canPrev,
                bool canNext);
  void setIsAutoMode(bool autoMode);

 protected:
  void setTitle(const QString& title);
  void setText(const QString& text);

 signals:
  void nextClicked();
  void prevClicked();
  void skipClicked();

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  void initUi();
  void initOthers();
  void initSignals();

  // 在 bubble 上绘制倒计时进度
  void drawCountDownOnBubble(QPainter* painter);
  void drawBubble(QPainter* painter);
  void drawHighlight(QPainter* painter);

  void changeBubblePosition();
  struct DirectionInfo {
    bool canPlace = false;
    int spaceWidth = 0;
    int spaceHeight = 0;
  };
  enum Position { Auto, Left, Right, Top, Bottom } position_ = Left;
  DirectionInfo evaluateDirectionSpace(Position pos);

  // 自绘层颜色：读 ThemeManager token（明暗主题自动适配），禁止硬编码色值。
  QColor maskColor() const;
  QColor bubbleColor() const;
  QColor accentColor() const;

 private:
  // bubble members
  QWidget* bubble_widget_;
  QLabel* countdown_label_;
  QLabel* title_label_;
  QLabel* text_label_;
  QPushButton* skip_btn_;
  QPushButton* prev_btn_;
  QPushButton* next_btn_;

  QString title_;
  QString text_;
  const int kLayoutMargin = 26;
  int cornerRadius_ = 12;

  // overlay members
  QRect highlightRect_;
  QPixmap pixmap_;
  int margin_ = 10;
  int borderWidth_ = 3;

  // 搞动画的
  bool use_anime_ = true;
  const int kAnimeDur = 300;
  QVariantAnimation* anime_highlight_;
  QPropertyAnimation* anime_bubble_;
  QRect anime_highlight_rect_;

  // 自动执行用的
  bool auto_mode_ = false;
  QTimer* count_down_timer_;
  qreal time_percent_ = 1.0;
  // 本次自动倒计时已走的 tick 数；每次开始/停止都复位，避免续接上次残留值。
  int count_down_tick_ = 0;
};

}  // namespace etest::app
