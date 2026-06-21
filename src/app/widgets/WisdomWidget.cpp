#include "WisdomWidget.h"

#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <algorithm>
#include <random>

// ════════════════════════════════════════════════════════════════════════════�?//  Construction
// ════════════════════════════════════════════════════════════════════════════�?
WisdomWidget::WisdomWidget(QWidget* parent)
    : SaverWidgetBase(parent), rng_(std::random_device{}()) {
  // Load poems from database
  poems_ = WisdomDatabase::instance().initDatabase(
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

  // Build shuffled indexes
  shuffledIds_.resize(poems_.size());
  std::iota(shuffledIds_.begin(), shuffledIds_.end(), 0);
  std::shuffle(shuffledIds_.begin(), shuffledIds_.end(), rng_);
  currentIndex_ = 0;

  // Theme colors
  paperTheme_ = {
      QColor(0xF7, 0xF4, 0xEB),  // 宣纸色背�?      QColor(0x2C, 0x2C, 0x2C),  // 墨色文字
      QColor(0x66, 0x66, 0x66),  // 灰色赏析
      QColor(0xC4, 0x3D, 0x3D)   // 朱砂�?accent
  };
  inkTheme_ = {
      QColor(0x1A, 0x1A, 0x1A),  // 深夜墨色背景
      QColor(0xE8, 0xE0, 0xD0),  // 暖白文字
      QColor(0x99, 0x99, 0x99),  // 灰色赏析
      QColor(0xC4, 0x3D, 0x3D)   // 朱砂�?accent
  };
  currentTheme_ = paperTheme_;

  setObjectName(QStringLiteral("wisdomWidget"));
  setMinimumSize(600, 450);
  setMouseTracking(false);

  initUi();
  initSignals();

  // Apply default theme (background fill + text color)
  applyTheme(paperTheme_);
  setFadeOpacity(1.0);

  // Compute optimal fixed font size for all poems
  computeOptimalFont();

  // Set fixed height for 4 lines (based on current font)
  QFontMetrics fm(sentenceLabel_->font());
  sentenceLabel_->setFixedHeight(fm.lineSpacing() * 4 + 12);

  // Load first poem
  loadPoem(shuffledIds_[currentIndex_]);

  // Show commentary immediately for first poem
  setCommentaryOpacity(1.0);
}

WisdomWidget::~WisdomWidget() = default;

QString WisdomWidget::displayName() const {
  return QStringLiteral("哲思·片刻");
}

// ════════════════════════════════════════════════════════════════════════════�?//  UI setup
// ════════════════════════════════════════════════════════════════════════════�?
void WisdomWidget::initUi() {
  // ── Sentence label ──
  sentenceLabel_ = new QLabel(this);
  sentenceLabel_->setObjectName(QStringLiteral("sentenceLabel"));
  sentenceLabel_->setAlignment(Qt::AlignCenter);
  sentenceLabel_->setWordWrap(false);
  sentenceLabel_->setMinimumWidth(200);
  // Font is set by computeOptimalFont() after loading poems

  // ── Commentary label ──
  commentaryLabel_ = new QLabel(this);
  commentaryLabel_->setObjectName(QStringLiteral("commentaryLabel"));
  commentaryLabel_->setAlignment(Qt::AlignCenter);
  commentaryLabel_->setWordWrap(true);
  commentaryLabel_->setMinimumWidth(200);
  commentaryLabel_->setMaximumWidth(520);
  QFont commFont(QStringLiteral("Microsoft YaHei"));
  commFont.setPointSize(12);
  commentaryLabel_->setFont(commFont);

  // ── Source / dynasty label ──
  sourceLabel_ = new QLabel(this);
  sourceLabel_->setObjectName(QStringLiteral("sourceLabel"));
  sourceLabel_->setAlignment(Qt::AlignCenter);
  sourceLabel_->setMinimumWidth(200);
  sourceLabel_->setMaximumWidth(520);
  QFont srcFont(QStringLiteral("Microsoft YaHei"));
  srcFont.setPointSize(11);
  srcFont.setItalic(true);
  sourceLabel_->setFont(srcFont);

  // ── Content container ──
  auto* contentLayout = new QVBoxLayout;
  contentLayout->setAlignment(Qt::AlignCenter);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);
  contentLayout->addSpacing(48);
  contentLayout->addWidget(sentenceLabel_, 0, Qt::AlignCenter);
  contentLayout->addSpacing(48);
  contentLayout->addWidget(commentaryLabel_, 0, Qt::AlignCenter);
  contentLayout->addSpacing(16);
  contentLayout->addWidget(sourceLabel_, 0, Qt::AlignCenter);

  // ── Outer vertical layout: center content vertically ──
  auto* outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);
  outerLayout->setSpacing(0);
  outerLayout->addStretch(1);
  outerLayout->addLayout(contentLayout, 0);
  outerLayout->addStretch(1);

  // ── Fade animations (palette alpha, no QGraphicsOpacityEffect) ──
  fadeOutAnim_ = new QVariantAnimation(this);
  fadeOutAnim_->setDuration(300);
  fadeOutAnim_->setStartValue(1.0);
  fadeOutAnim_->setEndValue(0.0);
  fadeOutAnim_->setEasingCurve(QEasingCurve::InOutQuad);

  fadeInAnim_ = new QVariantAnimation(this);
  fadeInAnim_->setDuration(600);
  fadeInAnim_->setStartValue(0.0);
  fadeInAnim_->setEndValue(1.0);
  fadeInAnim_->setEasingCurve(QEasingCurve::InOutQuad);

  commentaryAppear_ = new QVariantAnimation(this);
  commentaryAppear_->setDuration(800);
  commentaryAppear_->setStartValue(0.0);
  commentaryAppear_->setEndValue(1.0);
  commentaryAppear_->setEasingCurve(QEasingCurve::OutCubic);

  connect(fadeOutAnim_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& v) { setFadeOpacity(v.toReal()); });
  connect(fadeInAnim_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& v) { setFadeOpacity(v.toReal()); });
  connect(commentaryAppear_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& v) { setCommentaryOpacity(v.toReal()); });

  setFocusPolicy(Qt::StrongFocus);
  setFocus();
}

