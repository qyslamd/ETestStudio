#include "WizardTemplateCard.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QStyle>
#include <QStyleOptionButton>
#include <QVBoxLayout>

#include "AppIconProvider.h"

namespace etest::app {

WizardTemplateCard::WizardTemplateCard(const QString& iconName,
                                       const QString& title, const QString& desc,
                                       const QString& badge, QWidget* parent)
    : QAbstractButton(parent) {
  setCursor(Qt::PointingHandCursor);
  setCheckable(true);
  setObjectName(QStringLiteral("templateCard"));

  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(16, 16, 16, 14);
  lay->setSpacing(6);

  auto* icon = new QLabel(this);
  icon->setObjectName(QStringLiteral("templateCardIcon"));
  icon->setPixmap(
      core_ui::AppIconProvider::instance().icon(iconName).pixmap(28, 28));
  lay->addWidget(icon);

  auto* titleLabel = new QLabel(title, this);
  titleLabel->setObjectName(QStringLiteral("templateCardTitle"));
  lay->addWidget(titleLabel);

  auto* descLabel = new QLabel(desc, this);
  descLabel->setObjectName(QStringLiteral("templateCardDesc"));
  descLabel->setWordWrap(true);
  lay->addWidget(descLabel);

  if (!badge.isEmpty()) {
    auto* badgeLabel = new QLabel(badge, this);
    badgeLabel->setObjectName(QStringLiteral("templateCardBadge"));
    lay->addWidget(badgeLabel);
  }
  lay->addStretch();
}

void WizardTemplateCard::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QStyleOptionButton opt;
  opt.initFrom(this);
  if (isChecked()) {
    opt.state |= QStyle::State_On;
  }
  QPainter p(this);
  style()->drawControl(QStyle::CE_PushButton, &opt, &p, this);
}

}  // namespace etest::app
