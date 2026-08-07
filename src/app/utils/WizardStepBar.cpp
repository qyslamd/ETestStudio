#include "WizardStepBar.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

#include "ThemeManager.h"

namespace etest::app {

using core_ui::ThemeManager;

WizardStepBar::WizardStepBar(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(48);
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this](bool) { update(); });
}

void WizardStepBar::setLabels(const QStringList& labels) {
  labels_ = labels;
  if (current_step_ >= labels_.size()) {
    current_step_ = labels_.isEmpty() ? 0 : labels_.size() - 1;
  }
  update();
}

void WizardStepBar::setCurrentStep(int currentStep) {
  const int clamped =
      qBound(0, currentStep, labels_.isEmpty() ? 0 : labels_.size() - 1);
  if (clamped == current_step_) {
    return;
  }
  current_step_ = clamped;
  update();
}

QSize WizardStepBar::sizeHint() const {
  return QSize(520, 48);
}

QSize WizardStepBar::minimumSizeHint() const {
  return QSize(200, 40);
}

WizardStepBar::StepColors WizardStepBar::colors() const {
  StepColors c;
  const ThemeManager& tm = ThemeManager::instance();
  const bool dark = tm.isDarkTheme();
  c.accent = tm.accentColor();
  c.success = dark ? QColor(0x3F, 0xB9, 0x50) : QColor(0x10, 0x7C, 0x10);
  c.circleBg = dark ? QColor(0x3A, 0x3A, 0x4A) : QColor(0xE8, 0xE8, 0xF0);
  c.circleText = tm.disabledTextColor();
  c.labelActive = tm.textColor();
  c.lineIdle = dark ? QColor(0x3A, 0x3A, 0x4A) : QColor(0xD0, 0xD0, 0xDD);
  return c;
}

void WizardStepBar::paintEvent(QPaintEvent*) {
  if (labels_.isEmpty()) {
    return;
  }

  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  const int count = labels_.size();
  const qreal colW = width() / static_cast<qreal>(count);
  const int circleR = 16;
  const int cy = height() / 2;

  const StepColors c = colors();

  QFont labelFont = this->font();
  labelFont.setWeight(QFont::Medium);
  p.setFont(labelFont);
  const QFontMetrics fm(labelFont);

  // 逐个绘制：连接线、圆、标签
  for (int i = 0; i < count; ++i) {
    const qreal colX = i * colW;
    const int cx = static_cast<int>(colX) + circleR;  // 圆左侧对齐
    const bool completed = i < current_step_;
    const bool active = i == current_step_;

    // 连接线（当前步之后到下一步圆左缘），颜色跟随本步状态
    if (i < count - 1) {
      const int labelRight = cx + circleR + 6 + fm.horizontalAdvance(labels_.at(i));
      const int lineStart = labelRight + 8;
      const int lineEnd =
          static_cast<int>((i + 1) * colW) + circleR - 8;  // 下一步圆左缘 - 间距
      if (lineEnd > lineStart) {
        const QColor lineColor =
            completed ? c.success : (active ? c.accent : c.lineIdle);
        p.setPen(Qt::NoPen);
        p.setBrush(lineColor);
        p.drawRoundedRect(QRectF(lineStart, cy - 1.0, lineEnd - lineStart, 2.0),
                          1.0, 1.0);
      }
    }

    // 圆
    const QRectF circleRect(cx - circleR, cy - circleR, circleR * 2.0,
                            circleR * 2.0);
    if (active) {
      // 光晕
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(c.accent.red(), c.accent.green(), c.accent.blue(), 70));
      p.drawEllipse(circleRect.adjusted(-3.0, -1.0, 3.0, 3.0));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(completed ? c.success : (active ? c.accent : c.circleBg));
    p.drawEllipse(circleRect);

    // 圆内内容
    p.setPen(completed ? Qt::white : (active ? Qt::white : c.circleText));
    if (completed) {
      QPainterPath check;
      check.moveTo(cx - 5.5, cy);
      check.lineTo(cx - 1.5, cy + 4.0);
      check.lineTo(cx + 5.5, cy - 4.0);
      p.setPen(QPen(Qt::white, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      p.setBrush(Qt::NoBrush);
      p.drawPath(check);
    } else {
      QFont numFont = this->font();
      // 字号随窗口字体缩放（含 pointSize 与 pixelSize 两种配置），高 DPI 下不脱节
      const qreal basePoint = this->font().pointSizeF();
      const int basePixel = this->font().pixelSize();
      if (basePoint > 0) {
        numFont.setPointSizeF(basePoint + 1.0);
      } else if (basePixel > 0) {
        numFont.setPixelSize(basePixel + 1);
      }
      numFont.setWeight(QFont::DemiBold);
      p.setFont(numFont);
      p.drawText(circleRect, Qt::AlignCenter, QString::number(i + 1));
      p.setFont(labelFont);
    }

    // 标签：宽度限制在本列剩余空间，超长省略号截断，避免溢出到下一列
    const int labelX = cx + circleR + 6;
    const int labelW = qMax(0, static_cast<int>(colW - (labelX - colX)));
    const QRect labelRect(labelX, cy - fm.height() / 2, labelW, fm.height());
    p.setPen(active || completed ? c.labelActive : c.circleText);
    p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(labels_.at(i), Qt::ElideRight, labelW));
  }
}

}  // namespace etest::app