void WisdomWidget::initSignals() {
  connect(fadeOutAnim_, &QVariantAnimation::finished, this, [this]() {
    currentIndex_++;
    if (currentIndex_ >= shuffledIds_.size()) {
      std::shuffle(shuffledIds_.begin(), shuffledIds_.end(), rng_);
      currentIndex_ = 0;
    }
    loadPoem(shuffledIds_[currentIndex_]);
    startFadeIn();
  });

  connect(fadeInAnim_, &QVariantAnimation::finished, this, [this]() {
    floatUpCommentary();
    animating_ = false;
  });
}

// ════════════════════════════════════════════════════════════════════════════�?//  Poem loading
// ════════════════════════════════════════════════════════════════════════════�?
void WisdomWidget::loadPoem(int index) {
  if (index < 0 || index >= poems_.size())
    return;

  const auto& poem = poems_[index];

  // Format sentence with manual line breaks, then set text
  sentenceLabel_->setText(formatSentence(poem.sentence));
  commentaryLabel_->setText(poem.commentary);

  // Show dynasty · source
  if (poem.dynasty.isEmpty() && poem.source.isEmpty()) {
    sourceLabel_->setText(QString());
  } else if (poem.dynasty.isEmpty()) {
    sourceLabel_->setText(poem.source);
  } else if (poem.source.isEmpty()) {
    sourceLabel_->setText(poem.dynasty);
  } else {
    sourceLabel_->setText(poem.dynasty + QStringLiteral(" · ") + poem.source);
  }

  setCommentaryOpacity(0.0);

  emit poemChanged(poem.sentence, poem.source);
}

QString WisdomWidget::formatSentence(const QString& text) const {
  // Split by Chinese punctuation for natural line breaks
  QStringList clauses =
      text.split(QRegularExpression(QStringLiteral("[，。；！？、]")),
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                 Qt::SkipEmptyParts);
#else
                 QString::SkipEmptyParts);
#endif
  if (clauses.size() > 1) {
    QStringList lines;
    for (const auto& c : clauses) {
      lines << c.trimmed();
    }
    return lines.join(QStringLiteral("\n"));
  }
  return text;
}

