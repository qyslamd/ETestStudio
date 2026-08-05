#include "ProgramChecklistWidget.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDir>
#include <QListView>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace etest::app {

namespace {
constexpr int kProgramRole = Qt::UserRole + 1;  // item 存的相对项目根路径
}  // namespace

ProgramChecklistWidget::ProgramChecklistWidget(QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  select_all_ = new QCheckBox(QStringLiteral("全选"), this);
  select_all_->setTristate(true);
  layout->addWidget(select_all_);

  model_ = new QStandardItemModel(this);
  list_view_ = new QListView(this);
  list_view_->setModel(model_);
  list_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(list_view_, 1);

  // 全选：点击时按"当前未全选则全选，否则全不选"切换（不落入 tristate 循环）
  connect(select_all_, &QCheckBox::clicked, this, [this]() {
    const bool target = selectedPrograms().size() != model_->rowCount();
    updating_ = true;
    for (int i = 0; i < model_->rowCount(); ++i) {
      model_->item(i)->setCheckState(target ? Qt::Checked : Qt::Unchecked);
    }
    updating_ = false;
    updateSelectAllState();
    emit programsChanged();
  });

  // 单项勾选变化 → 刷新全选态 + 通知宿主
  connect(model_, &QStandardItemModel::itemChanged, this,
          [this](QStandardItem* item) {
            Q_UNUSED(item);
            if (updating_) {
              return;
            }
            updateSelectAllState();
            emit programsChanged();
          });
}

void ProgramChecklistWidget::setProjectRoot(const QString& root) {
  if (root == project_root_) {
    return;
  }
  project_root_ = root;
  refreshList();
}

void ProgramChecklistWidget::setSelectedPrograms(const QStringList& paths) {
  updating_ = true;
  for (int i = 0; i < model_->rowCount(); ++i) {
    auto* item = model_->item(i);
    const QString rel = item->data(kProgramRole).toString();
    item->setCheckState(paths.contains(rel) ? Qt::Checked : Qt::Unchecked);
  }
  updating_ = false;
  updateSelectAllState();
}

QStringList ProgramChecklistWidget::selectedPrograms() const {
  QStringList result;
  for (int i = 0; i < model_->rowCount(); ++i) {
    const auto* item = model_->item(i);
    if (item->checkState() == Qt::Checked) {
      result.append(item->data(kProgramRole).toString());
    }
  }
  return result;
}

void ProgramChecklistWidget::refreshList() {
  const QStringList prev = selectedPrograms();
  updating_ = true;
  model_->clear();
  if (!project_root_.isEmpty()) {
    const QDir casesDir(project_root_ + QStringLiteral("/cases"));
    const QStringList files =
        casesDir.entryList({QStringLiteral("*.etprog")}, QDir::Files, QDir::Name);
    for (const QString& f : files) {
      auto* item = new QStandardItem(f);
      const QString rel = QStringLiteral("cases/") + f;
      item->setData(rel, kProgramRole);
      item->setCheckable(true);
      item->setCheckState(prev.contains(rel) ? Qt::Checked : Qt::Unchecked);
      model_->appendRow(item);
    }
  }
  updating_ = false;
  updateSelectAllState();
}

void ProgramChecklistWidget::updateSelectAllState() {
  int checked = 0;
  const int total = model_->rowCount();
  for (int i = 0; i < total; ++i) {
    if (model_->item(i)->checkState() == Qt::Checked) {
      ++checked;
    }
  }
  select_all_->blockSignals(true);
  if (total == 0 || checked == 0) {
    select_all_->setCheckState(Qt::Unchecked);
  } else if (checked == total) {
    select_all_->setCheckState(Qt::Checked);
  } else {
    select_all_->setCheckState(Qt::PartiallyChecked);
  }
  select_all_->blockSignals(false);
}

}  // namespace etest::app
