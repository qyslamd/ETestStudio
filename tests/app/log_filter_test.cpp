#include <gtest/gtest.h>

#include "widgets/LogFilter.h"

#include <QSet>
#include <QString>

namespace {

// spdlog::level 枚举值：trace=0, debug=1, info=2, warn=3, err=4, critical=5。
// 测试用裸 int，避免拖 spdlog include 路径，与 LogEntry::level（int）对齐。
constexpr int kDebug = 1;
constexpr int kInfo = 2;
constexpr int kWarn = 3;
constexpr int kErr = 4;
constexpr int kCritical = 5;

// 构造一个启用全部 5 级的 filter，作为多数用例的基线。
etest::app::LogFilter makeAllLevelsFilter() {
  etest::app::LogFilter f;
  f.enabledLevels = QSet<int>{kDebug, kInfo, kWarn, kErr, kCritical};
  return f;
}

}  // namespace

// ── 级别过滤 ──────────────────────────────────────────────────

// 启用集合包含该级别 -> 命中。
TEST(LogFilterTest, LevelInEnabledSetMatches) {
  auto f = makeAllLevelsFilter();
  f.text.clear();
  EXPECT_TRUE(f.matches(kInfo, QStringLiteral("anything")));
  EXPECT_TRUE(f.matches(kErr, QStringLiteral("anything")));
}

// 启用集合不含该级别 -> 不命中，与文本无关。
TEST(LogFilterTest, LevelNotInEnabledSetDoesNotMatch) {
  etest::app::LogFilter f;
  f.enabledLevels = QSet<int>{kWarn, kErr};
  f.text.clear();
  EXPECT_FALSE(f.matches(kInfo, QStringLiteral("hit keyword")));
  EXPECT_FALSE(f.matches(kDebug, QStringLiteral("hit keyword")));
}

// ── 文本过滤（普通模式） ──────────────────────────────────────

// 空文本 = 不限文本，仅看级别。
TEST(LogFilterTest, EmptyTextMatchesAllByLevel) {
  auto f = makeAllLevelsFilter();
  f.text.clear();
  f.useRegex = false;
  EXPECT_TRUE(f.matches(kInfo, QStringLiteral("")));
  EXPECT_TRUE(f.matches(kInfo, QStringLiteral("random")));
}

// 普通包含，大小写不敏感（默认）。
TEST(LogFilterTest, PlainContainsCaseInsensitive) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("error");
  f.useRegex = false;
  f.caseSensitive = false;
  EXPECT_TRUE(f.matches(kErr, QStringLiteral("[ERROR] boom")));
  EXPECT_TRUE(f.matches(kErr, QStringLiteral("ErRoR here")));
  EXPECT_FALSE(f.matches(kErr, QStringLiteral("all good")));
}

// 普通包含，大小写敏感。
TEST(LogFilterTest, PlainContainsCaseSensitive) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("Error");
  f.useRegex = false;
  f.caseSensitive = true;
  EXPECT_TRUE(f.matches(kErr, QStringLiteral("[Error] boom")));
  EXPECT_FALSE(f.matches(kErr, QStringLiteral("[ERROR] boom")));
}

// 中文关键字普通包含。
TEST(LogFilterTest, PlainContainsChinese) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("拓扑");
  f.useRegex = false;
  f.caseSensitive = false;
  EXPECT_TRUE(f.matches(kInfo, QStringLiteral("加载拓扑文件成功")));
  EXPECT_FALSE(f.matches(kInfo, QStringLiteral("加载协议文件成功")));
}

// ── 文本过滤（正则模式） ──────────────────────────────────────

// 正则匹配，大小写不敏感。
TEST(LogFilterTest, RegexMatchCaseInsensitive) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("err(or)?");
  f.useRegex = true;
  f.caseSensitive = false;
  EXPECT_TRUE(f.matches(kErr, QStringLiteral("[ERR] boom")));
  EXPECT_TRUE(f.matches(kErr, QStringLiteral("[ERROR] boom")));
  EXPECT_FALSE(f.matches(kErr, QStringLiteral("all good")));
}

