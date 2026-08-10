#pragma once

#include <QList>
#include <QString>

#include <functional>

class QMenu;
class QObject;

namespace etest::app {

// 一条编辑器命令的描述（命令定义模式）。由编辑器暴露，Ribbon 上下文页据此创建
// 自己的 QAction 并桥接到编辑器方法；独立工具模式亦可据此构建自带工具栏。
// 命令回调引用编辑器自身，仅在编辑器存活期间有效；编辑器关闭时上下文页即清空。
struct EditorCommand {
  QString group;      // 所在面板标题（Ribbon panel 名）
  QString title;      // 命令标题
  QString iconName;   // AppIconProvider 图标名；空则不设图标
  bool checkable = false;
  bool large = false;  // true = Ribbon 大按钮（panel 顶部）；false = 小按钮
  std::function<void()> trigger;    // 触发回调（调用编辑器方法）
  std::function<bool()> isEnabled;  // 状态查询：可空 = 恒 true
  std::function<bool()> isChecked;  // 状态查询：可空 = 恒 false
  std::function<QMenu*()> menu;     // 可空：非空时命令带下拉菜单（对齐/分布等）
};

// 编辑器可选实现的命令贡献接口。Ribbon 上下文页经 dynamic_cast 获取。
class IEditorCommandSource {
 public:
  virtual ~IEditorCommandSource() = default;

  // 有序命令清单（含分组）。每次调用返回当前清单。
  virtual QList<EditorCommand> editorCommands() = 0;

  // 命令状态变化载体：其上的 `commandsChanged()` 信号表示 enabled/checked 变化。
  // 通常返回编辑器自身（QObject）。
  virtual QObject* commandStateObject() = 0;
};

}  // namespace etest::app
