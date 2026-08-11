#include "guidance_homepage.h"

#include <QCheckBox>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPushButton>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include "AppIconProvider.h"
#include "ThemeManager.h"
#include "guidance_config.h"
#include "guidance_controller.h"

namespace etest::app {

using etest::core_ui::ThemeManager;
using etest::core_ui::AppIconProvider;

// ---------------------------------------------------------------------------
// GuidanceCard
// ---------------------------------------------------------------------------

GuidanceCard::GuidanceCard(GuidanceFlow* flow, QWidget* parent)
    : QWidget(parent),
      flow_(flow),
      placeholder_(flow == nullptr || flow->stepCount() == 0) {
  initUi();
}

void GuidanceCard::initUi() {
  setObjectName(QStringLiteral("guidanceCard"));
  setAttribute(Qt::WA_TranslucentBackground);
  // 让 QSS（#guidanceCard 背景）真正生效，否则卡片常态无底色。
  setAttribute(Qt::WA_StyledBackground, true);
  setCursor(Qt::PointingHandCursor);
  setMinimumSize(150, 170);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(8);

  icon_label_ = new QLabel(this);
  icon_label_->setObjectName(QStringLiteral("guidanceCardIcon"));
  icon_label_->setAlignment(Qt::AlignCenter);
  if (flow_ != nullptr && !flow_->icon().isNull()) {
    icon_label_->setPixmap(flow_->icon().scaled(
        QSize(48, 48), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  layout->addWidget(icon_label_);

  title_label_ = new QLabel(flow_ != nullptr ? flow_->name() : QString(), this);
  title_label_->setObjectName(QStringLiteral("guidanceCardTitle"));
  title_label_->setAlignment(Qt::AlignCenter);
  title_label_->setWordWrap(true);
  layout->addWidget(title_label_);

  desc_label_ = new QLabel(
      placeholder_ ? QStringLiteral("敬请期待")
                   : (flow_ != nullptr ? flow_->description() : QString()),
      this);
  desc_label_->setObjectName(QStringLiteral("guidanceCardDesc"));
  desc_label_->setAlignment(Qt::AlignCenter);
  desc_label_->setWordWrap(true);
  layout->addWidget(desc_label_);
}

void GuidanceCard::setSelected(bool selected) {
  if (selected_ != selected) {
    selected_ = selected;
    update();
  }
}

void GuidanceCard::enterEvent(QEvent* event) {
  hovered_ = true;
  update();
  QWidget::enterEvent(event);
}

void GuidanceCard::leaveEvent(QEvent* event) {
  hovered_ = false;
  update();
  QWidget::leaveEvent(event);
}

void GuidanceCard::paintEvent(QPaintEvent* event) {
  // 先让 QSS（#guidanceCard）绘制背景，再自绘圆角边框兜底。
  QWidget::paintEvent(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QPainterPath path;
  path.addRoundedRect(cardRect, 8, 8);

  if (selected_) {
    p.fillPath(path, ThemeManager::instance().selectionBackground());
  } else if (hovered_) {
    p.fillPath(path, ThemeManager::instance().hoverBackground());
  }

  QColor border = ThemeManager::instance().borderColor();
  int width = 1;
  if (selected_) {
    border = ThemeManager::instance().accentColor();
    width = 2;
  } else if (placeholder_) {
    border = ThemeManager::instance().disabledTextColor();
  }
  p.setPen(QPen(border, width));
  p.drawPath(path);
}

void GuidanceCard::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    pressed_ = true;
    update();
    return;
  }
  QWidget::mousePressEvent(event);
}

void GuidanceCard::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && pressed_) {
    pressed_ = false;
    update();
    if (!placeholder_ && rect().contains(event->pos())) {
      emit clicked();
    }
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

// ---------------------------------------------------------------------------
// GuidanceHomePage
// ---------------------------------------------------------------------------

GuidanceHomePage::GuidanceHomePage(GuidanceController* controller,
                                   QWidget* parent)
    : OverlayDialog(parent), controller_(controller) {
  // 引导聚焦遮罩：黑色半透明。遮罩绘制/圆角/阴影/覆盖父窗口全部由 OverlayDialog 承担。
  setMaskColor(QColor(0, 0, 0, 128));
  initUi();
  initSignals();
}

void GuidanceHomePage::initUi() {
  // 居中面板：固定尺寸，QSS #guidanceHomePanel 可覆盖背景（半透明 + 圆角）；
  // 无 QSS 时用 panelBackground token 兜底。阴影由 OverlayDialog::setWidget 添加。
  panel_ = new QFrame(this);
  panel_->setObjectName(QStringLiteral("guidanceHomePanel"));
  panel_->setFixedSize(880, 520);
  panel_->setAttribute(Qt::WA_StyledBackground);
  // 背景由 QSS（#guidanceHomePanel：半透明 + 圆角）渲染，不 autoFillBackground，
  // 否则 palette 矩形会盖住 QSS 圆角。
  QPalette pal = panel_->palette();
  pal.setColor(QPalette::Window, ThemeManager::instance().panelBackground());
  panel_->setPalette(pal);

  auto* panel_layout = new QVBoxLayout(panel_);
  panel_layout->setContentsMargins(28, 20, 28, 20);
  panel_layout->setSpacing(16);

  // 标题行：居中主标题 + 副标题，右上角关闭 ×（对齐原型 guide-home）。
  auto* header_layout = new QHBoxLayout;
  header_layout->setContentsMargins(0, 0, 0, 0);
  header_layout->addStretch(1);

  auto* title_box = new QVBoxLayout;
  title_box->setSpacing(4);
  title_box->setAlignment(Qt::AlignCenter);

  title_label_ = new QLabel(QStringLiteral("欢迎使用 ETest Studio"), panel_);
  title_label_->setObjectName(QStringLiteral("guidanceHomeTitle"));
  title_label_->setAlignment(Qt::AlignCenter);
  QFont title_font = title_label_->font();
  title_font.setPointSize(20);
  title_font.setBold(true);
  title_label_->setFont(title_font);
  title_box->addWidget(title_label_);

  sub_label_ = new QLabel(QStringLiteral("选择一项开始了解"), panel_);
  sub_label_->setObjectName(QStringLiteral("guidanceHomeSub"));
  sub_label_->setAlignment(Qt::AlignCenter);
  title_box->addWidget(sub_label_);

  header_layout->addLayout(title_box);
  header_layout->addStretch(1);

  close_btn_ = new QToolButton(panel_);
  close_btn_->setObjectName(QStringLiteral("guidanceHomeClose"));
  close_btn_->setIcon(AppIconProvider::instance().icon("close"));
  close_btn_->setIconSize(QSize(16, 16));
  close_btn_->setCursor(Qt::PointingHandCursor);
  header_layout->addWidget(close_btn_);

  panel_layout->addLayout(header_layout);

  // 卡片行：居中，rebuildCards() 按 controller_->flows() 往里填卡。
  cards_layout_ = new QHBoxLayout;
  cards_layout_->setSpacing(16);
  cards_layout_->setAlignment(Qt::AlignCenter);
  panel_layout->addLayout(cards_layout_);

  // 底部操作行：自动演示勾选（checkBoxAuto 语义保留）+ 全部演示按钮。
  auto* bottom_layout = new QHBoxLayout;
  bottom_layout->setContentsMargins(0, 0, 0, 0);
  bottom_layout->setSpacing(16);

  auto_check_ = new QCheckBox(QStringLiteral("自动演示"), panel_);
  auto_check_->setObjectName(QStringLiteral("guidanceAutoCheck"));
  auto_check_->setLayoutDirection(Qt::RightToLeft);
  bottom_layout->addWidget(auto_check_);

  bottom_layout->addStretch(1);

  all_btn_ = new QPushButton(QStringLiteral("全部演示"), panel_);
  all_btn_->setObjectName(QStringLiteral("guidanceAllBtn"));
  bottom_layout->addWidget(all_btn_);

  panel_layout->addLayout(bottom_layout);

  setWidget(panel_);
}

void GuidanceHomePage::initSignals() {
  connect(close_btn_, &QToolButton::clicked, this,
          &GuidanceHomePage::hideHomePage);
  connect(all_btn_, &QPushButton::clicked, this, [this] {
    hideHomePage();
    if (controller_ != nullptr) {
      controller_->startAll(auto_check_->isChecked());
    }
  });
}

void GuidanceHomePage::rebuildCards() {
  while (QLayoutItem* item = cards_layout_->takeAt(0)) {
    if (QWidget* w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }

  if (controller_ == nullptr) {
    return;
  }
  for (GuidanceFlow* flow : controller_->flows()) {
    auto* card = new GuidanceCard(flow, this);
    connect(card, &GuidanceCard::clicked, this,
            [this, flow] { startFlow(flow); });
    cards_layout_->addWidget(card);
  }
}

void GuidanceHomePage::startFlow(GuidanceFlow* flow) {
  if (flow == nullptr || flow->stepCount() == 0) {
    return;
  }
  hideHomePage();
  if (controller_ != nullptr) {
    controller_->startOne(flow, auto_check_->isChecked());
  }
}

void GuidanceHomePage::hideHomePage() {
  // 模态 exec() 下，hide() 会触发 QDialog 事件循环退出，exec() 随之返回。
  hide();
}

void GuidanceHomePage::showEvent(QShowEvent* event) {
  // 每次显示前重建卡片（flows 可能已变）；面板居中由基类 showEvent 完成。
  rebuildCards();
  OverlayDialog::showEvent(event);
}

}  // namespace etest::app
