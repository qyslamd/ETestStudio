#include "RecentProjectCard.h"

#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace etest::app {

RecentProjectCard::RecentProjectCard(const QString& projectPath,
                                     const QString& displayName,
                                     const QString& dirPath,
                                     const QString& timeStr,
                                     QWidget* parent)
    : QWidget(parent), project_path_(projectPath) {
  initUi(displayName, dirPath, timeStr);
  initSignals();
}

void RecentProjectCard::setTimeStr(const QString& timeStr) {
  if (!time_label_) return;
  if (timeStr.isEmpty()) {
    time_label_->hide();
  } else {
    time_label_->setText(timeStr);
    time_label_->show();
  }
}

void RecentProjectCard::setRemoveActionText(const QString& text) {
  remove_action_text_ = text;
}

void RecentProjectCard::initUi(const QString& displayName,
                               const QString& dirPath,
                               const QString& timeStr) {
  setObjectName(QStringLiteral("WelcomeProjectCardContent"));
  setCursor(Qt::PointingHandCursor);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 8, 10, 8);
  layout->setSpacing(2);

  name_label_ = new QLabel(displayName, this);
  name_label_->setObjectName(QStringLiteral("WelcomeRecentCardName"));
  name_label_->setWordWrap(true);
  layout->addWidget(name_label_);

  path_label_ = new QLabel(dirPath, this);
  path_label_->setObjectName(QStringLiteral("WelcomeRecentCardPath"));
  path_label_->setWordWrap(true);
  layout->addWidget(path_label_);

  time_label_ = new QLabel(timeStr, this);
  time_label_->setObjectName(QStringLiteral("WelcomeRecentCardTime"));
  if (timeStr.isEmpty()) time_label_->hide();
  layout->addWidget(time_label_);
}

void RecentProjectCard::initSignals() {
  // 信号在事件处理器中发射，此处无额外连接
}

void RecentProjectCard::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    emit openRequested(project_path_);
    return;
  }
  QWidget::mousePressEvent(event);
}

void RecentProjectCard::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);
  menu.setObjectName(QStringLiteral("PhRecentContextMenu"));
  auto* removeAction = menu.addAction(remove_action_text_);
  if (menu.exec(event->globalPos()) == removeAction) {
    emit removeRequested(project_path_);
  }
}

}  // namespace etest::app
