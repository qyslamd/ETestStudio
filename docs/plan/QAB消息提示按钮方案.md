# QAB 消息提示按钮方案

## 问题陈述

现有提示消息功能布局在 `hint_bar_`（`HintBarWidget`），位于 page0 编辑器区域顶部，存在两个问题：

1. **位置局限**：只在 page0（编辑态）可见，page1（运行态）看不到提示消息
2. **占用空间**：占据编辑器区域顶部高度，无消息时隐藏但布局结构仍在

目标：将提示消息功能迁移到 QAB（Quick Access Bar）上的一个 action，QAB 始终可见，有消息时图标显示小红点，点击弹出 popup 窗口显示消息列表。`hint_bar_` 完全移除。

## 架构回顾

### HintBarWidget 现状

`HintBarWidget`（`src/app/widgets/HintBarWidget.h`）：

**数据结构**：
```cpp
struct HintData {
  QString text;                    // 消息文本
  QString actionLabel;             // 可选操作按钮文本
  std::function<void()> action;    // 可选操作回调
};
```

**接口**：
- `postHint(text, actionLabel, action)` -- 发送提示消息
- `clearAll()` -- 清空所有

**内部机制**：
- 最多显示 3 条（`kMaxVisible=3`），超出进 `pending_queue_` 等待
- 每条提示项：指示器 ● + elided 文本 + 可选操作按钮 + 关闭按钮 ✕
- 关闭一条后从队列补入下一条

**MainWindow 中的使用**：
- `initUi`（209 行）创建，放在 `editor_area` 顶部
- `lazyInit`（1295-1306 行）`#ifdef _DEBUG` 块调用 `postHint` 发送测试消息
- QSS 中有 `#HintBar` / `#HintItem` / `#HintIndicator` / `#HintText` / `#HintActionBtn` / `#HintCloseBtn` 样式

**调用者**：仅 `MainWindow::lazyInit` 中的 DEBUG 测试消息

## 方案设计

### 新组件：HintButton

替代 `HintBarWidget`，作为 QAB 上的消息提示按钮。

**命名**：`HintButton`（与现有 `HintData` 命名一致）

**职责**：
- 点击弹出 popup 窗口（`Qt::Popup`），内含 `QListView` + 自定义 delegate
- 监听 `MessageService` 的 `unreadCountChanged` 信号，有未读消息时图标切换为带小红点版本

**API**：
```cpp
class HintButton : public QToolButton {
  Q_OBJECT
 public:
  explicit HintButton(QWidget* parent = nullptr);

 private:
  HintPopup* popup_;  // popup 窗口（QListView + 底部工具栏）
};
```

`HintButton` 不持有消息数据。消息入口在 `MessageService::instance()`，任何组件都可通过 `MessageService::instance().postHint(...)` 发消息。`HintButton` 监听 `MessageService` 的 `unreadCountChanged` 信号驱动图标切换。

点击 `HintButton` 时显式判断 `popup_->isVisible()`：可见则 `close()`，不可见则 `move()`+`show()`。避免 `Qt::Popup` 模式下点击触发按钮被视为外部点击导致闪烁。

### MessageService（单例）

全局消息服务，继承 `QAbstractListModel`，既是消息入口又是 QListView 的 model。符合项目现有单例风格（`ConfigManager::instance()` / `ProjectManager::instance()`）。

```cpp
class MessageService : public QAbstractListModel {
  Q_OBJECT
 public:
  static MessageService& instance();

  // 消息入口（任何组件可调用）
  void postHint(const QString& text,
                const QString& actionLabel = QString(),
                std::function<void()> action = nullptr);
  void clearAll();
  void markAllRead();
  void removeAt(int row);
  void markRead(int row);

  // QAbstractListModel
  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  int unreadCount() const;

 signals:
  void unreadCountChanged(int count);
};
```

`HintData` 定义在 `MessageService.h`，含 `text` / `actionLabel` / `action` / `read` 字段。

`HintPopup` 的 QListView 直接 `setModel(&MessageService::instance())`。

### 图标切换

