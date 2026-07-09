#include "ProblemsPanel.h"
#include <QHeaderView>
#include <QLabel>

namespace etest::app {

ProblemsPanel::ProblemsPanel(QWidget* parent) : QWidget(parent) {
  initUi();
}

void ProblemsPanel::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  table_ = new QTableWidget(this);
  table_->setFrameShape(QFrame::NoFrame);
  table_->setColumnCount(4);
  table_->setHorizontalHeaderLabels(
      {QStringLiteral("文件"), QStringLiteral("行"), QStringLiteral("描述"),
       QStringLiteral("类型")});
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  layout->addWidget(table_);
}

void ProblemsPanel::clearProblems() {
  table_->setRowCount(0);
}

void ProblemsPanel::addProblem(const QString& source, const QString& type,
                                const QString& message) {
  int row = table_->rowCount();
  table_->insertRow(row);
  table_->setItem(row, 0, new QTableWidgetItem(source));
  table_->setItem(row, 1, new QTableWidgetItem(QString()));
  table_->setItem(row, 2, new QTableWidgetItem(message));
  table_->setItem(row, 3, new QTableWidgetItem(type));
}

void ProblemsPanel::showSummary(int errors, int warnings) {
  // Update the table header to show summary counts
  QString summary = QStringLiteral("问题（%1 错误, %2 警告）")
                        .arg(errors)
                        .arg(warnings);
  table_->setHorizontalHeaderLabels(
      {QStringLiteral("文件"), QStringLiteral("行"), summary,
       QStringLiteral("类型")});
}

}  // namespace etest::app
