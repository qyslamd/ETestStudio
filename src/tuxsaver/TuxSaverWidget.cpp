#include "TuxSaverWidget.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QtMath>

// ═════════════════════════════════════════════════════════════════════════════
//  Construction
// ═════════════════════════════════════════════════════════════════════════════

TuxSaverWidget::TuxSaverWidget(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setMinimumSize(400, 280);
  setAttribute(Qt::WA_TranslucentBackground);

  anim_timer_ = new QTimer(this);
  connect(anim_timer_, &QTimer::timeout, this, &TuxSaverWidget::tick);

  // 眨眼定时器：随机选一只可见企鹅触发
  blink_sched_ = new QTimer(this);
  blink_sched_->setSingleShot(true);
  connect(blink_sched_, &QTimer::timeout, this, [this]() {
    QVector<int> candidates;
    for (int i = 0; i < penguins_.size(); ++i) {
      if (penguins_[i].state != State::HIDDEN && penguins_[i].blink == 0)
        candidates.append(i);
    }
    if (!candidates.isEmpty()) {
      int idx = candidates[QRandomGenerator::global()->bounded(candidates.size())];
      penguins_[idx].blink = 0.001;
      penguins_[idx].blink_closing = true;
    }
    blink_sched_->start(QRandomGenerator::global()->bounded(2000, 6000));
  });

  // 初始化 3 只企鹅（位置在首次显示时确定）
  for (int i = 0; i < 3; ++i) {
    penguins_.append(PenguinData{});
  }

  // 默认说的话
  phrases_ = QStringList{
    QStringLiteral("好无聊啊..."),
    QStringLiteral("Zzz..."),
    QStringLiteral("看什么呢？"),
    QStringLiteral("今天天气不错"),
    QStringLiteral("咕..."),
    QStringLiteral("想出去走走"),
    QStringLiteral("代码写完了吗？"),
    QStringLiteral("嘿！"),
  };

  blink_sched_->start(QRandomGenerator::global()->bounded(2000, 6000));

  qApp->installEventFilter(this);
  idle_timer_.start();
  startIdleDetect();
}

TuxSaverWidget::~TuxSaverWidget() = default;

