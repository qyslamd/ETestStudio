#include "ProblemsPanel.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidgetItem>

namespace etest::app {

ProblemsPanel::ProblemsPanel(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

void ProblemsPanel::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  table_ = new QTableWidget(this);
  table_->setFrameShape(QFrame::NoFrame);
  table_->setColumnCount(3);
  table_->setHorizontalHeaderLabels(
      {QStringLiteral("来源"), QStringLiteral("描述"), QStringLiteral("类型")});
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  layout->addWidget(table_);
}

void ProblemsPanel::initSignals() {
  // 双击触发跳转
  connect(table_, &QTableWidget::cellDoubleClicked, this,
          &ProblemsPanel::onCellDoubleClicked);
}

void ProblemsPanel::clearProblems() {
  table_->setRowCount(0);
}

void ProblemsPanel::addProblem(NavTarget target,
                               const QString& display_source,
                               const QString& type, const QString& message) {
  int row = table_->rowCount();
  table_->insertRow(row);

  auto* source_item = new QTableWidgetItem(display_source);
  // UserRole 存 NavTarget 的 int 值，点击时取回，无字符串<->枚举往返
  source_item->setData(Qt::UserRole, static_cast<int>(target));
  table_->setItem(row, 0, source_item);
  table_->setItem(row, 1, new QTableWidgetItem(message));
  table_->setItem(row, 2, new QTableWidgetItem(type));
}

void ProblemsPanel::showSummary(int errors, int warnings) {
  QString summary = QStringLiteral("问题（%1 错误, %2 警告）")
                        .arg(errors)
                        .arg(warnings);
  table_->setHorizontalHeaderLabels(
      {QStringLiteral("来源"), summary, QStringLiteral("类型")});
}

void ProblemsPanel::onCellDoubleClicked(int row, int /*column*/) {
  if (row < 0 || row >= table_->rowCount()) {
    return;
  }
  auto* source_item = table_->item(row, 0);
  if (!source_item) {
    return;
  }
  bool ok = false;
  int value = source_item->data(Qt::UserRole).toInt(&ok);
  if (!ok) {
    return;
  }
  emit problemActivated(static_cast<NavTarget>(value));
}

}  // namespace etest::app
