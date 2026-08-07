#include "BaseWizardDialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include "AppIconProvider.h"
#include "utils/WizardStepBar.h"

namespace etest::app {

WizardPage::WizardPage(QWidget* parent) : QWidget(parent) {}

// 淡入上滑转场绘制层：覆盖在页面栈之上，旧页原地淡出、新页自下方 8px 上滑淡入。
class BaseWizardDialog::PageTransitionOverlay : public QWidget {
 public:
  explicit PageTransitionOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
  }

  void setPixmaps(const QPixmap& oldPm, const QPixmap& newPm) {
    old_pixmap_ = oldPm;
    new_pixmap_ = newPm;
    update();
  }

  void setProgress(qreal progress) {
    progress_ = progress;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const qreal t = qBound<qreal>(0.0, progress_, 1.0);
    if (!old_pixmap_.isNull()) {
      p.setOpacity(1.0 - t);
      p.drawPixmap(0, 0, old_pixmap_);
    }
    if (!new_pixmap_.isNull()) {
      p.setOpacity(t);
      p.drawPixmap(0, 8.0 * (1.0 - t), new_pixmap_);
    }
  }

 private:
  QPixmap old_pixmap_;
  QPixmap new_pixmap_;
  qreal progress_ = 0.0;
};

BaseWizardDialog::BaseWizardDialog(QWidget* parent) : AnimationDialog(parent) {
  initUi();
  initSignals();
}

int BaseWizardDialog::addPage(WizardPage* page) {
  const int index = page_stack_->addWidget(page);
  pages_.append(page);
  // 页面输入变化时刷新导航按钮
  connect(page, &WizardPage::completeChanged, this,
          &BaseWizardDialog::updateNavButtons);
  rebuildStepBar();
  updateNavButtons();
  return index;
}

void BaseWizardDialog::setCurrentPage(int index) {
  if (index < 0 || index >= pages_.size()) {
    return;
  }
  if (page_transition_animation_->state() == QAbstractAnimation::Running) {
    return;
  }
  page_stack_->setCurrentIndex(index);
  updateNavButtons();
  emit currentPageChanged(index);
}

int BaseWizardDialog::currentPageIndex() const {
  return page_stack_->currentIndex();
}

int BaseWizardDialog::pageCount() const {
  return pages_.size();
}

void BaseWizardDialog::next() {
  const int cur = currentPageIndex();
  if (cur < 0 || cur >= pageCount() - 1) {
    return;
  }
  if (!pages_.at(cur)->validatePage()) {
    return;
  }
  animatePageChange(cur, cur + 1);
}

void BaseWizardDialog::back() {
  const int cur = currentPageIndex();
  if (cur <= 0) {
    return;
  }
  animatePageChange(cur, cur - 1);
}

void BaseWizardDialog::setHeader(const QString& iconName, const QString& title,
                                 const QString& subtitle) {
  header_icon_label_->setPixmap(
      core_ui::AppIconProvider::instance().icon(iconName).pixmap(22, 22));
  header_title_label_->setText(title);
  header_subtitle_label_->setText(subtitle);
  header_icon_label_->show();
  header_title_label_->show();
  header_subtitle_label_->show();
}

void BaseWizardDialog::setCreateButtonText(const QString& text) {
  create_btn_->setText(text);
}

void BaseWizardDialog::animatePageChange(int fromIndex, int toIndex) {
  if (fromIndex == toIndex ||
      page_transition_animation_->state() == QAbstractAnimation::Running) {
    return;
  }

  transition_old_pixmap_ = page_stack_->grab();
  page_stack_->setCurrentIndex(toIndex);
  transition_new_pixmap_ = page_stack_->grab();
  transition_to_ = toIndex;
  // 转场一开始就刷新步骤条，避免 300ms 内步骤条滞后旧页
  if (step_bar_) {
    step_bar_->setCurrentStep(toIndex);
  }

  transition_overlay_->setGeometry(page_stack_->geometry());
  transition_overlay_->setPixmaps(transition_old_pixmap_,
                                  transition_new_pixmap_);
  transition_overlay_->setProgress(0.0);
  transition_overlay_->show();
  transition_overlay_->raise();

  page_transition_animation_->setStartValue(0.0);
  page_transition_animation_->setEndValue(1.0);
  page_transition_animation_->setDuration(300);
  page_transition_animation_->setEasingCurve(QEasingCurve::OutCubic);
  page_transition_animation_->start();
}

