#pragma once

#include <QList>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QWidget>
#include <functional>
#include <variant>

namespace etest::app {

// 单个引导步骤：目标元素（控件/矩形/图片）+ 标题/描述 + 进入/退出回调。
// 纯类，无 Q_OBJECT（不依赖元对象系统）。
class GuidanceStep {
 public:
  using Target = std::variant<QWidget*, QRect, QPixmap>;
  using Func = std::function<void()>;

 private:
  QString title_;
  QString description_;
  Target target_;
  std::function<void()> onEnter;
  std::function<void()> onExit;

 public:
  GuidanceStep(const Target& target)
      : target_(target),
        title_("未知"),
        description_("未知"),
        onEnter(nullptr),
        onExit(nullptr) {}

  inline QString title() const { return title_; }
  inline void setTitle(const QString& title) { title_ = title; }

  inline QString description() const { return description_; }
  inline void setDescription(const QString& desc) { description_ = desc; }

  inline Target target() { return target_; }

  // 允许在 enter 回调中延迟定位目标（D12：编辑器 openFile 后才可拿到画布控件）。
  inline void setTarget(const Target& target) { target_ = target; }

  inline Func enterFunc() const { return onEnter; }
  inline void setEnterFunc(const Func& func) { onEnter = func; }

  inline Func exitFunc() const { return onExit; }
  inline void setExitFunc(const Func& func) { onExit = func; }

  GuidanceStep* withTitle(const QString& title) {
    title_ = title;
    return this;
  }
  GuidanceStep* withDescription(const QString& desc) {
    description_ = desc;
    return this;
  }
  GuidanceStep* withEnterFunc(const Func& func) {
    onEnter = func;
    return this;
  }
  GuidanceStep* withExitFunc(const Func& func) {
    onExit = func;
    return this;
  }
};

// 一条引导主题（Flow）：图标 + 名称/描述 + 一组 Step + 主题级进入/退出回调。
// 纯类，无 Q_OBJECT。
class GuidanceFlow {
 private:
  QPixmap icon_;
  QString name_;
  QString description_;
  std::function<void()> onEnter = nullptr;
  std::function<void()> onExit = nullptr;
  QList<GuidanceStep*> steps_;

 public:
  GuidanceFlow(const QString& name, const QString& desc)
      : name_(name), description_(desc) {}

  GuidanceFlow(const QPixmap& icon, const QString& name, const QString& desc)
      : icon_(icon), name_(name), description_(desc) {}

  GuidanceFlow(const QPixmap& icon) : icon_(icon) {}

  GuidanceFlow() = default;
  ~GuidanceFlow();

  void addStep(GuidanceStep* step);
  void clear();

  inline QString name() const { return name_; }
  inline void setName(const QString& name) { name_ = name; }

  inline QString description() const { return description_; }
  inline void setDescription(const QString& desc) { description_ = desc; }

  inline QPixmap icon() const { return icon_; }
  inline void setIcon(const QPixmap& icon) { icon_ = icon; }

  inline const QList<GuidanceStep*>& steps() const { return steps_; }
  inline int stepCount() const { return steps_.count(); }

  inline std::function<void()> enterFunc() const { return onEnter; }
  inline void setEnterFunc(const std::function<void()>& func) {
    onEnter = func;
  }

  inline std::function<void()> exitFunc() const { return onExit; }
  inline void setExitFunc(const std::function<void()>& func) { onExit = func; }

  GuidanceFlow* withStep(GuidanceStep* step) {
    addStep(step);
    return this;
  }
};

// 引导配置：持有全部 Flow。纯类，无 Q_OBJECT。
class GuidanceConfig {
 public:
  GuidanceConfig();
  ~GuidanceConfig();

  inline const QList<GuidanceFlow*>& flows() const { return flows_; }

  GuidanceFlow* addFlow(GuidanceFlow* flow);
  void clear();
  int totalSteps() const;

 private:
  QList<GuidanceFlow*> flows_;
};

}  // namespace etest::app

Q_DECLARE_METATYPE(etest::app::GuidanceFlow*)
