#pragma once

#include <QWidget>

/// 屏保策略的抽象基类
///
/// TuxSaverOverlay 通过此接口管理所有屏保模式，
/// 每种屏保实现为一个 SaverWidgetBase 子类。
class SaverWidgetBase : public QWidget {
  Q_OBJECT
 public:
  explicit SaverWidgetBase(QWidget* parent = nullptr);
  ~SaverWidgetBase() override = default;

  /// 覆盖层显示时调用
  virtual void onActivate() {}
  /// 覆盖层隐藏时调用
  virtual void onDeactivate() {}

  /// 模式显示名称（用于切换器等 UI）
  virtual QString displayName() const = 0;

  /// 可选：如果屏保有自身的空闲检测，可覆盖此方法
  virtual void setIdleThreshold(int sec);
};
