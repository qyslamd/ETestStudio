#ifndef ETEST_APP_LOG_OUTPUT_PANEL_H_
#define ETEST_APP_LOG_OUTPUT_PANEL_H_

#include <QList>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "LogFilter.h"
#include "LogFilterBar.h"
#include "logger/LogHistoryBuffer.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// LogOutputPanel - 应用日志输出面板（page0 底部「输出」）
// ══════════════════════════════════════════════════════════════════════════════
// 数据流：QtConsoleSink::logMessage 信号 -> appendLog -> entries_ 缓存 -> 按筛选渲染。
// entries_ 为唯一数据源（5000 环形），筛选条件变化时 applyFilter 全量重渲。
class LogOutputPanel : public QWidget {
  Q_OBJECT

 public:
  explicit LogOutputPanel(QWidget* parent = nullptr);

  /// 实时日志入口（MainWindow connect QtConsoleSink::logMessage）
  void appendLog(int level, const QString& text);

  /// 清空显示与缓存
  void clearLog();

 private:
  void initUi();
  void initSignals();

  /// 渲染单条（含高亮），末尾按 scroll_locked_ 决定滚底（实时单条用）
  void renderEntry(const etest::core::logger::LogEntry& entry);
  /// 渲染核心：着色整行 + 命中片段高亮 span，返回 HTML
  QString buildEntryHtml(const etest::core::logger::LogEntry& entry) const;
  /// 级别对应颜色
  QString levelColor(int level) const;

  /// 全量重渲：clear + 遍历 entries_ 渲染命中项（带 was_at_bottom 滚动优化）
  void applyFilter();
  /// 防抖入口
  void requestFilterApply();

 private slots:
  void onHistoricalLogs(const QList<etest::core::logger::LogEntry>& entries);
  void onFilterChanged(const LogFilter& filter);
  void onScrollLockChanged(bool locked);

 private:
  LogFilterBar* filter_bar_ = nullptr;
  QTextEdit* text_edit_ = nullptr;

  /// 唯一数据源（环形，超限丢最老）
  QList<etest::core::logger::LogEntry> entries_;
  LogFilter current_filter_;
  static constexpr int kMaxEntries = 5000;

  QTimer* debounce_timer_ = nullptr;
  bool scroll_locked_ = false;
};

}  // namespace etest::app

#endif  // ETEST_APP_LOG_OUTPUT_PANEL_H_