void BaseWizardDialog::initUi() {
  setWindowTitle(QStringLiteral("向导"));

  auto* contentWidget = new QWidget(this);
  contentWidget->setObjectName(QStringLiteral("wizardCard"));
  contentWidget->setMinimumWidth(760);

  auto* mainLayout = new QVBoxLayout(contentWidget);
  mainLayout->setContentsMargins(32, 28, 32, 24);
  mainLayout->setSpacing(16);

  // 页头：图标盒 + 标题 + 副标题（默认隐藏，由 setHeader 显示）
  auto* headerLayout = new QHBoxLayout();
  headerLayout->setSpacing(14);
  header_icon_label_ = new QLabel(contentWidget);
  header_icon_label_->setObjectName(QStringLiteral("wizardHeaderIcon"));
  header_icon_label_->setFixedSize(44, 44);
  header_icon_label_->setAlignment(Qt::AlignCenter);
  header_icon_label_->hide();

  auto* titleGroup = new QVBoxLayout();
  titleGroup->setSpacing(2);
  header_title_label_ = new QLabel(contentWidget);
  header_title_label_->setObjectName(QStringLiteral("wizardHeaderTitle"));
  header_title_label_->hide();
  header_subtitle_label_ = new QLabel(contentWidget);
  header_subtitle_label_->setObjectName(QStringLiteral("wizardHeaderSubtitle"));
  header_subtitle_label_->hide();
  titleGroup->addWidget(header_title_label_);
  titleGroup->addWidget(header_subtitle_label_);

  headerLayout->addWidget(header_icon_label_);
  headerLayout->addLayout(titleGroup, 1);
  mainLayout->addLayout(headerLayout);

  // 步骤条
  step_bar_ = new WizardStepBar(contentWidget);
  step_bar_->setObjectName(QStringLiteral("wizardStepBar"));
  mainLayout->addWidget(step_bar_);

  // 页面栈
  page_stack_ = new QStackedWidget(contentWidget);
  page_stack_->setMinimumHeight(320);
  mainLayout->addWidget(page_stack_, 1);

  // 页脚：左 上一步，右 取消 + 下一步 + 创建项目
  auto* footerLayout = new QHBoxLayout();
  footerLayout->setSpacing(8);
  back_btn_ = new QPushButton(QStringLiteral("上一步"), contentWidget);
  back_btn_->setObjectName(QStringLiteral("wizardBackBtn"));
  back_btn_->setIcon(
      core_ui::AppIconProvider::instance().icon(QStringLiteral("chevron_left")));
  cancel_btn_ = new QPushButton(QStringLiteral("取消"), contentWidget);
  cancel_btn_->setObjectName(QStringLiteral("wizardCancelBtn"));
  next_btn_ = new QPushButton(QStringLiteral("下一步"), contentWidget);
  next_btn_->setObjectName(QStringLiteral("wizardNextBtn"));
  next_btn_->setIcon(core_ui::AppIconProvider::instance().icon(
      QStringLiteral("chevron_right_light")));
  create_btn_ = new QPushButton(QStringLiteral("创建项目"), contentWidget);
  create_btn_->setObjectName(QStringLiteral("wizardCreateBtn"));
  create_btn_->setIcon(
      core_ui::AppIconProvider::instance().icon(QStringLiteral("check_light")));

  footerLayout->addWidget(back_btn_);
  footerLayout->addStretch();
  footerLayout->addWidget(cancel_btn_);
  footerLayout->addWidget(next_btn_);
  footerLayout->addWidget(create_btn_);
  mainLayout->addLayout(footerLayout);

  setWidget(contentWidget);

  // 转场绘制层置于内容区之上、覆盖页面栈区域（不参与布局）
  transition_overlay_ = new PageTransitionOverlay(widget_);
  transition_overlay_->hide();
}

