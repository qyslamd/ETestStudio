#ifndef ETEST_APP_WIDGETS_LOG_FILTER_H_
#define ETEST_APP_WIDGETS_LOG_FILTER_H_

#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QString>

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// LogFilter - 日志筛选条件（纯数据结构，与 UI 解耦）
// ══════════════════════════════════════════════════════════════════════════════
// 由 LogFilterBar 聚合，LogOutputPanel 持有当前筛选条件并据此重渲。
// 级别用 int（与 core::logger::LogEntry::level 对齐，spdlog::level::level_enum 强转）。
// enabledLevels 恒非空：LogFilterBar 拦截「取消最后一个勾选」保证。
//   空集语义未定义，调用方不应主动构造空集 filter。
struct LogFilter {
  QString text;                 // 过滤文本（普通模式为字面量，正则模式为正则表达式）
  bool useRegex = false;        // true=正则匹配，false=字面量包含
  bool caseSensitive = false;   // 大小写敏感
  QSet<int> enabledLevels;      // 启用的级别集合

  // 判断单条日志是否命中筛选。
  // 级别不在 enabledLevels -> false。
  // text 为空 -> true（仅级别过滤）。
  // 普通模式：字面量包含（受 caseSensitive）。
  // 正则模式：合法时按正则匹配；非法时 isValid()=false，退化为「文本不限」。
  bool matches(int level, const QString& entryText) const;

  // 返回 text 在 entryText 中的命中区间列表 [start, end)。
  // text 为空、正则非法、或无命中 -> 返回空列表。
  // 供 UI 高亮用，与 matches 共用同一匹配逻辑。
  QList<QPair<int, int>> matchRanges(const QString& entryText) const;

  // 正则模式且 text 非空时，返回 QRegularExpression::isValid()；
  // 其余情况（普通模式、空 text）恒 true。
  bool isValid() const;

  // 返回正则非法时的错误描述（QRegularExpression::errorString）。
  // 非正则模式或合法时返回空串。
  QString errorString() const;

 private:
  // 构造当前正则（应用 caseSensitive）。useRegex=false 或 text 空时返回空正则。
  QRegularExpression compileRegex() const;
};

}  // namespace etest::app

#endif  // ETEST_APP_WIDGETS_LOG_FILTER_H_
