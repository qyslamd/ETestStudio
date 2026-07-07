#include "ProblemsPanel.h"
#include <QHeaderView>

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

}  // namespace etest::app
