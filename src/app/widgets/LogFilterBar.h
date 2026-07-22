#ifndef ETEST_APP_WIDGETS_LOG_FILTER_BAR_H_
#define ETEST_APP_WIDGETS_LOG_FILTER_BAR_H_

#include <QHash>
#include <QLineEdit>
#include <QList>
#include <QToolButton>
#include <QWidget>

#include "LogFilter.h"

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// LogFilterBar - 日志筛选工具栏
// ══════════════════════════════════════════════════════════════════════════════
// 水平一条：[DEBUG][INFO][WARN][ERROR][FATAL]  [过滤框]  [.*][Aa][🔒][🗑]
// 由 LogOutputPanel 顶部持有。级别按钮拦截「取消最后一个勾选」，保证 enabledLevels
// 恒非空。样式全走 QSS #LogFilterBar 选择器。
class LogFilterBar : public QWidget {
  Q_OBJECT

 public:
  explicit LogFilterBar(QWidget* parent = nullptr);

  /// 当前筛选条件（级别集合恒非空）
  LogFilter filter() const;

  /// 滚动锁定状态
  bool scrollLocked() const { return scroll_locked_; }

  /// 设置过滤框 invalid 视觉态（正则非法时红框 + tooltip）
  void setFilterInvalid(bool invalid, const QString& error = QString());

 signals:
  /// 筛选条件实际变化时发射（级别/文本/正则/大小写任一变化）
  void filterChanged(const LogFilter& filter);
  /// 滚动锁定开关变化
  void scrollLockChanged(bool locked);
  /// 清空按钮点击
  void clearRequested();

 private:
  void initUi();
  void initSignals();

  /// 某级别按钮 toggled 槽，拦截「取消最后一个勾选」
  void onLevelToggled(int level, bool checked);

  /// 级别定义（单字母按钮 + spdlog level + objectName 后缀 + 全称 tooltip）
  struct LevelDef {
    QString name;
    int level;
    QString objectSuffix;
    QString fullName;
  };
  QList<LevelDef> level_defs_;
  /// level -> 按钮，用于 toggle 拦截
  QHash<int, QToolButton*> level_buttons_;

  QLineEdit* filter_edit_ = nullptr;
  QToolButton* regex_btn_ = nullptr;
  QToolButton* case_btn_ = nullptr;
  QToolButton* lock_btn_ = nullptr;
  QToolButton* clear_btn_ = nullptr;

  bool scroll_locked_ = false;
  // setFilterInvalid 去重用，避免每次 filterChanged 都刷样式
  bool last_invalid_ = false;
  QString last_error_;
};

}  // namespace etest::app

#endif  // ETEST_APP_WIDGETS_LOG_FILTER_BAR_H_