void BaseWizardDialog::initSignals() {
  connect(back_btn_, &QPushButton::clicked, this, &BaseWizardDialog::back);
  connect(next_btn_, &QPushButton::clicked, this, &BaseWizardDialog::next);
  // 创建前先过 onCreateValidate（子类校验），返回 false 则不 accept
  connect(create_btn_, &QPushButton::clicked, this, [this]() {
    if (onCreateValidate()) {
      accept();
    }
  });
  connect(cancel_btn_, &QPushButton::clicked, this,
          &BaseWizardDialog::confirmCancel);

  page_transition_animation_ = new QVariantAnimation(this);
  connect(page_transition_animation_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            transition_overlay_->setProgress(value.toReal());
          });
  connect(page_transition_animation_, &QVariantAnimation::finished, this,
          [this]() {
            transition_overlay_->hide();
            transition_old_pixmap_ = QPixmap();
            transition_new_pixmap_ = QPixmap();
            page_stack_->setCurrentIndex(transition_to_);
            updateNavButtons();
            emit currentPageChanged(transition_to_);
          });
}

void BaseWizardDialog::updateNavButtons() {
  const int index = currentPageIndex();
  const int count = pageCount();
  const bool isFirst = (index <= 0);
  const bool isLast = (index >= count - 1);

  back_btn_->setEnabled(!isFirst);
  next_btn_->setVisible(!isLast);
  create_btn_->setVisible(isLast);

  if (step_bar_) {
    step_bar_->setCurrentStep(index);
  }
}

void BaseWizardDialog::rebuildStepBar() {
  QStringList labels;
  labels.reserve(pages_.size());
  for (const WizardPage* page : pages_) {
    labels << page->stepLabel();
  }
  step_bar_->setLabels(labels);
}

void BaseWizardDialog::keyPressEvent(QKeyEvent* event) {
  switch (event->key()) {
    case Qt::Key_Escape:
      confirmCancel();
      event->accept();
      return;
    case Qt::Key_Left:
      back();
      event->accept();
      return;
    case Qt::Key_Right:
      next();
      event->accept();
      return;
    default:
      break;
  }
  AnimationDialog::keyPressEvent(event);
}

// Enter 在按下的瞬间常被焦点控件（如 QLineEdit）消费，按下默认按钮的机制
// 在此链路下并不可靠，改在释放时兜底触发；焦点在按钮上时 Enter 已由按钮
// 自身的 keyPressEvent 处理（点击），这里跳过避免二次触发。
void BaseWizardDialog::keyReleaseEvent(QKeyEvent* event) {
  if (!event->isAutoRepeat() &&
      (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
    if (!qobject_cast<QAbstractButton*>(QApplication::focusWidget())) {
      if (currentPageIndex() >= pageCount() - 1) {
        create_btn_->click();
      } else {
        next();
      }
      event->accept();
      return;
    }
  }
  AnimationDialog::keyReleaseEvent(event);
}

void BaseWizardDialog::closeEvent(QCloseEvent* event) {
  // Alt+F4 / 系统关闭同样走取消二次确认，与 Esc 行为一致
  if (requestCancelConfirmation()) {
    event->accept();
  } else {
    event->ignore();
  }
}

bool BaseWizardDialog::requestCancelConfirmation() {
  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("取消操作"),
      QStringLiteral("确定要取消吗？已填写的内容将丢失。"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  return reply == QMessageBox::Yes;
}

void BaseWizardDialog::confirmCancel() {
  if (requestCancelConfirmation()) {
    reject();
  }
}

}  // namespace etest::app
