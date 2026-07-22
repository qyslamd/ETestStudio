#include "LogFilter.h"

namespace etest::app {

bool LogFilter::matches(int level, const QString& entryText) const {
  // 级别过滤
  if (!enabledLevels.contains(level)) {
    return false;
  }

  // 文本为空 = 仅级别过滤
  if (text.isEmpty()) {
    return true;
  }

  if (useRegex) {
    QRegularExpression re = compileRegex();
    if (!re.isValid()) {
      // 非法正则退化为「文本不限」，仅级别过滤已通过
      return true;
    }
    return re.match(entryText).hasMatch();
  }

  // 普通包含
  Qt::CaseSensitivity cs =
      caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
  return entryText.contains(text, cs);
}

QList<QPair<int, int>> LogFilter::matchRanges(const QString& entryText) const {
  QList<QPair<int, int>> ranges;
  if (text.isEmpty()) {
    return ranges;
  }
  if (useRegex) {
    QRegularExpression re = compileRegex();
    if (!re.isValid()) {
      return ranges;
    }
    auto it = re.globalMatch(entryText);
    while (it.hasNext()) {
      QRegularExpressionMatch m = it.next();
      ranges.append({m.capturedStart(), m.capturedEnd()});
    }
    return ranges;
  }
  Qt::CaseSensitivity cs =
      caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
  int from = 0;
  while (from < entryText.size()) {
    int idx = entryText.indexOf(text, from, cs);
    if (idx < 0) {
      break;
    }
    ranges.append({idx, idx + text.size()});
    from = idx + text.size();
  }
  return ranges;
}

bool LogFilter::isValid() const {
  if (!useRegex || text.isEmpty()) {
    return true;
  }
  return compileRegex().isValid();
}

QString LogFilter::errorString() const {
  if (!useRegex || text.isEmpty()) {
    return QString();
  }
  return compileRegex().errorString();
}

QRegularExpression LogFilter::compileRegex() const {
  QRegularExpression::PatternOptions options;
  if (!caseSensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }
  return QRegularExpression(text, options);
}

}  // namespace etest::app