两套图标，有**未读**消息时切换为带小红点版本，全部已读或无消息时切换回普通版本：
- 无未读消息：`message` 图标（需新增 SVG）
- 有未读消息：`message_alert` 图标（带小红点，需新增 SVG）

图标放 `src/app/resources/icons/svg/`，配 `_light` / `_dark` 两套。

`HintData` 新增 `bool read` 字段，`postHint` 时默认 `read=false`。四个操作的职责正交：

| 操作 | 效果 |
|------|------|
| 点击操作按钮 | 触发回调 + 标记该消息已读（不删除，允许重复点击） |
| 点击关闭按钮 | 删除该消息（条目级） |
| 一键已读 | 所有消息标记已读（列表级，不删除） |
| 清空 | 删除所有消息（列表级） |

未读数变化时触发图标切换。删除未读消息时未读数相应减少。

### HintData 归属

`HintData` 定义在 `MessageService.h`，`HintButton` / `HintMessageDelegate` 共用。

### Popup 窗口

独立类 `HintPopup`，继承 `QWidget`，不用 `QMenu`。参考 YiGuai 项目 `popups/PopupBase` 的实现模式。

**窗口标志**：
```cpp
setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
setAttribute(Qt::WA_TranslucentBackground);
setAttribute(Qt::WA_NoMouseReplay);  // 防鼠标事件重放（避免点击触发按钮后 popup 关闭又打开）
```

**阴影**：`QGraphicsDropShadowEffect`，blurRadius 20，配合 `WA_TranslucentBackground` 实现圆角阴影。

**尺寸约束**：固定宽度 360px，高度自适应内容（最小 120px，最大 400px），QListView 虚拟滚动应对大量消息。

**结构**：
```
HintPopup (QWidget, Qt::Popup)
  └─ QVBoxLayout
       ├─ 顶部工具栏（QToolButton: 一键已读 + 清空）
       ├─ 分割线
       └─ QListView (自定义 delegate, NoSelection)
```

**整体布局**：
```
┌───────────────────────────────────────┐  ← 圆角 8px + 阴影
│  ┌───────────────────────────────┐    │  ← 内容容器 QFrame
│  │ [一键已读] [清空]              │    │  ← 顶部工具栏，右对齐
│  ├───────────────────────────────┤    │  ← 分割线
│  │  QListView                     │    │
│  │  ┌─┬─────────────┬────┬───┐   │    │
│  │  │■│ 消息文本...   │操作│ ✕│   │    │  ← 未读行：色块 + 加粗
│  │  ├─┼─────────────┼────┼───┤   │    │
│  │  │ │ 消息文本...   │操作│ ✕│   │    │  ← 已读行：无色块 + 普通色
│  │  └─┴─────────────┴────┴───┘   │    │
│  │  （行高 36px，QListView 透明背景）│    │
│  └───────────────────────────────┘    │
└───────────────────────────────────────┘
```

**Delegate 行内布局**（`HintMessageDelegate::paint`）：
- 左侧色块：4px 宽，未读时主题色（如蓝），已读时透明
- 消息文本：elide，左对齐垂直居中，未读加粗
- 操作按钮：约 48px 宽，有 actionLabel 时绘制，hover 高亮
- 关闭按钮：20px 宽，hover 高亮

顶部工具栏按钮用 `QToolButton`，配 objectName（`HintPopupMarkAllBtn` / `HintPopupClearBtn`），样式写 QSS 文件。

**显隐切换**（参考 YiGuai `mainwindow.cpp:1174`）：
```cpp
popup->move(pos);  // pos 为按钮全局坐标下方
popup->setVisible(!popup->isVisible());
```

**QListView + 自定义 delegate**：
- `HintMessageDelegate` 继承 `QStyledItemDelegate`
- 绘制内容：消息文本（elide）+ 可选操作按钮 + 关闭按钮
- 未读消息文本加粗 + 左侧指示色块，已读消息普通色（视觉区分）
- delegate 维护 `hovered_index` + `hovered_region`，paint 时绘制 hover 高亮（操作按钮 / 关闭按钮可点区域反馈）
- `editorEvent` 处理操作按钮 / 关闭按钮点击（判断坐标区域）
- `sizeHint` 返回固定行高

