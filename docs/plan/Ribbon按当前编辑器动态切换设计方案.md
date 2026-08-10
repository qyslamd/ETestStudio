# Ribbon 按当前编辑器动态切换设计方案

## 1. 问题陈述

当前主程序（IDE 模式）的 Ribbon 是静态的，`MainWindow::setupRibbon()` 一次性创建
固定分类页（编辑 / 执行 / 工具 / 帮助），不感知当前打开的是哪个编辑器。

与此同时，四个专用编辑器（拓扑 / 协议 / 测试程序 / 运行配置）为了能作为独立工具
（独立 .exe）运行，都自建了 QMainWindow 工具栏。在 IDE 嵌入模式下，
`setEmbeddedMode(true)` 只隐藏了 menuBar，**没有隐藏自带工具栏**，导致：

- dock 内每个专用编辑器都渲染一条自己的工具栏；
- Ribbon 的「编辑」页又提供一套通用编辑命令（撤销/重做/剪切/复制/粘贴/查找/替换）。

两层工具栏并存、互不感知，界面冗余，视觉上"呆"，且编辑命令分散在两个入口。

另一个不一致点：Mock 响应编辑器没有工具栏（按钮内联在表单页里），体验与其它编辑器
割裂。

### 目标

Ribbon 成为 IDE 模式下唯一的编辑命令入口：**常驻全局页 + 一个随当前编辑器出现的
上下文页**。dock 内不再存在第二套编辑器工具栏。

## 2. 架构回顾（现状）

### 2.1 Ribbon 结构（MainWindow.cpp:2086 setupRibbon）

| 分类页 | 内容 |
|---|---|
| 编辑 | 文件 panel（新建项目/打开项目/打开文件/保存）、编辑 panel（撤销/重做/剪切/复制/粘贴/查找/替换/跳转行）、视图 panel（欢迎页/侧边栏/日志/终端/辅助侧边栏） |
| 执行 | 运行配置（验证）、执行控制（运行/运行全部/暂停/停止/清空数据）、统计 |
| 工具 | 设置、检查硬件设备、四个独立工具启动入口 |
| 帮助 | 关于 |

- 应用按钮（文件）与 QuickAccessBar（新建项目/打开项目/保存/撤销/重做/登录/设置/
  文件搜索/消息提示）为常驻区，本文不涉及。
- 「编辑」页通用编辑动作经 `editor_controller_`（EditorPanelController）路由。需注意：
  **只有 undo/redo 走 `IEditor::undo()/redo()` 接口**；剪切/复制/粘贴/查找/替换/跳转行
  在 EditorPanelController 内部 `dynamic_cast<TextEditorWidget>`，对图片/etlog/运行配置
  等非文本编辑器是空操作（见 MainWindow.cpp:1941-1971）。

### 2.2 编辑器侧（现状）

| 编辑器 | 类 | 基类 | 自带工具栏 | 工具栏内容 | 嵌入时隐藏工具栏 |
|---|---|---|---|---|---|
| 拓扑 | TopologyEditorWidget | QMainWindow | 有（拓扑工具栏） | 撤销重做/复制/粘贴/删除/对齐×6/分布×2/缩放×4/导出/清理 + 3 个面板开关，约 16 个 QAction | 否 |
| 协议 | ProtocolEditorWidget | QMainWindow | 有（帧工具栏） | 撤销重做/新帧/删帧/加删节点/字节序(控件)/帧类型(控件)/3 个面板开关 | 否 |
| 测试程序 | TestProgramEditorWidget | QMainWindow | 有（测试程序工具栏） | 撤销重做/用例增删/步骤增删/上移下移/标签方向 | 否 |
| 运行配置 | RunConfigEditor | QMainWindow | 有（运行配置工具栏） | 撤销重做/新建/保存/对齐(按钮)/分布(按钮)/测试程序/可视化组件/属性开关 | 否 |
| Mock | MockConfigEditor | QWidget | 无（按钮内联在表单） | - | - |
| 文本 | TextEditorWidget | QWidget | 无 | - | - |
| 图片 | ImageViewerWidget | QWidget | 无 | - | - |
| etlog | EtlogViewerWidget | QWidget | 无 | - | - |

四个专用编辑器在嵌入式模式下隐藏 menuBar（setEmbeddedMode 内 `menuBar()->hide()`），
但均未隐藏 `toolbar`。编辑器类型注册共 8 类，见
EditorManager.cpp:124/131/155/182/205/333/363/368。

