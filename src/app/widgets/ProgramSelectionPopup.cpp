#include "ProgramSelectionPopup.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "AppIconProvider.h"
#include "ProjectInfo.h"
#include "ProjectManager.h"
#include "logger/Logger.h"

namespace etest::app {

ProgramSelectionPopup::ProgramSelectionPopup(QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  button_ = new QToolButton(this);
  button_->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("testprogram")));
  button_->setIconSize(QSize(16, 16));
  button_->setText(QStringLiteral("程序选择"));
  button_->setToolTip(QStringLiteral("选择要运行的测试程序"));
  button_->setPopupMode(QToolButton::InstantPopup);
  button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  // button_->setMinimumWidth(120);

  menu_ = new QMenu(this);
  list_widget_ = new QListWidget(menu_);
  // 让菜单匹配列表宽度
  list_widget_->setMinimumWidth(250);
  list_widget_->setMaximumHeight(300);

  connect(list_widget_, &QListWidget::itemChanged, this,
          &ProgramSelectionPopup::onItemChanged);
  // 菜单弹出前刷新列表
  connect(menu_, &QMenu::aboutToShow, this,
          &ProgramSelectionPopup::refreshList);

  // QWidgetAction 将 list widget 嵌入 menu
  auto* widget_action = new QWidgetAction(menu_);
  widget_action->setDefaultWidget(list_widget_);
  menu_->addAction(widget_action);

  button_->setMenu(menu_);
  layout->addWidget(button_);
}

QStringList ProgramSelectionPopup::selectedPaths() const {
  QStringList paths;
  for (int i = 0; i < list_widget_->count(); ++i) {
    auto* item = list_widget_->item(i);
    if (item->checkState() == Qt::Checked) {
      QString path = item->data(Qt::UserRole).toString();
      if (!path.isEmpty()) {
        paths.append(path);
      }
    }
  }
  return paths;
}

bool ProgramSelectionPopup::hasAnyProgram() const {
  return list_widget_->count() > 0;
}

int ProgramSelectionPopup::selectedCount() const {
  int count = 0;
  for (int i = 0; i < list_widget_->count(); ++i) {
    if (list_widget_->item(i)->checkState() == Qt::Checked) {
      ++count;
    }
  }
  return count;
}

void ProgramSelectionPopup::refreshList() {
  list_widget_->blockSignals(true);
  list_widget_->clear();
  all_paths_.clear();

  scanPrograms();

  for (const QString& path : all_paths_) {
    auto* item =
        new QListWidgetItem(QFileInfo(path).completeBaseName(), list_widget_);
    item->setData(Qt::UserRole, path);
    item->setToolTip(path);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(selected_.contains(path) ? Qt::Checked : Qt::Unchecked);
  }

  // 修剪已删除文件的选中状态
  QSet<QString> current = QSet<QString>::fromList(all_paths_);
  selected_.intersect(current);

  list_widget_->blockSignals(false);
  updateButtonText();
}

void ProgramSelectionPopup::scanPrograms() {
  auto& pm = etest::core::project::ProjectManager::instance();
  if (!pm.isProjectOpen()) {
    return;
  }

  auto* project = pm.currentProject();
  if (!project) {
    return;
  }

  all_paths_ =
      project->scanDirectory(QStringLiteral("cases"), QStringLiteral("etprog"));
}

void ProgramSelectionPopup::onItemChanged(QListWidgetItem* item) {
  if (!item) {
    return;
  }
  QString path = item->data(Qt::UserRole).toString();
  if (item->checkState() == Qt::Checked) {
    selected_.insert(path);
  } else {
    selected_.remove(path);
  }
  updateButtonText();
  emit selectionChanged();
}

void ProgramSelectionPopup::updateButtonText() {
  int n = selectedCount();
  if (n > 0) {
    button_->setText(QStringLiteral("已选 %1 个").arg(n));
  } else {
    button_->setText(QStringLiteral("程序选择"));
  }
}

}  // namespace etest::app