**Model**：`MessageService::instance()` 即为 model（继承 `QAbstractListModel`），QListView 直接 `setModel(&MessageService::instance())`。插入消息使用 `beginInsertRows` / `endInsertRows`。

**交互**：
- 点击操作按钮 -> 触发回调 + 标记已读
- 点击关闭按钮 -> 删除该消息（若删除后列表为空则关闭 popup）
- 一键已读 -> 所有消息标记已读
- 清空 -> 删除所有消息 + 关闭 popup
- 打开 popup 时无消息 -> 显示"暂无消息"占位

### HintButton 信号

```cpp
signals:
  void unreadCountChanged(int count);  // 未读数变化，驱动图标切换
```

### 主题切换

`HintButton` 内部监听 `ThemeManager::themeChanged` 信号，自行刷新图标（`message` / `message_alert` 的 `_light` / `_dark` 版本），不依赖外部通知。

### hint_bar_ 移除

| 移除项 | 位置 |
|--------|------|
| `HintBarWidget` 类 | `src/app/widgets/HintBarWidget.h` / `.cpp` |
| `hint_bar_` 成员 | `MainWindow.h` |
| `hint_bar_` 创建 + 添加布局 | `MainWindow.cpp` initUi 209-210 行 |
| DEBUG 测试消息 | `MainWindow.cpp` lazyInit 1295-1306 行，改调 `MessageService::instance().postHint(...)` |
| QSS 样式 | `#HintBar` / `#HintItem` / `#HintIndicator` / `#HintText` / `#HintActionBtn` / `#HintCloseBtn` |
| CMake 源文件 | `HintBarWidget.cpp` / `.h` 从构建列表移除 |

### QAB 集成

替换 `MainWindow.cpp` setupRibbon 中 Line 2177 的 `qab->addAction(nullptr); // TODO`：

```cpp
// ── QAB 消息提示按钮 ──
hint_button_ = new HintButton(this);
qab->addWidget(hint_button_);
```

DEBUG 测试消息从 `lazyInit` 移到 `hint_button_` 创建后（或保留在 `lazyInit` 中改为 `hint_button_->postHint(...)`）。

## 讨论点

### 讨论 1：消息列表是否设上限

**决策**：无上限。QListView 可滚动，多少条都能查看。

### 讨论 2：消息持久性

**决策**：不持久化。提示消息是即时性的，重启后清空。

### 讨论 3：DEBUG 测试消息位置

**决策**：保留在 `lazyInit` 中，改为 `hint_button_->postHint(...)`。语义是整个应用程序的提示，不局限于项目打开后。

### 讨论 4：操作按钮在 delegate 中的交互

**决策**：delegate `paint` 绘制按钮外观，`editorEvent` 判断点击坐标。每条消息的操作按钮跟随消息本身，与 HintBarWidget 行为一致。

### 讨论 5：Popup 顶部工具栏

Popup 窗口顶部增加工具栏，含两个按钮：
- **一键已读**：将所有未读消息标记为已读，小红点消失
- **清空**：清空所有消息，popup 关闭

## 决策记录

| 讨论点 | 决策 |
|--------|------|
| 1 消息列表上限 | 无上限，QListView 滚动查看 |
| 2 消息持久性 | 不持久化，重启清空 |
| 3 DEBUG 测试消息位置 | 保留 lazyInit，语义为应用程序级提示 |
| 4 操作按钮交互 | delegate paint + editorEvent 判断坐标 |
| 5 Popup 顶部工具栏 | 一键已读 + 清空 |

## 影响分析

### 新增文件