### 2.3 已有扩展点

- `EditorManager::currentEditorChanged(IEditor*)` 信号已存在，MainWindow.cpp:774 已连接
  （当前用于保存/关闭动作使能 + 状态栏同步）。这是上下文页切换的挂接点。
- `EditorManager::currentEditor()` 返回当前激活编辑器。
- SARibbon 2.5.7 支持运行时 `hideCategory(SARibbonCategory*)`、
  `showCategory(SARibbonCategory*)`、`insertCategoryPage(title, index)`、
  `categoryByName(title)`、`setCurrentIndex(index)`。整页显隐切换在技术上无障碍
  （SARibbonBar.h:181/185/194/197/305，均经核实存在）。
- 注意 `showCategory` 恢复的是隐藏时记录的 index，而非固定插入位
  （SARibbonBar.cpp:718）。见决策 D14 的顺序约束。
- Ribbon 与中央 QStackedWidget 之间存在双向联动：ribbon tab 变化映射到
  central_stack page0/page1，反向 `raiseCategory`（MainWindow.cpp:1213-1244、
  :2844-2848），以 `switching_page_` 守卫防回环。上下文切换逻辑必须与其共用守卫。

## 3. 目标设计

### 3.1 总体模型

Ribbon = **常驻全局页**（不随编辑器变化）+ **至多一个上下文页**（随当前编辑器切换，
出现在 tab 最左、文件按钮之后；纯空态为零）。

命令与焦点同步。

### 3.2 常驻页

| 分类页 | 内容 | 说明 |
|---|---|---|
| 文件（应用按钮） | 文件菜单：新建项目/打开项目/打开文件/保存（自「编辑」页文件 panel 并入）+ 现有文件菜单项 | Office 文件菜单惯例；QAB 已含保存/新建项目/打开项目，补齐打开文件 |
| 视图 | 欢迎页 / 侧边栏 / 日志 / 终端 / 辅助侧边栏 | 从「编辑」页的视图 panel 迁出，成为独立常驻页 |
| 执行 | 运行配置 / 执行控制 / 统计 | 现有，不变 |
| 工具 | 设置 / 硬件 / 独立工具启动 | 现有，不变 |
| 帮助 | 关于 | 现有，不变 |

### 3.3 上下文页（随编辑器切换）

| 激活编辑器 | 上下文页标题 | 内容 |
|---|---|---|
| 拓扑 | 拓扑 | 撤销重做 · 复制/粘贴/删除 · 对齐×6 · 分布×2 · 缩放×4 · 导出/清理 · 3 个面板开关 |
| 协议 | 协议 | 撤销重做 · 新帧/删帧 · 加/删节点 · 字节序 · 3 个面板开关 |
| 测试程序 | 测试程序 | 撤销重做 · 用例增删 · 步骤增删/上移下移 · 标签方向 |
| 运行配置 | 运行配置 | 撤销重做 · 新建/保存 · 对齐×6 · 分布×2 · 测试程序/可视化组件/属性开关 |
| Mock | Mock | 创建配置 · 添加/删除行 · 新建/删除响应 · 删除端口配置 |
| 文本 / 图片 / etlog | 编辑（兜底页） | 撤销重做 · 剪切/复制/粘贴 · 查找/替换/跳转行 |

规则：

- 上下文页 = 该编辑器的**全部**编辑命令（通用编辑命令 + 特有命令合并），用户不跨 tab
  找按钮。
- 上下文页**至多存在一个**：专用编辑器激活时显示对应页；普通编辑器（文本/图片/
  etlog）激活时显示「编辑」兜底页；**纯空态（欢迎页，无激活编辑器）不显示任何上下文
  页**（见 D16）。
- 兜底「编辑」页的剪切/复制/粘贴/查找/替换/跳转行**按编辑器类型控制 enabled**（只对
  文本编辑器启用），避免对图片/etlog 等显示无效按钮。
- 上下文页带强调色标题（Office 上下文选项卡惯例），让"当前正在操作什么"可视化。
- QuickAccessBar 保留常驻撤销/重做（跨上下文高频），与上下文页内那份并存但不同屏。

### 3.4 工具栏去留

- 四个专用编辑器（拓扑/协议/测试程序/运行配置）在 IDE 嵌入模式下**不再渲染自带
  工具栏**（embedded 分支隐藏，或嵌入模式下不创建）。
- 独立工具模式（独立 .exe）不受影响，仍是自有的 QMainWindow + 菜单栏 + 工具栏。

