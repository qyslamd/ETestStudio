#include "OutputPanel.h"

#include <spdlog/spdlog.h>
#include <QScrollBar>

#include "logger/Logger.h"

namespace etest::app {

OutputPanel::OutputPanel(QWidget* parent) : QWidget(parent) {
  setupUi();

  // 拉取 Logger::init() 之后到本构造之间的历史日志。
  // drain() 在 connect 之后同步触发 onHistoricalLogs 槽，槽内循环 appendLog
  // 把历史追加到 QTextEdit。main_window.cpp 中 connect 实时 logMessage
  // 在本构造之后进行，所以历史天然先于实时。
  if (auto* hist = etest::core::logger::Logger::qtHistoryBuffer()) {
    connect(hist, &etest::core::logger::LogHistoryBuffer::drained, this,
            &OutputPanel::onHistoricalLogs);
    hist->drain(this);
  }
}

void OutputPanel::onHistoricalLogs(
    const QList<etest::core::logger::LogEntry>& entries) {
  for (const auto& entry : entries) {
    appendLog(entry.level, entry.text);
  }
}

void OutputPanel::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  text_edit_ = new QTextEdit(this);
  text_edit_->setReadOnly(true);
  text_edit_->setPlaceholderText(QStringLiteral("输出信息将显示在此处..."));
  layout->addWidget(text_edit_);
}

void OutputPanel::appendLog(int level, const QString& text) {
  QString color = levelColor(level);
  QString escaped = text.toHtmlEscaped();
  QString html =
      QStringLiteral("<span style=\"color:%1\">%2</span>").arg(color, escaped);
  text_edit_->append(html);

  // 行数限制
  QTextDocument* doc = text_edit_->document();
  if (doc->blockCount() > kMaxLines) {
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
                        doc->blockCount() - kMaxLines);
    cursor.removeSelectedText();
  }

  // 自动滚动到底部
  QScrollBar* bar = text_edit_->verticalScrollBar();
  bar->setValue(bar->maximum());
}

void OutputPanel::clearLog() {
  text_edit_->clear();
}

QString OutputPanel::levelColor(int level) const {
  switch (level) {
    case spdlog::level::debug:
      return QStringLiteral("#858585");
    case spdlog::level::info:
      return QStringLiteral("#CCCCCC");
    case spdlog::level::warn:
      return QStringLiteral("#D7BA7D");
    case spdlog::level::err:
      return QStringLiteral("#F44747");
    case spdlog::level::critical:
      return QStringLiteral("#FF0000");
    default:
      return QStringLiteral("#CCCCCC");
  }
}

QString OutputPanel::levelName(int level) const {
  switch (level) {
    case spdlog::level::debug:
      return QStringLiteral("DEBUG");
    case spdlog::level::info:
      return QStringLiteral("INFO");
    case spdlog::level::warn:
      return QStringLiteral("WARN");
    case spdlog::level::err:
      return QStringLiteral("ERROR");
    case spdlog::level::critical:
      return QStringLiteral("FATAL");
    default:
      return QStringLiteral("UNKNOWN");
  }
}

}  // namespace etest::app
