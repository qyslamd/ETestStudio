# UI 布局重构方案

## 问题

当前整个 MainWindow 的布局完全由 Qt-Advanced-Docking-System（QADS）的 `CDockManager` 接管，侧边栏、底部面板、辅助面板、编辑器全部作为 `CDockWidget` 管理。但事实上这些面板中只有编辑器需要浮动/停靠能力，其余面板均被限制了 `DockWidgetFloatable` 和 `DockWidgetMovable`，QADS 的停靠能力被浪费。

目标：将 QADS 的使用范围限制到编辑器区域，其余面板回归普通 `QWidget` + `QSplitter` 布局。

## 布局结构

```
                      MainWindow (QMainWindow)
┌───────────────────────────────────────────────────────────────────────┐
│                        centralContainer (QWidget)                     │
│ ┌───────────────────────────────────────────────────────────────────┐ │
│ │  QHBoxLayout                                                     │ │
│ │ ┌──────────┬────────────────────────────────────────────────────┐│ │
│ │ │Activity  │              QSplitter (水平)                       ││ │
│ │ │   Bar    │ ┌──────────┬────────────────────┬──────────────┐   ││ │
│ │ │  48px    │ │ Sidebar  │ QSplitter (垂直)    │ AuxSidebar   │   ││ │
│ │ │  fixed   │ │  Widget  │ ┌────────────────┐ │  Widget      │   ││ │
│ │ │          │ │ 0~300px  │ │  CDockManager  │ │ (collapsible) │   ││ │
│ │ │  ┌───┐   │ │          │ │ ┌──┬──┬──┬──┐  │ │              │   ││ │
│ │ │  │资 │   │ │          │ │ │欢│E1│E2│E3│  │ │              │   ││ │
│ │ │  │源 │   │ │          │ │ │迎│  │  │  │  │ │              │   ││ │
│ │ │  │管 │   │ │          │ │ └──┴──┴──┴──┘  │ │              │   ││ │
│ │ │  │理 │   │ │          │ ├────────────────┤ │              │   ││ │
│ │ │  │器 │   │ │          │ │BottomContainer │ │              │   ││ │
│ │ │  ├───┤   │ │          │ │  (show/hide)   │ │              │   ││ │
│ │ │  │搜 │   │ │          │ └────────────────┘ │              │   ││ │
│ │ │  │索 │   │ │          │                    │              │   ││ │
│ │ │  ├───┤   │ │          │                    │              │   ││ │
│ │ │  │.. │   │ │          │                    │              │   ││ │
│ │ │  └───┘   │ │          │                    │              │   ││ │
│ │ └──────────┴─┴──────────┴────────────────────┴──────────────┘   ││ │
│ └───────────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────────┘
```

要点：
- 编辑器区域和底部面板共用一个垂直 splitter，用户可直接拖拽 splitter 手柄调整高度
- `CDockManager` 始终存在，不销毁重建
- `WelcomeWidget` 是 `CDockManager` 的一个普通 tab
- `BottomContainerWidget` 通过 `setVisible()` 控制显隐（即可通过关闭按钮隐藏，也可通过 View 菜单切换）
- 没有 `QStackedWidget`，WelcomeWidget 不独立于 CDockManager

## 组件拆分

### ActivityBarWidget（新建）

从原 `SidebarWidget` 拆出左侧 48px 的活动按钮栏：
- 8 个图标按钮：资源管理器、搜索、源代码管理、调试、扩展、硬件、协议、用例
- 底部设置按钮
- 维护 `active_index_`，记录当前选中按钮
- 点击按钮发出 `pageClicked(int index)` 信号
- 再次点击同一按钮不产生新信号（由 `MainWindow` 判断 toggle 逻辑）

### SidebarWidget（改造）

去掉活动栏部分，只保留内容面板：
- 标题栏 + `QStackedWidget`（8 页：文件浏览/搜索/Git/调试/扩展/硬件/协议/用例）
- 新增 `showContent()` / `hideContent()` / `toggleContent()` 替代原来的 `toggleContentPanel()`
- 移除 `buttons_` 和 `active_index_`（移到 `ActivityBarWidget`）
- 移除 `contentPanelToggled` 信号（由 `MainWindow` 编排）

### AuxSidebarWidget

当前辅助侧边栏已经是 QWidget 容器，只需将其从 `CDockWidget` 改为普通 `QWidget` 放入 `QSplitter`。

初始内容为 `QLabel("辅助侧边栏")` 占位。辅助侧边栏的功能延后设计，暂时不实现任何逻辑。
默认宽度 280px，通过 View 菜单"辅助侧边栏"菜单项 toggle 显隐。

### BottomContainerWidget（原 PanelContainerWidget）