### 3.5 命令定义模式（命令的唯一来源）

Ribbon 上下文页的动作采用**命令定义模式**，而非共享 QAction 实例：

- 每个编辑器暴露一份**命令清单**：一组命令描述，每条含分组、标题、图标、
  checkable、默认 enabled、触发回调、状态查询与状态变化信号。
- Ribbon 上下文页在编辑器激活时**按清单清空重建**自己的 QAction（parent 为 Ribbon 侧），
  触发时调用编辑器的命令回调；编辑器状态变化经其信号通知 Ribbon 刷新动作的
  enabled/checked。
- 编辑器关闭 → 上下文页随之清空，不存在跨生命周期共享 QAction 的悬垂问题。
- 独立工具模式复用同一份命令清单构建自带工具栏，命令定义单一来源，两种模式一致。

非命令类内联控件不进 Ribbon 命令清单，保留在编辑器内部：

- 协议的字节序切换：改造成 checkable QAction 后进入命令清单；
- 协议的帧类型下拉、拓扑的缩放比例标签：属编辑器内部状态选择/显示，非操作命令，
  保留编辑器内。

### 3.6 状态实时同步

- 是否有选中元素（拓扑复制/粘贴/删除、协议删节点等）→ enabled；
- 能否撤销/重做（脏历史栈）→ enabled；
- 面板开合（协议/运行配置的 checkable 开关）→ checked。

同步机制：编辑器在内部状态变化时发出统一"命令状态变化"信号（现有 undoStateChanged、
modificationChanged 可并入），Ribbon 侧刷新动作。禁止轮询。

### 3.7 上下文页与 central_stack 的互斥联动

Ribbon 各分组按"操作对象"归类，点击时联动 central_stack。沿用现有
MainWindow.cpp:1213-1272 机制（`switching_page_` 守卫防回环、切 page1 时
`raiseCategory(category_exec_)`、切 page0 时不主动 raise），新增上下文页归入 page0 侧：

| 分组 | 操作对象 | 点击联动 |
|---|---|---|
| 上下文页（拓扑/协议/测试程序/运行配置/Mock/编辑兜底） | page0 | 切到 page0 |
| 视图 | page0 | 切到 page0 |
| 执行 | page1 | 切到 page1，并 raiseCategory |
| 工具 / 帮助 | 公共 | 不切页 |

上下文页**可见性**由两个输入共同决定：

- 当前激活编辑器（`currentEditorChanged`）：决定显示哪个上下文页；
- central_stack 当前页（`QStackedWidget::currentChanged`）：page0 显示、page1 隐藏。

规则：

- page0 下至多显示一个上下文页（对应激活编辑器，普通编辑器为「编辑」兜底，纯空态
  一个不显示）。
- 切到 page1（运行态）：hideCategory 全部上下文页，命令条只剩执行 + 全局页；
  回 page0 时按当前激活编辑器恢复对应上下文页（无激活编辑器则不恢复）。
- 上下文页命令的 enabled 仍与引擎运行锁联动（运行中禁用），与现有"编辑锁由引擎
  状态驱动"机制一致。
- **守卫约束**：上下文页的 hide/show 属于 tab 集合管理，不主动修改 central_stack
  页面；但 SARibbon 的 `showCategory` 会无条件 `raiseCategory` 并 emit
  `currentRibbonTabChanged`，`hideCategory` 在移除当前选中 tab 时也会 emit
  （SARibbonBar.cpp:724/728/700）。因此所有 hide/show 必须以 `switching_page_`
  save/restore 守卫包裹执行，且落在 `central_stack currentChanged` handler 的
  `if (!switching_page_)` **之外**（点 tab 切页时该条件为假会跳过 guard 块，需以显式
  置位包裹覆盖）；`showCategory` 仅在 page0 下调用。统一抽取
  `applyContextPageVisibility()`（内部 save/restore `switching_page_`），由
  `central_stack currentChanged` 与 `currentEditorChanged`（page0 时）共同调用：
  `currentEditorChanged` 槽更新"目标上下文页"语义，central 处于 page0 时在守卫内
  执行对应上下文页的 show/hide，处于 page1 时只更新语义、保持隐藏，避免误切回
  page0。
- 回 page0 恢复上下文页时会 raise 它（ribbon 跳到上下文页而非停留被点的 tab），
  属预期行为（D2/D7 命令随焦点同步），已记录于验收 10。

## 4. 方案选项对比

