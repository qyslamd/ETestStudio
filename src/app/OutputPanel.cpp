#include "OutputPanel.h"

namespace etest {
namespace app {

OutputPanel::OutputPanel(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void OutputPanel::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  text_edit_ = new QTextEdit(this);
  text_edit_->setReadOnly(true);
  text_edit_->setPlaceholderText(QStringLiteral("输出信息将显示在此处..."));
  layout->addWidget(text_edit_);
}

void OutputPanel::appendLog(const QString& text) {
  text_edit_->append(text);
}

void OutputPanel::clearLog() {
  text_edit_->clear();
}

}  // namespace app
}  // namespace etest
