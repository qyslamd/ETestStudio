#pragma once

#include <QElapsedTimer>
#include <QLabel>
#include <QVariantAnimation>
#include <QWidget>
#include <random>

#include "SaverWidgetBase.h"
#include "WisdomDatabase.h"

/// 哲思·片刻 — 诗词屏保控件
///
/// 继承 SaverWidgetBase，被 TuxSaverOverlay 策略管理。
/// 展示中国古诗词金句 + 赏析，支持宣纸/墨色双主题切换。
class WisdomWidget : public SaverWidgetBase {
  Q_OBJECT
 public:
  explicit WisdomWidget(QWidget* parent = nullptr);
  ~WisdomWidget() override;

  // ── SaverWidgetBase ──
  QString displayName() const override;

  /// 切换下一句（带动画）
  void refresh();

  /// 设置主题（true = 墨色，false = 宣纸色）
  void setDarkTheme(bool dark);

 signals:
  void poemChanged(const QString& sentence, const QString& source);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void loadPoem(int index);
  QString formatSentence(const QString& text) const;
  void computeOptimalFont();
  void startFadeOut();
  void startFadeIn();
  void floatUpCommentary();
  void setFadeOpacity(qreal opacity);
  void setCommentaryOpacity(qreal opacity);

  struct ThemeColors {
    QColor background;
    QColor text;
    QColor commentary;
    QColor accent;
  };

  ThemeColors paperTheme_;
  ThemeColors inkTheme_;
  ThemeColors currentTheme_;

  void applyTheme(const ThemeColors& theme);

  QVector<PoemData> poems_;
  QVector<int> shuffledIds_;
  int currentIndex_ = -1;
  std::mt19937 rng_;

  // UI elements
  QLabel* sentenceLabel_ = nullptr;
  QLabel* commentaryLabel_ = nullptr;
  QLabel* sourceLabel_ = nullptr;

  // Animations
  QVariantAnimation* fadeOutAnim_ = nullptr;
  QVariantAnimation* fadeInAnim_ = nullptr;
  QVariantAnimation* commentaryAppear_ = nullptr;

  bool animating_ = false;
};
