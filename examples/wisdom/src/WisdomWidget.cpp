#include "WisdomWidget.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

// ═════════════════════════════════════════════════════════════════════════════
//  Construction
// ═════════════════════════════════════════════════════════════════════════════

WisdomWidget::WisdomWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("wisdomRoot");
    setMinimumSize(600, 400);
    setAttribute(Qt::WA_TranslucentBackground);

    currentPoem_ = WisdomDatabase::instance().currentPoem();

    initUi();
    initSignals();
    applyTheme(currentPoem_.tag);
    setFadeOpacity(1.0);
    setCommentaryOpacity(0.0);

    QTimer::singleShot(300, this, [this]() { floatUpCommentary(); });
}

QString WisdomWidget::formatSentence(const QString& text) const {
    QString result = text;
    static const QRegularExpression re(QStringLiteral("[，。；！？、]"));
    QStringList clauses = result.split(re, Qt::SkipEmptyParts);
    if (clauses.size() > 1) {
        QStringList lines;
        for (const auto& c : clauses) {
            lines << c.trimmed();
        }
        return lines.join(QStringLiteral("\n"));
    }
    return result;
}

void WisdomWidget::adjustSentenceFont(const QString& text) {
    QStringList clauses = text.split(
        QRegularExpression(QStringLiteral("[，。；！？、]")),
        Qt::SkipEmptyParts);
    int maxLen = 0;
    for (const auto& c : clauses) {
        maxLen = qMax(maxLen, c.trimmed().length());
    }
    if (maxLen == 0) maxLen = text.length();

    int pt = 44;
    if (maxLen > 8) pt = 36;
    if (maxLen > 12) pt = 30;
    if (maxLen > 16) pt = 24;
    if (maxLen > 22) pt = 20;
    if (maxLen > 30) pt = 16;

    QFont f("KaiTi", pt);
    f.setWeight(QFont::Light);
    f.setItalic(true);
    sentenceLabel_->setFont(f);
}

// ═════════════════════════════════════════════════════════════════════════════
//  UI — 四层纵向结构：顶部留白 25% → 核心文案 → 注释说明 → 底部留白 20%
// ═════════════════════════════════════════════════════════════════════════════

void WisdomWidget::initUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    container_ = new QWidget(this);
    container_->setObjectName("container");
    auto* layout = new QVBoxLayout(container_);
    layout->setContentsMargins(40, 0, 40, 0);
    layout->setSpacing(0);

    // ── (1) 顶部留白 25% ──
    layout->addStretch(25);

    // ── (2) 核心古文主文本区 ──
    sentenceLabel_ = new QLabel(container_);
    sentenceLabel_->setObjectName("sentenceLabel");
    sentenceLabel_->setAlignment(Qt::AlignCenter);
    sentenceLabel_->setWordWrap(false);
    adjustSentenceFont(currentPoem_.sentence);
    sentenceLabel_->setText(formatSentence(currentPoem_.sentence));
    layout->addWidget(sentenceLabel_, 0, Qt::AlignCenter);

    // 古文与注释之间拉大间距（约两行文字高度）
    layout->addSpacing(48);

    // ── (3) 注释说明区 ──
    // 分隔线
    divider_ = new QFrame(container_);
    divider_->setObjectName("divider");
    divider_->setFrameShape(QFrame::NoFrame);
    divider_->setFixedHeight(1);
    divider_->setMaximumWidth(60);
    divider_->setAutoFillBackground(true);
    layout->addWidget(divider_, 0, Qt::AlignCenter);

    layout->addSpacing(16);

    // 第一行：出处
    sourceLabel_ = new QLabel(container_);
    sourceLabel_->setObjectName("sourceLabel");
    sourceLabel_->setAlignment(Qt::AlignCenter);
    sourceLabel_->setMaximumWidth(560);
    sourceLabel_->setText(currentPoem_.source);
    QFont sourceFont("SimSun", 10);
    sourceLabel_->setFont(sourceFont);
    layout->addWidget(sourceLabel_, 0, Qt::AlignCenter);

    layout->addSpacing(12);

    // 第二行：解读
    commentaryLabel_ = new QLabel(container_);
    commentaryLabel_->setObjectName("commentaryLabel");
    commentaryLabel_->setAlignment(Qt::AlignCenter);
    commentaryLabel_->setWordWrap(true);
    commentaryLabel_->setMinimumWidth(200);
    commentaryLabel_->setMaximumWidth(520);
    commentaryLabel_->setText(currentPoem_.commentary);
    QFont commFont("Microsoft YaHei", 11);
    commentaryLabel_->setFont(commFont);
    layout->addWidget(commentaryLabel_, 0, Qt::AlignCenter);

    // ── (4) 底部留白 20% ──
    layout->addStretch(20);

    outerLayout->addWidget(container_);

    // Animations
    containerFadeAnim_ = new QVariantAnimation(this);
    commentaryFadeAnim_ = new QVariantAnimation(this);

    // ── 底部功能元素（绝对定位，resizeEvent 中放置）──
    // 红色「悟」按钮 — 右下角
    sealLabel_ = new QLabel(this);
    sealLabel_->setObjectName("sealLabel");
    sealLabel_->setText(QStringLiteral("\xe6\x82\x9f"));
    sealLabel_->setAlignment(Qt::AlignCenter);
    sealLabel_->setFixedSize(36, 36);
    QFont sealFont("KaiTi", 14);
    sealFont.setBold(true);
    sealLabel_->setFont(sealFont);

    // 淡水印「舟」— 左下角
    boatLabel_ = new QLabel(this);
    boatLabel_->setObjectName("boatLabel");
    boatLabel_->setText(QStringLiteral("\xe8\x88\x9f"));
    boatLabel_->setAlignment(Qt::AlignCenter);
    boatLabel_->setFixedSize(50, 50);
    QFont boatFont("KaiTi", 28);
    boatFont.setWeight(QFont::Light);
    boatLabel_->setFont(boatFont);
    boatLabel_->setStyleSheet(
        "color: rgba(100,100,100,25); background: transparent;");

    commentaryTimer_ = new QTimer(this);
    commentaryTimer_->setSingleShot(true);
}

