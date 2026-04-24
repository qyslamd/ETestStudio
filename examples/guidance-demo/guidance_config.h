#ifndef GUIDANCE_CONFIG_H
#define GUIDANCE_CONFIG_H

#include <QList>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QWidget>
#include <functional>
#include <variant>

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

  inline Func enterFunc() const { return onEnter; }
  inline void setEnterFunc(const Func& func) { onEnter = func; }

  inline Func exitFunc() const { return onExit; }
  inline void setExitFunc(const Func& func) { onExit = func; }

  GuidanceStep* withTitle(const QString& title) {
    this->title_ = title;
    return this;
  }
  GuidanceStep* withDescripton(const QString& desc) {
    this->description_ = desc;
    return this;
  }
  GuidanceStep* withEnterFunc(const Func& func) {
    this->onEnter = func;
    return this;
  }
  GuidanceStep* withExitFunc(const Func& func) {
    this->onExit = func;
    return this;
  }
};

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

  GuidanceFlow(const QPixmap& icon1, const QString& name1, const QString& desc1)
      : icon_(icon1), name_(name1), description_(desc1) {}

  GuidanceFlow(const QPixmap& icon) : icon_(icon) {}

  GuidanceFlow(){};
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

Q_DECLARE_METATYPE(GuidanceFlow*)

#endif  // GUIDANCE_CONFIG_H