### 方案 A：按编辑器类型整页切换 + 命令定义模式（推荐）

- 每个编辑器类型预建一个隐藏的 SARibbon 上下文分类页；编辑器激活时按其命令清单
  清空重建面板动作。
- `currentEditorChanged` 时 show 对应页、hide 其余；普通编辑器（文本/图片/etlog）
  回退「编辑」兜底页，纯空态不显示任何上下文页。
- embedded 模式下四个专用编辑器隐藏自带工具栏。
- 优点：整页显隐是 SARibbon 原生能力；命令定义单一来源，无 QAction 悬垂；独立工具
  模式复用同一命令清单；兜底页与上下文页结构对称，代码路径统一。运行时"清空重建
  panel"经核实无障：SARibbonPanel::actionEvent 处理 ActionRemoved 并清理内部项
  （SARibbonPanel.cpp:1106-1115），且 QAction 由 Ribbon 侧持有可自行 delete。
- 缺点：编辑器工具栏需重构为"命令清单"形态；状态同步桥需要实现。

### 方案 B：单一「上下文」页动态重建

- 保留「编辑」页不动，另加一个上下文页，切换编辑器时清空重建其 panel/动作。
- 依赖 SARibbon 运行时删除 panel/action 的 API，需验证版本支持度。
- 优点：只有一个动态页，状态管理集中。
- 缺点：每次切换都重建 UI，有闪烁风险；依赖 API 不确定；上下文概念不清。

### 方案 C：通用命令留「编辑」页，仅特有命令进上下文页

- 「编辑」页不动；上下文页只放特有命令。
- 优点：改动最小，撤销/重做等通用命令只维护一份。
- 缺点：通用命令与特有命令分跨两个 tab，用户切页频率高；「编辑」页与上下文页
  "两层命令"的矛盾未根治，与目标（单一编辑命令入口）冲突。

### 决策

- 采用**方案 A**：整页切换 + 命令定义模式。
- 撤销/重做等通用命令进上下文页（而非留在「编辑」页），保证"单一编辑命令入口"；
  兜底「编辑」页承担普通编辑器场景，与上下文页不同屏，无重复感。
- 四个专用编辑器隐藏 embedded 模式自带工具栏；Mock 编辑器获得上下文页（内联按钮保留）。

## 5. 决策记录

| 编号 | 决策 | 理由 |
|---|---|---|
| D1 | 采用 Office 上下文选项卡模型（常驻页 + 至多一个上下文页，纯空态为零） | 用户已建立心智模型，无需教育；单命令入口最直观 |
| D2 | 上下文页在 tab 最左、文件按钮之后 | 编辑命令是高频区，位置最醒目 |
| D3 | 上下文页 = 通用编辑命令 + 特有命令合并 | 消除跨 tab 找按钮；兜底页与上下文页不同屏无重复 |
| D4 | 通用编辑命令不单独保留「编辑」页 | 「编辑」页退化为普通编辑器的兜底上下文页 |
| D5 | 四个专用编辑器 embedded 模式隐藏自带工具栏 | 根治双层工具栏；独立工具模式不受影响 |
| D6 | Mock 编辑器纳入上下文页体系，内联按钮保留 | 统一体验；表单内就地编辑仍更自然 |
| D7 | 上下文页带强调色标题 | 让上下文切换可视化，符合 Office 惯例 |
| D8 | 状态同步走"编辑器内部信号 → Ribbon 刷新动作" | 复用现有 undoStateChanged 等信号，避免轮询 |
| D9 | RunConfigEditor 纳入上下文页体系 | 第 4 个带工具栏编辑器，原则统一；上下文页共 6 个（5 专用 + 1 兜底「编辑」） |
| D10 | 命令采用"命令定义模式"，QAction 由 Ribbon 侧创建 | 无跨生命周期悬垂；统一处理 widget 项；独立工具复用命令清单 |
| D11 | 字节序改 checkable QAction 入命令清单；帧类型下拉/缩放标签保留编辑器内 | 非命令类内联控件不进命令清单 |
| D12 | 文件 panel 4 动作并入应用按钮文件菜单 | Office 文件菜单惯例，QAB 已含高频动作 |
| D13 | 兜底「编辑」页按编辑器类型控制剪切/复制/粘贴等 enabled | 现状该组动作对非文本编辑器是空操作，避免无效按钮 |
| D14 | 上下文页 insert 到 index 0 后立即 hide，确保 showCategory 恢复位置正确 | showCategory 恢复隐藏时记录的 index，须先固定为 0 |
| D15 | 上下文页可见性随 central_stack 联动：page1 隐藏、page0 按激活编辑器恢复；hide/show 在 `switching_page_` 守卫作用域内执行 | 运行态下编辑器不可见，命令条应反映执行焦点而非编辑命令；避免上下文页 tab 常驻堆叠；`showCategory` 无条件 raise 会触发 ribbon tab 信号，必须在守卫内防误切 |
| D16 | 纯空态（欢迎页，无激活编辑器）不显示任何上下文页 | 启动/无文件时显示一个命令全禁用的「编辑」tab 突兀；仅当激活普通编辑器才显示兜底页 |

