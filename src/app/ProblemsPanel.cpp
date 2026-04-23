#include "ProblemsPanel.h"
#include <QHeaderView>

ProblemsPanel::ProblemsPanel(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void ProblemsPanel::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  table_ = new QTableWidget(this);
  table_->setColumnCount(4);
  table_->setHorizontalHeaderLabels(
      {QStringLiteral("文件"), QStringLiteral("行"), QStringLiteral("描述"),
       QStringLiteral("类型")});
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  layout->addWidget(table_);
}
