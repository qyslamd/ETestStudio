#include "ExecutionOutputPanel.h"

#include <QScrollBar>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace etest::app {

ExecutionOutputPanel::ExecutionOutputPanel(QWidget* parent) : QWidget(parent) {
  initUi();
}

void ExecutionOutputPanel::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  text_edit_ = new QTextEdit(this);
  text_edit_->setReadOnly(true);
  text_edit_->setFrameShape(QFrame::NoFrame);
  QFont monoFont(QStringLiteral("Consolas"));
  monoFont.setStyleHint(QFont::Monospace);
  text_edit_->setFont(monoFont);
  layout->addWidget(text_edit_);
}

void ExecutionOutputPanel::appendResult(
    const etest::engine::StepResult& result) {
  QString ts =
      result.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"));
  QString icon = statusIcon(result.status);
  QString line = QStringLiteral("[%1] %2  %3 %4")
                     .arg(ts, result.stepPath, icon, result.command);
  if (!result.target.isEmpty()) {
    line += QStringLiteral(" %1").arg(result.target);
  }
  if (result.elapsedMs > 0) {
    line += QStringLiteral(" (%1ms)").arg(result.elapsedMs);
  }
  if (!result.message.isEmpty()) {
    line += QStringLiteral(" [%1]").arg(result.message);
  }
  text_edit_->append(line);
  trimToMaxLines();
  scrollToBottom();
}

void ExecutionOutputPanel::appendText(const QString& text) {
  text_edit_->append(text);
  trimToMaxLines();
  scrollToBottom();
}

void ExecutionOutputPanel::clearOutput() {
  text_edit_->clear();
}

void ExecutionOutputPanel::trimToMaxLines() {
  QTextDocument* doc = text_edit_->document();
  if (doc->blockCount() > kMaxLines) {
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
                        doc->blockCount() - kMaxLines);
    cursor.removeSelectedText();
  }
}

void ExecutionOutputPanel::scrollToBottom() {
  QScrollBar* bar = text_edit_->verticalScrollBar();
  bar->setValue(bar->maximum());
}

QString ExecutionOutputPanel::statusIcon(
    etest::engine::StepStatus status) const {
  switch (status) {
    case etest::engine::PASS:
      return QStringLiteral("[PASS]");
    case etest::engine::FAIL:
      return QStringLiteral("[FAIL]");
    case etest::engine::TIMEOUT:
      return QStringLiteral("[TIMEOUT]");
    case etest::engine::ERROR:
      return QStringLiteral("[ERROR]");
    case etest::engine::SKIPPED:
      return QStringLiteral("[SKIP]");
    default:
      return QStringLiteral("[..]");
  }
}

}  // namespace etest::app
