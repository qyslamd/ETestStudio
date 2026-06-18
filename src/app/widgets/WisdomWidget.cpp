#include "WisdomWidget.h"

#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QVariantAnimation>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <algorithm>
#include <random>

// ═════════════════════════════════════════════════════════════════════════════
//  Construction
// ═════════════════════════════════════════════════════════════════════════════

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
      QColor(0xF7, 0xF4, 0xEB),  // 宣纸色背景
      QColor(0x2C, 0x2C, 0x2C),  // 墨色文字
      QColor(0x66, 0x66, 0x66),  // 灰色赏析
      QColor(0xC4, 0x3D, 0x3D)   // 朱砂红 accent
  };
  inkTheme_ = {
      QColor(0x1A, 0x1A, 0x1A),  // 深夜墨色背景
      QColor(0xE8, 0xE0, 0xD0),  // 暖白文字
      QColor(0x99, 0x99, 0x99),  // 灰色赏析
      QColor(0xC4, 0x3D, 0x3D)   // 朱砂红 accent
  };
  currentTheme_ = paperTheme_;

  setObjectName(QStringLiteral("wisdomWidget"));
  setMinimumSize(400, 300);
  setMouseTracking(false);

  initUi();
  initSignals();

  // Load first poem
  loadPoem(shuffledIds_[currentIndex_]);
}

WisdomWidget::~WisdomWidget() = default;

QString WisdomWidget::displayName() const {
  return QStringLiteral("哲思·片刻");
}

// ═════════════════════════════════════════════════════════════════════════════
//  UI setup
// ═════════════════════════════════════════════════════════════════════════════

