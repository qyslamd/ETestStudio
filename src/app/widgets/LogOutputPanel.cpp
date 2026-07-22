#include "LogOutputPanel.h"

#include <QScrollBar>
#include <QRegularExpression>
#include <QTextCursor>

#include <spdlog/spdlog.h>

#include "logger/Logger.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造
// ══════════════════════════════════════════════════════════════════════════════

LogOutputPanel::LogOutputPanel(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();

  // 拉取 Logger::init() 之后到本构造之间的历史日志。
  if (auto* hist = etest::core::logger::Logger::qtHistoryBuffer()) {
    connect(hist, &etest::core::logger::LogHistoryBuffer::drained, this,
            &LogOutputPanel::onHistoricalLogs);
    hist->drain(this);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// initUi
// ══════════════════════════════════════════════════════════════════════════════

void LogOutputPanel::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  filter_bar_ = new LogFilterBar(this);
  layout->addWidget(filter_bar_);

  text_edit_ = new QTextEdit(this);
  text_edit_->setReadOnly(true);
  text_edit_->setFrameShape(QFrame::NoFrame);
  text_edit_->setPlaceholderText(QStringLiteral("输出信息将显示在此处..."));
  layout->addWidget(text_edit_);

  // 初始筛选条件（全级别、空文本）
  current_filter_ = filter_bar_->filter();
}

// ══════════════════════════════════════════════════════════════════════════════
// initSignals
// ══════════════════════════════════════════════════════════════════════════════

void LogOutputPanel::initSignals() {
  debounce_timer_ = new QTimer(this);
  debounce_timer_->setSingleShot(true);
  debounce_timer_->setInterval(300);
  connect(debounce_timer_, &QTimer::timeout, this, &LogOutputPanel::applyFilter);

  connect(filter_bar_, &LogFilterBar::filterChanged, this,
          &LogOutputPanel::onFilterChanged);
  connect(filter_bar_, &LogFilterBar::scrollLockChanged, this,
          &LogOutputPanel::onScrollLockChanged);
  connect(filter_bar_, &LogFilterBar::clearRequested, this,
          &LogOutputPanel::clearLog);
}

// ══════════════════════════════════════════════════════════════════════════════
// appendLog - 实时日志入口
// ══════════════════════════════════════════════════════════════════════════════

void LogOutputPanel::appendLog(int level, const QString& text) {
  entries_.append(etest::core::logger::LogEntry{level, text});
  // 环形裁剪
  while (entries_.size() > kMaxEntries) {
    entries_.removeFirst();
  }

  // 命中筛选才渲染
  if (current_filter_.matches(level, text)) {
    renderEntry(entries_.last());
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// clearLog
// ══════════════════════════════════════════════════════════════════════════════

void LogOutputPanel::clearLog() {
  entries_.clear();
  text_edit_->clear();
}

// ══════════════════════════════════════════════════════════════════════════════
// 渲染
// ══════════════════════════════════════════════════════════════════════════════

void LogOutputPanel::renderEntry(const etest::core::logger::LogEntry& entry) {
  text_edit_->append(buildEntryHtml(entry));
  if (!scroll_locked_) {
    QScrollBar* bar = text_edit_->verticalScrollBar();
    bar->setValue(bar->maximum());
  }
}

QString LogOutputPanel::buildEntryHtml(
    const etest::core::logger::LogEntry& entry) const {
  QString color = levelColor(entry.level);

  // 无文本过滤或正则非法 -> 整行着色，不高亮
  if (current_filter_.text.isEmpty() ||
      (current_filter_.useRegex && !current_filter_.isValid())) {
    return QStringLiteral(
               "<span style=\"color:%1\">%2</span>")
        .arg(color, entry.text.toHtmlEscaped());
  }

  // 命中区间（基于原始 text 索引，与 matches 共用逻辑）
  QList<QPair<int, int>> ranges = current_filter_.matchRanges(entry.text);
  if (ranges.isEmpty()) {
    // 不应发生（matches 已过），兜底整行着色
    return QStringLiteral("<span style=\"color:%1\">%2</span>")
        .arg(color, entry.text.toHtmlEscaped());
  }

  // 按命中区间分段拼接：未命中段级别色，命中段高亮（黄底黑字）。
  // 区间基于原始 text 索引，转义后长度可能变化，需按原 text 切片再各自转义。
  QString html;
  int cursor = 0;
  for (const auto& r : ranges) {
    if (r.first > cursor) {
      QString seg = entry.text.mid(cursor, r.first - cursor);
      html += QStringLiteral("<span style=\"color:%1\">%2</span>")
                  .arg(color, seg.toHtmlEscaped());
    }
    QString hit = entry.text.mid(r.first, r.second - r.first);
    html += QStringLiteral(
                "<span style=\"background:#FFD700;color:#000\">%1</span>")
                .arg(hit.toHtmlEscaped());
    cursor = r.second;
  }
  if (cursor < entry.text.size()) {
    QString seg = entry.text.mid(cursor);
    html += QStringLiteral("<span style=\"color:%1\">%2</span>")
                .arg(color, seg.toHtmlEscaped());
  }
  return html;
}

QString LogOutputPanel::levelColor(int level) const {
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

// ══════════════════════════════════════════════════════════════════════════════
// applyFilter - 全量重渲
// ══════════════════════════════════════════════════════════════════════════════

void LogOutputPanel::applyFilter() {
  QScrollBar* bar = text_edit_->verticalScrollBar();
  int old_value = bar->value();
  int old_max = bar->maximum();
  bool was_at_bottom = old_max > 0 && old_value >= old_max - 2;

  // 用 QTextCursor 直接操作文档，避免 QTextEdit::append 内部的
  // ensureCursorVisible 把视图拉到底（会破坏 was_at_bottom 保留原位置的语义）。
  text_edit_->clear();
  QTextCursor cursor(text_edit_->document());
  cursor.beginEditBlock();
  bool first = true;
  for (const auto& entry : entries_) {
    if (!current_filter_.matches(entry.level, entry.text)) {
      continue;
    }
    if (!first) {
      cursor.insertBlock();
    }
    cursor.insertHtml(buildEntryHtml(entry));
    first = false;
  }
  cursor.endEditBlock();

  // 重渲是否滚底只看 was_at_bottom（与 scroll_locked_ 无关）：
  // 原本在底部 -> 跟随新内容滚底；否则按比例恢复原位置，尽量不打断阅读。
  if (was_at_bottom) {
    bar->setValue(bar->maximum());
  } else if (old_max > 0 && bar->maximum() > 0) {
    bar->setValue(static_cast<int>(bar->maximum() *
                                   (static_cast<double>(old_value) / old_max)));
  }
}

void LogOutputPanel::requestFilterApply() {
  debounce_timer_->start();
}

// ══════════════════════════════════════════════════════════════════════════════
// 槽
// ══════════════════════════════════════════════════════════════════════════════

void LogOutputPanel::onHistoricalLogs(
    const QList<etest::core::logger::LogEntry>& entries) {
  for (const auto& entry : entries) {
    entries_.append(entry);
  }
  while (entries_.size() > kMaxEntries) {
    entries_.removeFirst();
  }
  applyFilter();
}

void LogOutputPanel::onFilterChanged(const LogFilter& filter) {
  current_filter_ = filter;
  // 正则非法提示
  if (filter.useRegex && !filter.isValid()) {
    filter_bar_->setFilterInvalid(true, filter.errorString());
  } else {
    filter_bar_->setFilterInvalid(false);
  }
  requestFilterApply();
}

void LogOutputPanel::onScrollLockChanged(bool locked) {
  scroll_locked_ = locked;
}

}  // namespace etest::app