void WisdomWidget::computeOptimalFont() {
  // Scan all poems to find the longest clause length
  int maxClauseLen = 0;
  for (const auto& poem : poems_) {
    QStringList clauses =
        poem.sentence.split(QRegularExpression(QStringLiteral("[，。；！？、]")),
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                            Qt::SkipEmptyParts);
#else
                            QString::SkipEmptyParts);
#endif
    for (const auto& c : clauses) {
      maxClauseLen = qMax(maxClauseLen, c.trimmed().length());
    }
    if (clauses.isEmpty())
      maxClauseLen = qMax(maxClauseLen, poem.sentence.length());
  }

  // Pick a font size that fits the longest clause
  int pt = 44;
  if (maxClauseLen > 8)  pt = 36;
  if (maxClauseLen > 12) pt = 30;
  if (maxClauseLen > 16) pt = 24;
  if (maxClauseLen > 22) pt = 20;
  if (maxClauseLen > 30) pt = 16;

  QFont f(QStringLiteral("KaiTi"), pt);
  f.setStyleStrategy(QFont::PreferAntialias);
  sentenceLabel_->setFont(f);
}

// ════════════════════════════════════════════════════════════════════════════�?//  Animations
// ════════════════════════════════════════════════════════════════════════════�?
void WisdomWidget::refresh() {
  if (animating_)
    return;
  animating_ = true;
  startFadeOut();
}

void WisdomWidget::startFadeOut() {
  fadeInAnim_->stop();
  commentaryAppear_->stop();
  fadeOutAnim_->start();
}

void WisdomWidget::startFadeIn() {
  fadeInAnim_->start();
}

void WisdomWidget::floatUpCommentary() {
  commentaryAppear_->start();
}

void WisdomWidget::setFadeOpacity(qreal opacity) {
  if (sentenceLabel_) {
    QColor c = currentTheme_.text;
    c.setAlphaF(qBound(0.0, opacity, 1.0));
    sentenceLabel_->setStyleSheet(
        QStringLiteral("QLabel{color:rgba(%1,%2,%3,%4);background:transparent;padding:6px 0;}")
            .arg(c.red()).arg(c.green()).arg(c.blue())
            .arg(c.alphaF(), 0, 'f', 3));
  }
}

void WisdomWidget::setCommentaryOpacity(qreal opacity) {
  auto setQss = [this](QLabel* label, const QColor& base, qreal a) {
    if (!label) return;
    QColor c = base;
    c.setAlphaF(qBound(0.0, a, 1.0));
    label->setStyleSheet(
        QStringLiteral("QLabel{color:rgba(%1,%2,%3,%4);background:transparent;}")
            .arg(c.red()).arg(c.green()).arg(c.blue())
            .arg(c.alphaF(), 0, 'f', 3));
  };
  setQss(commentaryLabel_, currentTheme_.commentary, opacity);
  setQss(sourceLabel_, currentTheme_.commentary, opacity);
}

// ════════════════════════════════════════════════════════════════════════════�?//  Theme
// ════════════════════════════════════════════════════════════════════════════�?
void WisdomWidget::setDarkTheme(bool dark) {
  currentTheme_ = dark ? inkTheme_ : paperTheme_;
  applyTheme(currentTheme_);
}

void WisdomWidget::applyTheme(const ThemeColors& theme) {
  QPalette pal = palette();
  QColor bg = theme.background;
  bg.setAlpha(180);  // 半透明，让 overlay 背景透出
  pal.setColor(QPalette::Window, bg);
  setPalette(pal);
  setAutoFillBackground(true);

  if (sentenceLabel_) {
    QPalette sp = sentenceLabel_->palette();
    sp.setColor(QPalette::WindowText, theme.text);
    sentenceLabel_->setPalette(sp);
  }
  if (commentaryLabel_) {
    QPalette cp = commentaryLabel_->palette();
    cp.setColor(QPalette::WindowText, theme.commentary);
    commentaryLabel_->setPalette(cp);
  }
}

// ════════════════════════════════════════════════════════════════════════════�?//  Events
// ════════════════════════════════════════════════════════════════════════════�?
void WisdomWidget::mousePressEvent(QMouseEvent* event) {
  QWidget::mousePressEvent(event);
  refresh();
}

void WisdomWidget::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Right ||
      event->key() == Qt::Key_Down || event->key() == Qt::Key_Return) {
    refresh();
    return;
  }
  QWidget::keyPressEvent(event);
}

void WisdomWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
}