- 类名 `PanelContainerWidget` → `BottomContainerWidget`
- 文件名 `PanelContainerWidget.h/.cpp` → `BottomContainerWidget.h/.cpp`
- 去掉最大化/最小化功能：移除 `max_button_`、`maximized_`、`setMaximized()`、`isMaximized()`、`panelMaximized()` / `panelRestored()` 信号
- 保留：`tab_widget_`（输出/问题/终端三个 tab）、`close_button_` + `panelClosed()` 信号
- `panelClosed()` 由 `MainWindow` 连接为 `setVisible(false)`，并在 `setVisible(true)` 时恢复上一次的高度
- 与 View 菜单的"输出"菜单项关联

### CDockManager

- 始终存在，不随编辑器数量销毁重建
- 初始状态只有一个 `WelcomeWidget` tab
- "开始视图"菜单项激活 WelcomeWidget 的 tab
- 所有编辑器关闭后 WelcomeWidget tab 依然在

## 组件协作

### ActivityBarWidget ↔ SidebarWidget

`MainWindow` 负责编排，不直接在两个组件之间连线：

```
ActivityBarWidget                  MainWindow                         SidebarWidget
  pageClicked(int) ──────►  判断：
                            如果是 re-click
                              → show/hide SidebarWidget
                              → 调整水平 QSplitter 尺寸
                            如果是切换页面
                              → 确保 SidebarWidget 可见
                              → SidebarWidget.switchPage(int)
                              → ActivityBarWidget.setActiveIndex(int)
```

`ActivityBarWidget` 只发出 `pageClicked(int)`，不决定 toggle 逻辑。

### ActivityBarWidget 信号

```cpp
signals:
    void pageClicked(int index);  // 不区分 re-click vs 切换
    void settingsTriggered();
```

`MainWindow` 连接：

```cpp
// 水平 splitter 有 3 个子 widget: Sidebar / 垂直区域 / AuxSidebar
connect(activity_bar_, &ActivityBarWidget::pageClicked, this, [this](int index) {
    if (activity_bar_->activeIndex() == index && sidebar_->isVisible()) {
        // re-click: toggle sidebar
        sidebar_->hideContent();
        h_splitter_->setSizes({0, h_splitter_->width(), aux_sidebar_width_});
    } else {
        // switch page
        if (!sidebar_->isVisible())
            sidebar_->showContent();
        sidebar_->switchPage(index);
        activity_bar_->setActiveIndex(index);
    }
});
```

## Session 持久化

分三部分保存/恢复：

| 数据 | 方式 | JSON 字段 |
|------|------|-----------|
| 编辑器布局（分屏/tab） | `CDockManager::saveState()` → `restoreState()` | `editorState` |
| Splitter 尺寸 | `QSplitter::saveState()` → `restoreState()` | `splitterState`、`editorSplitterState` |
| 活动栏选中索引 | 手动 JSON int | `activeBarIndex` |

### 保存规则

- `editorState`：如果 CDockManager 中除 WelcomeWidget 外没有编辑器 tab，则不保存此字段
- `splitterState`：水平 QSplitter 的状态（Sidebar / 垂直区域 / AuxSidebar 的尺寸）
- `editorSplitterState`：垂直 QSplitter 的状态（编辑器 / 底部的尺寸）

### 恢复规则

```cpp
void MainWindow::restoreSession() {
    // 1. 恢复活动栏索引
    // 2. 恢复水平 splitter 尺寸
    // 3. 恢复垂直 splitter 尺寸
    // 4. 如果有 editorState，恢复 CDockManager 状态
}
```

## 涉及的文件

| 操作 | 文件 |
|------|------|
| 新建 | `src/app/ActivityBarWidget.h` |
| 新建 | `src/app/ActivityBarWidget.cpp` |
| 修改 | `src/app/SidebarWidget.h` |
| 修改 | `src/app/SidebarWidget.cpp` |
| 修改 | `src/app/main_window.h` |
| 修改 | `src/app/main_window.cpp` |
| 修改 | `src/app/PanelContainerWidget.h`（重命名为 BottomContainerWidget） |
| 修改 | `src/app/PanelContainerWidget.cpp`（重命名为 BottomContainerWidget） |
| 修改 | `src/app/CMakeLists.txt` |

## 不处理的事项

- `EditorManager` 不变
- `PanelContainerWidget` 改名为 `BottomContainerWidget`，内部逻辑简化（去掉最大化）
- 辅助侧边栏功能延后设计，仅保留 QLabel 占位
- `SidebarWidget` 各页面内容（FileExplorerWidget、SearchWidget 等）不变
- QSS 样式：`QWidget#sidebarActivityBar` 改为 `ActivityBarWidget`，其余调整