void WisdomWidget::initSignals() {
    connect(containerFadeAnim_, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& v) { setFadeOpacity(v.toReal()); });

    connect(containerFadeAnim_, &QVariantAnimation::finished, this, [this]() {
        if (fadeOpacity_ < 0.5) {
            currentPoem_ = WisdomDatabase::instance().nextPoem();
            applyTheme(currentPoem_.tag);

            adjustSentenceFont(currentPoem_.sentence);
            sentenceLabel_->setText(formatSentence(currentPoem_.sentence));
            sourceLabel_->setText(currentPoem_.source);
            commentaryLabel_->setText(currentPoem_.commentary);

            setCommentaryOpacity(0.0);
            startFadeIn();
        } else {
            animating_ = false;
            commentaryTimer_->start(1500);
        }
    });

    connect(commentaryFadeAnim_, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& v) { setCommentaryOpacity(v.toReal()); });

    connect(commentaryTimer_, &QTimer::timeout, this,
            &WisdomWidget::floatUpCommentary);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Theme
// ═════════════════════════════════════════════════════════════════════════════

void WisdomWidget::applyTheme(const QString& tag) {
    static const ThemeColors coolTheme = {
        QColor(0xE2, 0xE6, 0xEB),
        QColor(0x2C, 0x35, 0x42),
        QColor(0x6A, 0x73, 0x80),
        QColor(0xC4, 0x3D, 0x3D),
        QColor(0xB0, 0xB8, 0xC0)
    };
    static const ThemeColors warmTheme = {
        QColor(0xF7, 0xF4, 0xEB),
        QColor(0x2C, 0x2C, 0x2C),
        QColor(0x8A, 0x8A, 0x8A),
        QColor(0xC4, 0x3D, 0x3D),
        QColor(0xC0, 0xB8, 0xB0)
    };
    static const ThemeColors darkTheme = {
        QColor(0x1A, 0x1A, 0x1A),
        QColor(0xD4, 0xC5, 0xA0),
        QColor(0x99, 0x99, 0x99),
        QColor(0xC4, 0x3D, 0x3D),
        QColor(0x44, 0x44, 0x44)
    };

    if (tag == QStringLiteral("\xe6\x83\x85\xe6\x84\x9f")) {
        currentTheme_ = darkTheme;
    } else if (tag == QStringLiteral(
                   "\xe5\x93\xb2\xe7\x90\x86")) {
        currentTheme_ = coolTheme;
    } else if (tag == QStringLiteral(
                   "\xe8\x87\xaa\xe7\x84\xb6")) {
        currentTheme_ = coolTheme;
    } else {
        currentTheme_ = warmTheme;
    }

    sealLabel_->setStyleSheet(
        QString("color: white; background-color: %1; "
                "border-radius: 4px; border: 1px solid %1;")
            .arg(currentTheme_.accent.name()));

    setFadeOpacity(fadeOpacity_);
    setCommentaryOpacity(commentaryOpacity_);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Opacity
// ═════════════════════════════════════════════════════════════════════════════

void WisdomWidget::setFadeOpacity(qreal opacity) {
    fadeOpacity_ = opacity;

    auto alphaColor = [](const QColor& c, qreal a) {
        QColor r = c;
        r.setAlphaF(a);
        return r;
    };

    sentenceLabel_->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(alphaColor(currentTheme_.text, opacity).name(QColor::HexArgb)));
    sourceLabel_->setStyleSheet(
        QString("color: %1; background: transparent;")
            .arg(alphaColor(currentTheme_.commentary, opacity).name(QColor::HexArgb)));

    QPalette dp = divider_->palette();
    dp.setColor(QPalette::Window, alphaColor(currentTheme_.divider, opacity));
    divider_->setPalette(dp);
}