void WisdomWidget::initUi() {
  auto* outerLayout = new QVBoxLayout(this);
  outerLayout->setAlignment(Qt::AlignCenter);

  // Inner container for centered text
  auto* container = new QWidget(this);
  container->setObjectName(QStringLiteral("wisdomContainer"));
  auto* layout = new QVBoxLayout(container);
  layout->setAlignment(Qt::AlignCenter);
  layout->setSpacing(0);

  // ── Sentence label ──
  sentenceLabel_ = new QLabel(this);
  sentenceLabel_->setObjectName(QStringLiteral("sentenceLabel"));
  sentenceLabel_->setAlignment(Qt::AlignCenter);
  sentenceLabel_->setWordWrap(true);
  sentenceLabel_->setMinimumWidth(200);
  QFont sentenceFont(QStringLiteral("KaiTi"));
  sentenceFont.setPointSize(32);
  sentenceFont.setStyleStrategy(QFont::PreferAntialias);
  sentenceLabel_->setFont(sentenceFont);

  // ── Divider ──
  layout->addSpacing(24);
  divider_ = new QFrame(this);
  divider_->setObjectName(QStringLiteral("divider"));
  divider_->setAutoFillBackground(true);
  divider_->setFrameShape(QFrame::NoFrame);
  divider_->setFixedHeight(2);
  divider_->setMaximumWidth(120);

  // ── Source label ──
  sourceLabel_ = new QLabel(this);
  sourceLabel_->setObjectName(QStringLiteral("sourceLabel"));
  sourceLabel_->setAlignment(Qt::AlignCenter);
  QFont sourceFont(QStringLiteral("KaiTi"));
  sourceFont.setPointSize(14);
  sourceLabel_->setFont(sourceFont);

  // ── Commentary label ──
  layout->addSpacing(16);
  commentaryLabel_ = new QLabel(this);
  commentaryLabel_->setObjectName(QStringLiteral("commentaryLabel"));
  commentaryLabel_->setAlignment(Qt::AlignCenter);
  commentaryLabel_->setWordWrap(true);
  commentaryLabel_->setMinimumWidth(200);
  commentaryLabel_->setMaximumWidth(500);
  QFont commFont(QStringLiteral("SimSun"));
  commFont.setPointSize(12);
  commentaryLabel_->setFont(commFont);

  layout->addWidget(sentenceLabel_, 0, Qt::AlignCenter);
  layout->addSpacing(24);
  layout->addWidget(divider_, 0, Qt::AlignCenter);
  layout->addSpacing(12);
  layout->addWidget(sourceLabel_, 0, Qt::AlignCenter);
  layout->addSpacing(16);
  layout->addWidget(commentaryLabel_, 0, Qt::AlignCenter);

  outerLayout->addWidget(container, 0, Qt::AlignCenter);

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

// ═════════════════════════════════════════════════════════════════════════════
//  Poem loading
// ═════════════════════════════════════════════════════════════════════════════

void WisdomWidget::loadPoem(int index) {
  if (index < 0 || index >= poems_.size()) return;

  const auto& poem = poems_[index];
  sentenceLabel_->setText(poem.sentence);
  sourceLabel_->setText(poem.source);
  commentaryLabel_->setText(poem.commentary);
  setCommentaryOpacity(0.0);

  emit poemChanged(poem.sentence, poem.source);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Animations
// ═════════════════════════════════════════════════════════════════════════════

void WisdomWidget::refresh() {
  if (animating_) return;
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
  auto alphaColor = [](const QColor& c, qreal a) {
    QColor r = c;
    r.setAlphaF(a);
    return r;
  };

  if (sentenceLabel_) {
    QPalette sp = sentenceLabel_->palette();
    sp.setColor(QPalette::WindowText, alphaColor(currentTheme_.text, opacity));
    sentenceLabel_->setPalette(sp);
  }
  if (sourceLabel_) {
    QPalette sp = sourceLabel_->palette();
    sp.setColor(QPalette::WindowText,
               alphaColor(currentTheme_.commentary, opacity));
    sourceLabel_->setPalette(sp);
  }
  if (divider_) {
    QPalette dp = divider_->palette();
    dp.setColor(QPalette::Window, alphaColor(currentTheme_.accent, opacity));
    divider_->setPalette(dp);
  }
}

void WisdomWidget::setCommentaryOpacity(qreal opacity) {
  if (commentaryLabel_) {
    QPalette cp = commentaryLabel_->palette();
    QColor c = currentTheme_.commentary;
    c.setAlphaF(opacity);
    cp.setColor(QPalette::WindowText, c);
    commentaryLabel_->setPalette(cp);
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Theme
// ═════════════════════════════════════════════════════════════════════════════

void WisdomWidget::setDarkTheme(bool dark) {
  currentTheme_ = dark ? inkTheme_ : paperTheme_;
  applyTheme(currentTheme_);
}

void WisdomWidget::applyTheme(const ThemeColors& theme) {
  QPalette pal = palette();
  pal.setColor(QPalette::Window, theme.background);
  setPalette(pal);
  setAutoFillBackground(true);

  if (sentenceLabel_) {
    QPalette sp = sentenceLabel_->palette();
    sp.setColor(QPalette::WindowText, theme.text);
    sentenceLabel_->setPalette(sp);
  }
  if (sourceLabel_) {
    QPalette sp = sourceLabel_->palette();
    sp.setColor(QPalette::WindowText, theme.commentary);
    sourceLabel_->setPalette(sp);
  }
  if (divider_) {
    QPalette dp = divider_->palette();
    dp.setColor(QPalette::Window, theme.accent);
    divider_->setPalette(dp);
  }
  if (commentaryLabel_) {
    QPalette cp = commentaryLabel_->palette();
    cp.setColor(QPalette::WindowText, theme.commentary);
    commentaryLabel_->setPalette(cp);
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Events
// ═════════════════════════════════════════════════════════════════════════════

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
  // Adjust font size based on widget width
  int w = event->size().width();
  if (sentenceLabel_) {
    int pt = qBound(18, w / 18, 42);
    QFont f = sentenceLabel_->font();
    f.setPointSize(pt);
    sentenceLabel_->setFont(f);
  }
}