// 正则匹配，大小写敏感。
TEST(LogFilterTest, RegexMatchCaseSensitive) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("Err(or)?");
  f.useRegex = true;
  f.caseSensitive = true;
  EXPECT_TRUE(f.matches(kErr, QStringLiteral("[Err] boom")));
  EXPECT_FALSE(f.matches(kErr, QStringLiteral("[err] boom")));
}

// 正则非法 -> isValid()=false，matches 退化为「仅级别过滤」（文本不限）。
TEST(LogFilterTest, InvalidRegexDegradesToLevelOnly) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("[unbalanced");  // 非法正则
  f.useRegex = true;
  EXPECT_FALSE(f.isValid());
  // 非法时文本不过滤，级别命中即通过
  EXPECT_TRUE(f.matches(kInfo, QStringLiteral("任意文本")));
  EXPECT_TRUE(f.matches(kDebug, QStringLiteral("anything")));  // debug 已启用
  // 级别不命中时即使文本无关也不通过（用未启用的 trace=0）
  EXPECT_FALSE(f.matches(0, QStringLiteral("anything")));
}

// ── 级别与文本组合 ────────────────────────────────────────────

// 级别不命中时，即使文本命中也不通过。
TEST(LogFilterTest, LevelFailOverridesTextHit) {
  etest::app::LogFilter f;
  f.enabledLevels = QSet<int>{kErr};
  f.text = QStringLiteral("hit");
  f.useRegex = false;
  EXPECT_FALSE(f.matches(kInfo, QStringLiteral("hit keyword")));
}

// ── isValid ─────────────────────────────────────────────────

// 普通模式永远 valid。
TEST(LogFilterTest, PlainModeAlwaysValid) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("anything");
  f.useRegex = false;
  EXPECT_TRUE(f.isValid());
}

// 空正则 valid（空 pattern 合法）。
TEST(LogFilterTest, EmptyRegexValid) {
  auto f = makeAllLevelsFilter();
  f.text.clear();
  f.useRegex = true;
  EXPECT_TRUE(f.isValid());
}

// ── matchRanges（高亮用） ────────────────────────────────────

// 普通模式：多命中区间正确。
TEST(LogFilterTest, PlainMatchRangesMultiple) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("er");
  f.useRegex = false;
  f.caseSensitive = false;
  // "error super"：e=0,r=1,r=2,o=3,r=4,space=5,s=6,u=7,p=8,e=9,r=10
  auto ranges = f.matchRanges(QStringLiteral("error super"));
  ASSERT_EQ(ranges.size(), 2);
  EXPECT_EQ(ranges[0].first, 0);    // "error" 的 er
  EXPECT_EQ(ranges[0].second, 2);
  EXPECT_EQ(ranges[1].first, 9);    // "super" 的 er
  EXPECT_EQ(ranges[1].second, 11);
}

// 正则模式：捕获区间为整体匹配。
TEST(LogFilterTest, RegexMatchRanges) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("err(or)?");
  f.useRegex = true;
  f.caseSensitive = false;
  // "[error]"：匹配 "error"（err + or），index 1-6
  auto ranges = f.matchRanges(QStringLiteral("[error]"));
  ASSERT_EQ(ranges.size(), 1);
  EXPECT_EQ(ranges[0].first, 1);
  EXPECT_EQ(ranges[0].second, 6);
}

// 空文本 -> 空区间。
TEST(LogFilterTest, EmptyTextNoRanges) {
  auto f = makeAllLevelsFilter();
  f.text.clear();
  EXPECT_TRUE(f.matchRanges(QStringLiteral("anything")).isEmpty());
}

// 非法正则 -> 空区间（不高亮）。
TEST(LogFilterTest, InvalidRegexNoRanges) {
  auto f = makeAllLevelsFilter();
  f.text = QStringLiteral("[unbalanced");
  f.useRegex = true;
  EXPECT_TRUE(f.matchRanges(QStringLiteral("anything")).isEmpty());
}