void WisdomWidget::setCommentaryOpacity(qreal opacity) {
    commentaryOpacity_ = opacity;

    QColor c = currentTheme_.commentary;
    c.setAlphaF(opacity);
    commentaryLabel_->setStyleSheet(
        QString("color: %1; background: transparent;").arg(c.name(QColor::HexArgb)));
}

// ═════════════════════════════════════════════════════════════════════════════
//  Animations
// ═════════════════════════════════════════════════════════════════════════════

void WisdomWidget::refresh() {
    if (animating_) return;
    animating_ = true;
    commentaryTimer_->stop();
    startFadeOut();
}

void WisdomWidget::startFadeOut() {
    containerFadeAnim_->setDuration(300);
    containerFadeAnim_->setStartValue(1.0);
    containerFadeAnim_->setEndValue(0.0);
    containerFadeAnim_->setEasingCurve(QEasingCurve::InOutQuad);
    containerFadeAnim_->start();
}

void WisdomWidget::startFadeIn() {
    containerFadeAnim_->setDuration(600);
    containerFadeAnim_->setStartValue(0.0);
    containerFadeAnim_->setEndValue(1.0);
    containerFadeAnim_->setEasingCurve(QEasingCurve::InOutQuad);
    containerFadeAnim_->start();
}

void WisdomWidget::floatUpCommentary() {
    commentaryFadeAnim_->setDuration(800);
    commentaryFadeAnim_->setStartValue(0.0);
    commentaryFadeAnim_->setEndValue(1.0);
    commentaryFadeAnim_->setEasingCurve(QEasingCurve::OutCubic);
    commentaryFadeAnim_->start();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Events
// ═════════════════════════════════════════════════════════════════════════════

void WisdomWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragOffset_ = event->globalPos() - frameGeometry().topLeft();
    }
    QWidget::mousePressEvent(event);
}

void WisdomWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - dragOffset_);
    }
    QWidget::mouseMoveEvent(event);
}

void WisdomWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        QGuiApplication::quit();
        return;
    }
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return ||
        event->key() == Qt::Key_Right || event->key() == Qt::Key_Down) {
        refresh();
        return;
    }
    QWidget::keyPressEvent(event);
}

void WisdomWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(2, 2, -2, -2);
    p.setPen(Qt::NoPen);
    p.setBrush(currentTheme_.background);
    p.drawRoundedRect(r, 16, 16);
}

void WisdomWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    // 右下角「悟」按钮 — 向内缩进留出侧边空白
    int sealMargin = 30;
    sealLabel_->move(width() - sealLabel_->width() - sealMargin,
                     height() - sealLabel_->height() - sealMargin);

    // 左下角「舟」水印 — 远离边缘，避免和注释重叠
    int boatMargin = 40;
    boatLabel_->move(boatMargin,
                     height() - boatLabel_->height() - boatMargin);
}