## 6. 影响范围与实现要点（可行性）

本设计涉及的改动集中在 app 层，下层模块（topology / protocol / test_program）以
静态库形式被 app 链接，无模块间依赖新增：

- `src/app/MainWindow.cpp`：setupRibbon 拆分常驻页与上下文页创建；currentEditorChanged
  槽中追加上下文页目标语义更新（page0 下守卫内执行 show/hide）；新建
  `applyContextPageVisibility()` 与"命令清单 → 上下文页动作"的填充及状态同步桥；
  文件动作并入应用按钮菜单。
- `src/app/EditorManager.cpp`：编辑器激活/关闭时向 MainWindow 报告其命令清单的入口
  （或由 MainWindow 在 createEditor 后查询）。
- 四个专用编辑器（拓扑/协议/测试程序/运行配置）：工具栏重构为命令清单形态；embedded
  分支隐藏自带工具栏；暴露命令状态变化信号。
- Mock 编辑器：抽象出对应 Ribbon 命令的命令清单与槽。
- `src/app/MainWindow.cpp`（`syncEditorActions`）：兜底「编辑」页的剪切/复制/粘贴/查找/
  替换/跳转行 enabled 按当前编辑器类型门控（仅文本编辑器启用）。
- 上下文切换与现有 ribbon↔central_stack 双向联动共用 `switching_page_` 守卫，避免
  用户停留在执行页(page1)时被误切回 page0。
- 上下文页显示语义：`showCategory` 内部会 `raiseCategory`，切换编辑器时 ribbon 当前
  tab 会激活到最左的上下文页。这是预期行为（命令随焦点同步，符合 D2/D7），非缺陷；
  守卫只负责阻止 central_stack 被误切。
- SARibbon 依赖能力（hideCategory/showCategory/insertCategoryPage）已确认存在于 2.5.7。

命令清单的载体形态：扩展 `IEditor` 接口，还是引入独立的贡献者接口
（如 `IEditorRibbonContributor`）。倾向后者，避免把 Ribbon 概念渗入 IEditor。

## 7. 验收标准

1. 打开拓扑编辑器，Ribbon 出现「拓扑」上下文页（强调色），含拓扑全部命令；dock 内
   无第二条工具栏。
2. 在拓扑与协议之间切换 tab，上下文页随之切换，无残留、无闪烁。
3. 关闭所有专用编辑器：回到文本/图片/etlog 编辑器则显示「编辑」兜底页；回到欢迎页
   （纯空态）则一个上下文页都不显示。
4. 命令状态实时：无选中元素时复制/粘贴/删除禁用；撤销栈空时撤销禁用；面板开关
   checkable 跟随实际开合。
5. 兜底「编辑」页的剪切/复制/粘贴/查找/替换仅在文本编辑器下启用，其余编辑器禁用。
6. 独立工具模式（topology-editor / protocol-editor / test-program-editor）外观与功能
   不变，命令与 IDE 模式同源。
7. 运行配置编辑器打开时出现「运行配置」上下文页；Mock 编辑器出现「Mock」上下文页，
   命令可触发对应表单操作。
8. 执行/视图/工具/帮助常驻页在任何编辑器下保持不变；上下文切换不打断中央页面
   （page0/page1）的停留状态。
9. 切到执行页（page1）时上下文页全部隐藏，命令条只剩执行 + 全局页；切回 page0 时
   按当前激活编辑器恢复对应上下文页（无激活编辑器则一个都不显示）。
10. 点击上下文页 tab / 视图 tab → 切到 page0；点击工具/帮助 → 不切页；点击执行 →
   切到 page1。`switching_page_` 守卫下无回环、无误切。恢复上下文页时 ribbon 跳到
   上下文页而非停留被点的 tab，属预期（命令随焦点同步）。