void TuxSaverWidget::setPenguinCount(int n) {
  n = qBound(1, n, 20);
  while (penguins_.size() < n) {
    PenguinData d;
    d.px = QRandomGenerator::global()->bounded(70, qMax(71, width() - 70));
    d.py = QRandomGenerator::global()->bounded(35, qMax(36, height() - 40));
    penguins_.append(d);
  }
  while (penguins_.size() > n)
    penguins_.pop_back();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Idle detection
// ═════════════════════════════════════════════════════════════════════════════

void TuxSaverWidget::startIdleDetect() {
  anim_timer_->start(33);
}

bool TuxSaverWidget::eventFilter(QObject* obj, QEvent* event) {
  switch (event->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
    case QEvent::Wheel:
      idle_timer_.restart();
      user_active_ = true;
      break;
    default:
      break;
  }
  return QWidget::eventFilter(obj, event);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Main tick
// ═════════════════════════════════════════════════════════════════════════════

void TuxSaverWidget::tick() {
  const int idle = static_cast<int>(idle_timer_.elapsed() / 1000);

  for (auto& d : penguins_) {
    // HIDDEN → appear when idle threshold reached
    if (d.state == State::HIDDEN && idle >= idle_threshold_) {
      setState(d, State::IDLE);
      d.px = QRandomGenerator::global()->bounded(70, qMax(71, width() - 70));
      d.py = QRandomGenerator::global()->bounded(35, qMax(36, height() - 40));
    }

    d.state_elapsed++;

    // ── 对话气泡（静止状态：站立、张望、挠头） ──
    if (d.state == State::IDLE || d.state == State::LOOKING ||
        d.state == State::SCRATCHING) {
      if (d.speech_text.isEmpty()) {
        if (d.state_elapsed > QRandomGenerator::global()->bounded(50, 110)
            && !phrases_.isEmpty()) {
          d.speech_text = phrases_[QRandomGenerator::global()->bounded(phrases_.size())];
          d.speech_elapsed = 0;
        }
      } else {
        d.speech_elapsed++;
        if (d.speech_elapsed > QRandomGenerator::global()->bounded(150, 210)) {
          d.speech_text.clear();
        }
      }
    }

    // ── Per-state logic ──
    switch (d.state) {
      case State::WALKING: {
        qreal step = 1.5;
        qreal dx = (d.flee_target_x - d.px);
        if (qAbs(dx) < step) {
          pickRandomState(d);
        } else {
          d.facing_left_ = (dx < 0);
          d.px += (dx > 0 ? 1 : -1) * step;
          d.anim_phase += 0.12;
          d.head_tilt = qSin(d.state_elapsed * 0.055) * 14;
        }
        break;
      }

      case State::FLEEING: {
        qreal step = 6.0;
        qreal dx = (d.flee_target_x - d.px);
        if (qAbs(dx) < step) {
          setState(d, State::HIDDEN);
        } else {
          d.facing_left_ = (dx < 0);
          d.px += (dx > 0 ? 1 : -1) * step;
          d.anim_phase += 0.25;
        }
        break;
      }

      case State::LOOKING:
        d.head_tilt = qSin(d.state_elapsed * 0.08) * 20;
        if (d.state_elapsed > 120)
          pickRandomState(d);
        break;

      case State::SCRATCHING:
        if (d.state_elapsed > 60)
          pickRandomState(d);
        break;

      case State::YAWNING: {
        double t = d.state_elapsed / 60.0;
        if (t < 0.3)
          d.beak_open = t / 0.3;
        else if (t < 0.7)
          d.beak_open = 1.0;
        else
          d.beak_open = 1.0 - (t - 0.7) / 0.3;
        if (t >= 1.0)
          pickRandomState(d);
        break;
      }

      case State::SLEEPING:
        d.anim_phase += 0.02;
        if (d.state_elapsed > 240)
          pickRandomState(d);
        break;

      case State::SURPRISED: {
        double t = d.state_elapsed / 30.0;
        if (t < 0.15)
          d.body_squash = t / 0.15;
        else if (t < 0.35)
          d.body_squash = 1.0 - (t - 0.15) / 0.2;
        else {
          d.body_squash = 0;
          setState(d, State::FLEEING);
          d.flee_target_x = d.facing_left_ ? -100 : width() + 100;
          continue;  // skip clampX for this penguin, it's leaving
        }
        break;
      }

      case State::IDLE:
        d.anim_phase += 0.03;
        if (d.state_elapsed > QRandomGenerator::global()->bounded(60, 180))
          pickRandomState(d);
        break;

      default:
        break;
    }

    // ── Blink animation (per-penguin) ──
    if (d.blink > 0) {
      const double speed = 0.07;
      if (d.blink_closing) {
        d.blink = qMin(1.0, d.blink + speed);
        if (d.blink >= 1.0) d.blink_closing = false;
      } else {
        d.blink = qMax(0.0, d.blink - speed);
        if (d.blink <= 0) {
          d.blink = 0;
          d.blink_closing = true;
          // next blink will be triggered by blink_sched_
        }
      }
    }

    clampX(d);
  }

  update();
}

void TuxSaverWidget::clampX(PenguinData& d) {
  d.px = qBound(60.0, d.px, static_cast<qreal>(width()) - 60.0);
}

// ═════════════════════════════════════════════════════════════════════════════
//  State machine
// ═════════════════════════════════════════════════════════════════════════════

void TuxSaverWidget::setState(PenguinData& d, State s) {
  d.state = s;
  d.state_elapsed = 0;
  d.head_tilt = 0;
  d.beak_open = 0;
  d.body_squash = 0;
  d.speech_text.clear();

  switch (s) {
    case State::WALKING:
      d.flee_target_x = QRandomGenerator::global()->bounded(80, qMax(81, width() - 80));
      break;
    case State::SLEEPING:
      d.anim_phase = 0;
      d.blink = 1.0;
      break;
    case State::IDLE:
      d.anim_phase = 0;
      break;
    default:
      break;
  }
}

QString TuxSaverWidget::stateName() const {
  State s = penguins_.isEmpty() ? State::HIDDEN : penguins_[0].state;
  switch (s) {
    case State::HIDDEN:     return QStringLiteral("隐藏");
    case State::IDLE:       return QStringLiteral("发呆");
    case State::WALKING:    return QStringLiteral("散步");
    case State::LOOKING:    return QStringLiteral("张望");
    case State::SCRATCHING: return QStringLiteral("挠头");
    case State::YAWNING:    return QStringLiteral("打哈欠");
    case State::SLEEPING:   return QStringLiteral("打盹");
    case State::SURPRISED:  return QStringLiteral("吓了一跳");
    case State::FLEEING:    return QStringLiteral("逃跑");
  }
  return QString();
}

void TuxSaverWidget::pickRandomState(PenguinData& d) {
  if (d.state == State::HIDDEN || d.state == State::FLEEING ||
      d.state == State::SURPRISED)
    return;

  int r = QRandomGenerator::global()->bounded(100);
  if (r < 35) {
    setState(d, State::WALKING);
    d.flee_target_x = QRandomGenerator::global()->bounded(80, qMax(81, width() - 80));
  } else if (r < 55) {
    setState(d, State::LOOKING);
  } else if (r < 70) {
    setState(d, State::IDLE);
  } else if (r < 82) {
    setState(d, State::SCRATCHING);
  } else if (r < 92) {
    setState(d, State::YAWNING);
  } else {
    setState(d, State::SLEEPING);
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Events
// ═════════════════════════════════════════════════════════════════════════════

void TuxSaverWidget::mousePressEvent(QMouseEvent* event) {
  for (auto& d : penguins_) {
    qreal dx = event->pos().x() - d.px;
    qreal dy = event->pos().y() - d.py;
    if (qAbs(dx) < 42 && qAbs(dy) < 48) {
      if (d.state != State::HIDDEN && d.state != State::FLEEING &&
          d.state != State::SURPRISED) {
        d.facing_left_ = (event->pos().x() < width() / 2);
        setState(d, State::SURPRISED);
        break;
      }
    }
  }
}

void TuxSaverWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  for (auto& d : penguins_) clampX(d);
}

void TuxSaverWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (!positioned_ && !penguins_.isEmpty()) {
    for (auto& d : penguins_) {
      d.px = QRandomGenerator::global()->bounded(70, qMax(71, width() - 70));
      d.py = QRandomGenerator::global()->bounded(35, qMax(36, height() - 40));
    }
    positioned_ = true;
  }
}

void TuxSaverWidget::setIdleThreshold(int sec) {
  idle_threshold_ = sec;
}

void TuxSaverWidget::setPhrases(const QStringList& phrases) {
  phrases_ = phrases;
}

QStringList TuxSaverWidget::phrases() const {
  return phrases_;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Paint
// ═════════════════════════════════════════════════════════════════════════════

void TuxSaverWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  drawBackground(p);

  for (const auto& d : penguins_) {
    if (d.state != State::HIDDEN) {
      drawPenguin(p, d.px, d.py, d.facing_left_, d);
    }
  }
}

void TuxSaverWidget::drawBackground(QPainter&) const {
  // Default: transparent background. Subclasses can override.
}

// ═════════════════════════════════════════════════════════════════════════════
//  Penguin drawing — Tux-style (side profile)
// ═════════════════════════════════════════════════════════════════════════════

void TuxSaverWidget::drawPenguin(QPainter& p, qreal px, qreal py,
                                  bool mirror, const PenguinData& d) const {
  const QColor bodyClr(0x22, 0x22, 0x28);
  const QColor bellyClr(0xFF, 0xFF, 0xFF);
  const QColor faceClr(0xF0, 0xF0, 0xF0);
  const QColor beakClr(0xFF, 0x8C, 0x00);
  const QColor footClr(0xFF, 0xA0, 0x30);
  const QColor eyeClr(0x18, 0x18, 0x18);
  const QColor shineClr(255, 255, 255, 200);
  const QColor blushClr(255, 180, 180, 60);
  const QColor shadowClr(0, 0, 0, 50);

  p.save();
  p.translate(px, py);

  if (mirror) {
    p.scale(-1, 1);
  }

  bool sleeping = (d.state == State::SLEEPING);
  bool yawning = (d.state == State::YAWNING);
  bool scratching = (d.state == State::SCRATCHING);
  bool surprised = (d.state == State::SURPRISED && d.body_squash > 0);
  bool walking = (d.state == State::WALKING || d.state == State::FLEEING);

  qreal walkBob = 0;
  qreal walkSway = 0;
  qreal walkLean = 0;
  qreal stepPhase = d.anim_phase;
  qreal crouch = 0;

  if (d.state == State::IDLE || d.state == State::SLEEPING) {
    walkBob = qSin(d.anim_phase) * 0.8;
  } else if (walking) {
    walkBob = qSin(stepPhase) * 2.0;
    walkSway = qSin(stepPhase) * 2.0;
    walkLean = -qSin(stepPhase) * 2.0;
    crouch = 5;
  }

  qreal squash = 1.0;
  if (surprised) {
    squash = 1.0 - d.body_squash * 0.3;
  }

  // ── Shadow ──
  p.setPen(Qt::NoPen);
  p.setBrush(shadowClr);
  qreal shadowScale = 1.0 + (1.0 - squash) * 0.5;
  p.drawEllipse(QPointF(4, 27 * squash), 16 * shadowScale, 4);

  p.save();
  p.scale(1, squash);

  // ── 偷感 crouch + 身体摆动 ──
  p.save();
  if (walking) {
    p.translate(0, crouch);
    p.translate(walkSway, 0);
    p.rotate(walkLean);
  }

  // ── Feet ──
  qreal nearFX = 0, farFX = 0;
  qreal nearFY = 0, farFY = 0;
  qreal tiptoeAngle = 0;

  if (walking) {
    qreal s = qSin(stepPhase);
    qreal c = qCos(stepPhase);
    nearFX = s * 6;
    nearFY = qMax(qAbs(c), 0.5) * 2;
    farFX = -s * 5;
    farFY = qMax(qAbs(qCos(stepPhase + M_PI)), 0.5) * 2;
    tiptoeAngle = 15;
  } else if (scratching) {
    nearFY = -5;
  }

  // Far foot (darker, behind)
  p.setBrush(QColor(0xBB, 0x80, 0x20));
  p.save();
  p.translate(-1 + farFX, 21 - farFY);
  if (walking) p.rotate(tiptoeAngle * 0.6);
  p.drawRoundedRect(QRectF(-4, 0, 8, 4), 2, 2);
  p.restore();

  // Near foot
  p.setBrush(footClr);
  p.save();
  p.translate(-3 + nearFX, 23 - nearFY);
  if (walking) p.rotate(tiptoeAngle);
  p.drawRoundedRect(QRectF(-5, 0, 10, 4), 2, 2);
  p.restore();

  // ── Body ──
  p.setBrush(bodyClr);
  p.drawEllipse(QPointF(1, 2), 13, 21);
  p.drawEllipse(QPointF(-1, -5), 11, 16);

  // ── Belly ──
  p.setBrush(bellyClr);
  QPainterPath belly;
  belly.addEllipse(QPointF(9, 2), 6, 15);
  p.drawPath(belly);

  // ── Wing ──
  qreal wingAngle = 15;
  if (d.state == State::WALKING || d.state == State::FLEEING) {
    wingAngle = 30 - qSin(stepPhase) * 45;
  } else if (scratching) {
    wingAngle = -30;
  } else if (surprised) {
    wingAngle = 50;
  }
  p.setBrush(bodyClr);
  p.save();
  p.translate(-2, -2);
  p.rotate(wingAngle);
  QPainterPath wingPath;
  wingPath.moveTo(0, -10);
  wingPath.quadTo(12, -6, 12, 0);
  wingPath.quadTo(12, 6, 0, 10);
  wingPath.quadTo(-4, 0, 0, -10);
  p.drawPath(wingPath);
  p.restore();

  // ── Head ──
  qreal headY = -20;
  if (scratching) headY = -19;
  if (sleeping) headY = -17;

  p.setBrush(bodyClr);
  p.drawEllipse(QPointF(2, headY), 11, 11);

  // ── Face ──
  p.setBrush(faceClr);
  p.drawEllipse(QPointF(7, headY), 8, 8);

  // ── Blush ──
  p.setBrush(blushClr);
  p.drawEllipse(QPointF(8, headY + 3), 3, 2.5);

  // ── Eye ──
  bool eyesClosed = (d.blink > 0.5 || sleeping || (yawning && d.beak_open > 0.7));
  if (eyesClosed) {
    p.setPen(QPen(eyeClr, 2));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(7, headY), QPointF(11, headY));
  } else {
    p.setPen(Qt::NoPen);
    p.setBrush(eyeClr);
    double eyeScale = surprised ? 1.3 : 1.0;
    p.drawEllipse(QPointF(9, headY), 3, 3.2 * eyeScale);
    p.setBrush(shineClr);
    p.drawEllipse(QPointF(8.2, headY - 1.2), 1.5 * eyeScale, 1.5 * eyeScale);
    p.setBrush(QColor(255, 255, 255, 130));
    p.drawEllipse(QPointF(10.2, headY + 1), 0.8 * eyeScale, 0.8 * eyeScale);
  }

  // ── Beak ──
  p.setBrush(beakClr);
  p.setPen(Qt::NoPen);
  if (yawning && d.beak_open > 0.1) {
    QPainterPath openBeak;
    openBeak.moveTo(11, headY + 2);
    openBeak.lineTo(20, headY + 1);
    openBeak.quadTo(19, headY + 3 + d.beak_open * 6, 11, headY + 4 + d.beak_open * 5);
    p.drawPath(openBeak);
    p.setBrush(QColor(0x44, 0x22, 0x00));
    p.drawEllipse(QPointF(14, headY + 3 + d.beak_open * 3), 2.5, 1.5);
    p.setBrush(beakClr);
    p.drawEllipse(QPointF(19, headY + 1), 1.5, 1.5);
  } else {
    QPainterPath beak;
    beak.moveTo(11, headY + 2);
    beak.lineTo(20, headY + 2);
    beak.quadTo(20, headY + 5, 11, headY + 6);
    p.drawPath(beak);
  }

  // ── Smile ──
  if (!yawning && !surprised) {
    p.setPen(QPen(bodyClr, 1.2));
    QPainterPath smile;
    smile.moveTo(11, headY + 7);
    smile.quadTo(14, headY + 9, 17, headY + 7);
    p.setBrush(Qt::NoBrush);
    p.drawPath(smile);
  }

  // ── Surprise mouth ──
  if (surprised) {
    p.setPen(Qt::NoPen);
    p.setBrush(eyeClr);
    p.drawEllipse(QPointF(14, headY + 4), 3.5, 4.5);
    p.setBrush(QColor(255, 255, 255, 180));
    p.drawEllipse(QPointF(12.5, headY + 2), 1.2, 1.2);
  }

  p.restore(); // crouch+lean
  p.restore(); // squash
  p.restore(); // mirror + translate

  // ── 对话气泡（屏幕坐标，不受 mirror 影响） ──
  if (!d.speech_text.isEmpty()) {
    QFont bubbleFont = font();
    bubbleFont.setPointSize(9);
    QFontMetrics fm(bubbleFont);

    const qreal kMaxW = 160;       // 最大宽度
    const qreal kPad = 8;          // 内边距
    const qreal kTailH = 7;        // 尾巴高度
    const qreal kRadius = 7;       // 圆角半径
    const qreal kMinBubbleW = 50;  // 最小气泡宽度

    // 测量换行文字尺寸
    QRectF textBound = fm.boundingRect(
        QRect(0, 0, static_cast<int>(kMaxW), 999),
        Qt::AlignLeft | Qt::TextWordWrap, d.speech_text);
    qreal tw = textBound.width();
    qreal th = textBound.height();

    qreal bw = qMax(tw + kPad * 2, kMinBubbleW);  // 气泡宽
    qreal bh = th + kPad * 2;                       // 气泡高

    // 头部顶部屏幕坐标（尾巴尖端指向这里）
    qreal headCX = px + (mirror ? -2 : 2);
    qreal headTopY = py + headY - 11;

    qreal bx = headCX - bw / 2.0;            // 气泡左边缘
    qreal by = headTopY - bh - kTailH;       // 气泡顶边缘
    qreal tailBaseY = by + bh;                // 尾巴根部 y

    // 限定不超出左右边界
    bx = qBound(2.0, bx, static_cast<qreal>(width()) - bw - 2.0);

    // 填充气泡（主体 + 尾巴）
    QPainterPath bubblePath;
    QRectF bodyRect(bx, by, bw, bh);
    bubblePath.addRoundedRect(bodyRect, kRadius, kRadius);

    QPainterPath tailPath;
    tailPath.moveTo(headCX - 5, tailBaseY);
    tailPath.lineTo(headCX, headTopY);
    tailPath.lineTo(headCX + 5, tailBaseY);
    tailPath.closeSubpath();
    bubblePath.addPath(tailPath);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 235));
    p.drawPath(bubblePath);

    // 气泡边框（仅主体，尾巴不描边）
    p.setPen(QPen(QColor(200, 200, 200, 180), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(bodyRect, kRadius, kRadius);

    // 文字
    p.setPen(QColor(30, 30, 30));
    p.setFont(bubbleFont);
    QRectF textRect(bx + kPad, by + kPad, tw, th);
    p.drawText(textRect, Qt::AlignLeft | Qt::TextWordWrap, d.speech_text);
  }
}
