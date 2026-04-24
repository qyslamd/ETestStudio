#pragma once

#include <QLabel>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QWidget>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

class GuidanceFlow;
class GuidanceStep;
class GuidancePresentation : public QWidget {
  Q_OBJECT
 public:
  GuidancePresentation(QWidget* parent = nullptr);

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

 private:
  void initUi();
  void initOthers();
  void initSignals();

  // 在 bubble 上绘制倒计时
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
  QRect targetRect_;
  const int kLayoutMargin = 26;
  int arrowSize_ = 12;
  int cornerRadius_ = 12;

  // overlay members
  QRect highlightRect_;
  QPixmap pixmap_;
  int margin_ = 10;
  QColor maskColor_ = QColor(0, 0, 0, 180);
  QColor borderColor_ = QColor(255, 170, 0);
  int borderWidth_ = 3;

  // 搞动画的
  bool use_anime_ = true;
  const int kAnimeDur = 300;
  QVariantAnimation* anime_highlight_;
  QPropertyAnimation* anime_bubble_;
  QRect anime_highlight_rect_;
  QPixmap pixmap_smile_;

  // 自动执行用的
  bool auto_mode_ = false;
  QTimer* count_down_timer_;
  qreal time_percent_ = 1.0;
};
