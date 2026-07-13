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
  QString color = statusColor(result.status);
  QString icon = statusIcon(result.status);

  QString html = QStringLiteral(
      "<span style='color:#888888'>[%1]</span> "
      "<span style='color:%2;font-weight:bold'>%3</span> "
      "%4")
      .arg(ts.toHtmlEscaped(), color, icon,
           result.command.toHtmlEscaped());
  if (!result.target.isEmpty()) {
    html += QStringLiteral(" <span style='color:#888888'>%1</span>")
                .arg(result.target.toHtmlEscaped());
  }
  if (result.elapsedMs > 0) {
    html += QStringLiteral(" <span style='color:%1'>(%2ms)</span>")
                .arg(color)
                .arg(result.elapsedMs);
  }
  if (!result.message.isEmpty()) {
    html += QStringLiteral(" <span style='color:#aaa'>[%1]</span>")
                .arg(result.message.toHtmlEscaped());
  }

  text_edit_->append(html);
  trimToMaxLines();
  scrollToBottom();
}

void ExecutionOutputPanel::appendText(const QString& text) {
  text_edit_->append(text.toHtmlEscaped());
  trimToMaxLines();
  scrollToBottom();
}

void ExecutionOutputPanel::appendError(const QString& msg) {
  QString html = QStringLiteral(
      "<span style='color:#C62828;font-weight:bold'>[ERROR]</span> "
      "<span style='color:#C62828'>%1</span>")
      .arg(msg.toHtmlEscaped());
  text_edit_->append(html);
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

QString ExecutionOutputPanel::statusColor(
    etest::engine::StepStatus status) const {
  switch (status) {
    case etest::engine::PASS:
      return QStringLiteral("#1B7A2B");
    case etest::engine::FAIL:
      return QStringLiteral("#C62828");
    case etest::engine::TIMEOUT:
      return QStringLiteral("#BD6B00");
    case etest::engine::ERROR:
      return QStringLiteral("#C62828");
    case etest::engine::SKIPPED:
      return QStringLiteral("#999999");
    default:
      return QStringLiteral("#888888");
  }
}

}  // namespace etest::app
