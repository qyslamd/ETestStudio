#pragma once

#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

#include "WisdomDatabase.h"

class WisdomWidget : public QWidget {
    Q_OBJECT

 public:
    explicit WisdomWidget(QWidget* parent = nullptr);

    void refresh();

 protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

 private:
    void initUi();
    void initSignals();
    void applyTheme(const QString& tag);
    void adjustSentenceFont(const QString& text);
    QString formatSentence(const QString& text) const;
    void setFadeOpacity(qreal opacity);
    void setCommentaryOpacity(qreal opacity);
    void startFadeOut();
    void startFadeIn();
    void floatUpCommentary();

    struct ThemeColors {
        QColor background;
        QColor text;
        QColor commentary;
        QColor accent;
        QColor divider;
    };

    QWidget* container_ = nullptr;
    QLabel* sentenceLabel_ = nullptr;
    QLabel* sourceLabel_ = nullptr;
    QLabel* commentaryLabel_ = nullptr;
    QFrame* divider_ = nullptr;
    QLabel* sealLabel_ = nullptr;
    QLabel* boatLabel_ = nullptr;

    QVariantAnimation* containerFadeAnim_ = nullptr;
    QVariantAnimation* commentaryFadeAnim_ = nullptr;

    QTimer* commentaryTimer_ = nullptr;

    PoemRecord currentPoem_;
    ThemeColors currentTheme_;
    qreal fadeOpacity_ = 1.0;
    qreal commentaryOpacity_ = 0.0;
    bool animating_ = false;

    QPoint dragOffset_;
};