| 文件 | 说明 |
|------|------|
| `src/app/widgets/HintButton.h` / `.cpp` | 消息提示按钮组件（QToolButton + 图标切换 + popup 显隐） |
| `src/app/widgets/HintPopup.h` / `.cpp` | Popup 窗口（QListView + 底部工具栏） |
| `src/app/widgets/HintMessageDelegate.h` / `.cpp` | QListView 自定义 delegate |
| `src/app/widgets/MessageService.h` / `.cpp` | 单例消息服务（继承 QAbstractListModel，含 HintData 定义、postHint 入口、unreadCount） |
| `src/app/resources/icons/svg/message_dark.svg` / `_light.svg` | 无未读消息图标 |
| `src/app/resources/icons/svg/message_alert_dark.svg` / `_light.svg` | 有未读消息图标（小红点） |

### 修改文件

| 文件 | 改动 |
|------|------|
| `src/app/MainWindow.h` | 移除 `hint_bar_`，新增 `hint_button_` |
| `src/app/MainWindow.cpp` | initUi 移除 hint_bar_ 创建（editor_area_layout 仅剩 dock_manager_，无需调整）；setupRibbon QAB 替换 TODO；lazyInit DEBUG 消息改调 hint_button_ |
| `src/app/CMakeLists.txt` | 移除 HintBarWidget，新增 HintButton / HintPopup / HintMessageDelegate / MessageService |
| `src/app/resources/styles/vscode.qss` / `default.qss` | 移除 HintBar 样式块，新增 HintButton / HintPopup / delegate 样式块 |

### 删除文件

| 文件 | 说明 |
|------|------|
| `src/app/widgets/HintBarWidget.h` / `.cpp` | 完全移除 |

不涉及引擎层、数据层改动，仅 UI 层。

## 消息发送点设计

消息从 lazyInit 一处打散到 6 个业务流程，覆盖不同模块。每条消息的回调切换到对应页面。

### 前提：MainWindow 暴露 navigateTo

```cpp
// MainWindow.h (public)
void navigateTo(int page, const QString& sidebarId = {});
```

实现：`central_stack_->setCurrentIndex(page)` + `sidebar_->switchPage(sidebarId)` + `activity_bar_->setActivePageId(sidebarId)`。

### 6 个发送点

| 编号 | 发送位置 | 时机 | 消息文本 | 回调（切换到） |
|------|---------|------|---------|--------------|
| 1 | `MainWindow::onProjectOpened` | 项目打开成功 | "项目「{name}」已打开" | page0 + 项目概览 |
| 2 | `MainWindow::lazyInit` 第 7 步 ICD 加载后 | 协议仓库解析完成 | "ICD 协议仓库解析完成" | page0 + 协议 |
| 3 | `MainWindow::lazyInit` 第 8 步插件加载后 | 硬件插件加载 | "已加载 {N} 个硬件插件" | page0 + 硬件 |
| 4 | `MainWindow` 调用 `syncProjectTopologies` 后 | 拓扑同步完成 | "拓扑数据已同步" | page0 + 拓扑 |
| 5 | `ExecutionPanelController::engineFinished` 信号 | 执行完成 | "测试执行完成 P{pass} F{fail}" | page1 执行仪表盘 |
| 6 | `MainWindow::onProjectOpened` 中 `syncControlStates` 后直接查询 `programPopup()->allPaths().size()` | 程序扫描完成 | "发现 {N} 个测试程序" | page0 + 测试程序 |

### 改动点

1. **MainWindow.h**：新增 `void navigateTo(int page, const QString& sidebarId = {})` 公共方法
2. **MainWindow.cpp**：
   - 实现 `navigateTo`
   - `onProjectOpened` 末尾发消息 1
   - `lazyInit` 第 7 步发消息 2，第 8 步发消息 3
   - `syncProjectTopologies` 调用后发消息 4
   - 连接 `ExecutionPanelController::engineFinished` 信号发消息 5
   - `onProjectOpened` 中 `syncControlStates` 后直接查询 `allPaths().size()` 发消息 6
   - **删除原 lazyInit 第 9 步的 5 条测试消息**
3. **ExecutionPanelController**：新增 `engineFinished(int pass, int fail)` 信号，转发自 `TestExecutionEngine::engineFinished`

回调均为 lambda 捕获 `this`（MainWindow），调 `navigateTo` 切换页面。
