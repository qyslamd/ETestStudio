# MonitorConfigDialog 迁移到运行编辑器方案（讨论稿）

> 状态：**讨论中**（草案，未定稿）。本文档围绕 `MonitorConfigDialog` 的去向展开讨论，结论落定后更新。

## 一、现状

`MonitorConfigDialog` 双栏对话框（左连接列表 + 右 visualizer 瓦片，非模态，决策 15/16/18），当前有两个宿主：

| 宿主 | 用法 | 当前状态 |
|---|---|---|
| **运行编辑器**（`RunConfigEditor::onAddMonitorClicked`） | 只接 `visualizerChosen`（添加监听器），写入 `config_.monitors` | 可用（无改名/删除/勾选） |
| **运行态**（`ExecutionPanelController::showChannelSelectionDialog`） | `channel_dialog_` + ribbon「通道选择」入口 | **UI 壳保留但失效**：5 个交互槽注释（D1-g），打开能显示连接/监听器，点类型/改名/删除无反应 |

监听器写回已收敛：只有运行编辑器写 `.erun`（`RunConfigEditor` 的 visualizerChosen 槽），运行态只读。

## 二、已有规划（D1 文档 4.9，明确"后续阶段，本阶段不实施"）

1. `RunConfigEditor` 以 **QDockWidget 呈现**监听器配置面板（仿「测试程序」dock：toggle action + DockTitleBar + 事件过滤），内容复用对话框的左右栏交互（连接列表 + 类型瓦片 + 改名/删除）。
2. 补齐运行编辑器交互：`renameRequested`/`deleteRequested`/`checkToggled`（当前只接 `visualizerChosen`）。
3. 运行态删除：`channel_dialog_`、`showChannelSelectionDialog`、五个槽、ribbon「通道选择」入口，运行态回归纯只读。
4. 配置后的可视化区联动：运行编辑器内直接作用于自身 `vis_area_`；运行态经 mtime 级联刷新看到结果。

## 三、目标（已确认）

- 监听器配置**完全收窄到 RunConfigEditor**：运行态移除 `MonitorConfigDialog` 任何入口（`channel_dialog_`/`showChannelSelectionDialog`/ribbon「通道选择」），监听器完全由运行编辑器配置进 `.erun`。
- **打破现有对话框设计**：围绕 RunConfigEditor 的 UI/UX 重新设计监听器配置的呈现形态与交互。
- 当前两个宿主：运行编辑器（`onAddMonitorClicked`，只接 `visualizerChosen`）+ 运行态（UI 壳失效）。

## 四、UI 形态方案（已确认方向：仿拓扑编辑器双 dock）

**交互范式**（对照拓扑 `device_palette_dock_` + `property_dock_`）：

1. **visualizer 调色板 dock**：所有 visualizer 类型的缩略图/真实渲染图列表或网格。
2. **拖放**：从调色板拖 visualizer 到可视化区（场景）→ 创建监听器卡片（像拓扑放置硬件）。
3. **属性 dock**：场景中选中一个 visualizer 卡片 → 属性 dock 加载其属性。
4. **属性配置**：在属性 dock 配置名称、**绑定连线**（选择拓扑连接）。

**交互顺序反转**：原对话框"先选连接 → 再配类型"（连接驱动）→ 新方案"先放 visualizer → 再绑连接"（visualizer 驱动）。

## 五、待讨论（开放问题）

1. **绑定连线时机/方式**：拖放后到属性面板绑定？拖放时是否需即时反馈（如高亮可绑定连接）？
2. **未绑定状态**：拖放后未绑连线的卡片显示什么（占位/提示）？绑定前是否出现在运行态/写入 `.erun`？
3. **一连接一监听器约束**：visualizer 驱动下，两个卡片绑同一连接如何约束/提示？
4. **调色板 visualizer 实例**：缩略图是静态预览还是可交互实例？拖放创建新卡片还是复用？
5. **属性面板内容**：除名称/绑定连线外，还含什么（类型切换？displayMode？布局？）？
6. **与 .erun 映射**：拖放位置 → `layout`，绑定连线 → `monitors.connectionId`，名称 → `monitors.name`，类型 → `monitors.displayMode`。

---

**讨论记录**（随讨论补充）：

1. **UI 形态**（用户确认）：双 dock 范式——visualizer 调色板（缩略图/渲染图网格）拖放进可视化区（仿拓扑放硬件）+ 属性 dock（选中卡片后加载属性，配置名称、绑定连线）。
2. **未绑定二级标题**：拖放后未绑定 → 二级标题显示「未绑定到连线」，用**主题警示语义色**渲染（红主题→黄、黄主题→红等，需 ThemeManager 新增 `warningColor()` + 各主题 JSON 配 `warning` 键）；绑定时二级标题显示连接描述。
3. **绑定控件**（复用 MonitorConfigDialog 左侧）：全部连接都列，**已绑定的连接视觉禁用 + 操作不可选**（一连接一监听器约束）；列表顶部保留搜索框。
4. **.erun 格式（关键修正）**：`Monitor` **自包含**，进场景分配 UUID：
   ```json
   monitors: [ { "id": "uuid", "connectionId": "conn-1"|"", "displayMode": "waveform", "name": "...", "x": 20, "y": 30, "w": 320, "h": 240 } ]
   ```
   `id` = 卡片实例 UUID（layout 关联 key，替代原独立 layout 数组的 connectionId）；`connectionId` 空 = 未绑定；`x/y/w/h` 几何并入 Monitor（**废弃独立 layout 数组**）。
5. **删除交互**（双入口）：场景右键菜单删除 + 属性 dock「删除监听器」按钮。
6. **运行态行为**：按 `.erun` 重建时**全部建卡（含未绑定）**——未绑定显示「未绑定到连线」（警示色）、不订阅数据；绑定订阅并显示连接描述。运行态反映完整配置态。
7. **属性面板内容**：名称、绑定连线（搜索 + 已绑定禁用）、**类型切换（displayMode）**、删除按钮。Monitor 面向对象化后，面板作为可生长属性容器，后续扩展属性水到渠成。
8. **属性面板布局**：dock 内用 **QScrollArea** 包裹属性表单（属性增多时内容可滚动，仿拓扑 property_dock）。
9. **调色板控件**：**QListView**（`ListMode` 默认列表 / `IconMode` 网格可切），item = visualizer 缩略图/渲染图 + 类型名，作为拖放源；不撑爆 dock（列表模式滚动）。
10. **拖放交互**：拖到场景时显示**跟随鼠标的半透明预览占位**（虚线框 + 类型名），release 在占位位置创建卡片（分配 UUID、未绑定）；卡片默认尺寸按各 visualizer 的 `sizeHint`。
11. **dock 布局**：左 = 测试程序 dock（现有）+ 调色板 dock（下方，可折叠）；右 = 属性面板 dock（选中卡片加载）；中央 = 可视化区。运行后按实际效果微调（用户：随便排，先跑起来）。
12. **拖放落点（审查 #4）**：`VisualizationArea` 视图层 `setAcceptDrops(true)` + override `dragEnterEvent/dragMoveEvent/dragLeaveEvent/dropEvent`（`mapToScene` 转场景坐标），半透明预览占位随鼠标；仿拓扑但走视图层（与现有"视图层接管鼠标事件"风格一致）。
13. **警示色上色（审查 #5）**：扩展 `SignalVisualizer::setSubtitleColor(QColor)`，各子类透传给副标题 label（缺省主题 `secondaryTextColor`）；未绑定卡创建时调 `ThemeManager::warningColor()`（etest_visualizer 已链 etest_core_ui）。
14. **warningColor 波及面（审查 #6）**：13 个主题 JSON 配 `warning` 键 + `gen_themes.py` 生成器输出 + `ThemePalette` 结构体加 `warningColor` + `ThemeManager::loadPaletteFromJson` 映射 + default 兜底 palette。漏一处就空色。
15. **属性面板 id 反查（审查 #7）**：`VisualizerProxy` 加 `setMonitorId/monitorId`（构造传入）；RunConfigEditor selectionChanged 槽 `qgraphicsitem_cast<VisualizerProxy*>` 取 id → 属性面板加载（在现有"≥2 门控"外补"单卡选中 → 加载属性"分支）。
16. **删除收敛（审查 #8）**：提炼 `removeMonitorById(const QString& id)`（saveSnapshot + removeAt + markModified），右键删除 + 属性按钮双入口都调它。
17. **清理（审查 #9）**：移除 `channel_dialog_`/`showChannelSelectionDialog`/`refreshMonitorTree`/ribbon「通道选择」入口（refreshMonitorTree 只服务对话框，成死代码）。
18. **拖放源 mime（审查 #14 补）**：mimeType = `application/x-etest-visualizer`，payload = `displayMode` 字符串；调色板 QListView item 的 UserRole 存 displayMode；场景 `dropEvent` 解析 mime → displayMode → 创建卡片。拖放源复用拓扑 `DeviceListWidget::startDrag` 模板，不需要真实 visualizer 实例做拖放源（item 缩略图即可）。
19. **卡片默认尺寸（审查 #16）**：各 visualizer 子类**重载 `QSize sizeHint() const`** 返回各自默认尺寸（waveform 320×200、led/gauge 120×120 等），拖放时用 `widget->sizeHint()`。
20. **信号改名（审查 #15）**：`VisualizationArea::visualizerClosed(connectionId)` → `visualizerRemoved(QString id)`，两处宿主（RunConfigEditor/ExecutionPanelController）同步。

## 六、审查定稿（3 🔴 已确认）

21. **key 语义（🔴1，已确认）**：`VisualizationArea` 的 key 从 `connectionId` 改为 **`monitor.id`**（API 形状保留 `QString` key）；调用方维护 id→connectionId 映射（运行态订阅/取消订阅、`visualizerRemoved(id)` 反查取消订阅）。
22. **fromJson 归一化（🔴2，已确认）**：去重键改 **`id`**（无 id 兜底 `QUuid::createUuid()`），**空 connectionId 保留**（未绑定合法）；追加格式不变量「非空 connectionId 全局唯一」（数据层兜底一连接一监听器）。
23. **运行态未绑定建卡（🔴3，已确认）**：`loadProjectMonitors` **过滤空 connectionId 不给 MonitorManager**；`rebuildVisualizers` 直接按 `run_config_.monitors` 建未绑定卡（key=id、不订阅、警示副标题）；MonitorManager 保持 connectionId-keyed 不动（engine 零影响）。
24. **运行态非订阅态区分（🔵10，已确认）**：空 connectionId（未绑定）与非空但拓扑无此连接（连接已删除）**都建卡**，警示副标题文案区分「未绑定到连线」/「连接已删除」；绑定控件里「连接已删除」归不可选组。
25. **类型切换实现（🔵10 补）**：属性面板切 displayMode → **单卡重建**（仿 `rebuildVisualizer`：撤旧建新，保留 id/连接/几何/名称，仅换 visualizer 类型）。

## 七、二轮审查定稿

26. **警示色机制（🔴A，已确认）**：副标题 label 加动态属性 `setProperty("state", "warning"|"deleted"|"normal")`，QSS 用属性选择器 `#WaveformSubtitle[state="warning"] { color: <warning>; }`（deleted 同理）；`gen_themes.py` 按主题注入各 QSS 副标题块（现有 13 主题同步补）；缺省 normal 用现有 QSS 规则。**废弃决策 13 的 palette 方案**（QSS 压制 + CLAUDE.md 禁 C++ setStyleSheet）。
27. **旧 `.erun` 格式彻底废弃（🔴B，已确认）**：旧格式（monitors + layout 数组、connectionId 关联）**完全不要**；`RunConfig::toJson/fromJson` **整个重写**为新格式（Monitor 自包含 id/connectionId/displayMode/name/x/y/w/h），**不写任何旧格式解析代码**（无兼容回填、无降级处理，旧格式不存在）。
28. **重订阅循环（🟡1）**：`syncProjectTopologies` 拓扑重载后的重订阅循环改经 id→connectionId 反查（`activeChannels()` 返回 id）；**删"移除失效卡"分支**（失效卡由 rebuildVisualizers 建卡 + 警示副标题，符合决策 24）；仅对有效卡重订阅。
29. **RunConfigEditor 侧清理（🟡2）**：除运行态外，RunConfigEditor 同步移除 `channel_dialog_`/`onAddMonitorClicked`/`add_monitor_action_`（工具栏「添加监听器」入口由调色板 dock 取代）。
30. **双去重（🟡3）**：fromJson 两条去重并存——`seenIds`（卡片身份去重）+ `seenConnectionIds`（**非空** connectionId 去重，保留首个）；空 connectionId 不进 connectionId 去重集。
31. **QSS 副标题注入（🟡4）**：决策 14 波及面补「各主题 QSS 副标题块加 `[state=]` 属性选择器」（gen_themes.py 生成 + 现有 13 主题补），与 26 合并。
32. **采纳的 🔵**：`VisualizerProxy` 由 `addVisualizer` 内 `setMonitorId(key)`（非构造传入）+ selectionChanged 补"取消选中 → 清空属性面板"；`activeChannels()`/`VisualizerGeometry::connectionId` 改名（`monitorIds()`/`VisualizerGeometry::id`）；widget 内部仍以 connectionId 画 trace（id→connectionId 映射单向，建卡时把 connectionId 传给 widget）；未绑定可保存不硬拦（保存时可选软提醒）；`collectLayout`/`refreshUi` 几何读写改造点名；`MonitorConfigDialog` 类整体删除（`createPreviewVisualizer` 迁到调色板 dock 复用）；LED 副标题 QSS 拼写统一（`#LEDSubtitle` → objectName `LedSubtitle`）。

## 八、实施计划

> 依赖：T1/T2/T5 可并行（不同模块）→ T3/T4（依赖 T2）→ T6/T7（组件）→ T8（集成）→ T9（运行态）→ T10（清理）→ T11（全量编译+手动验证）。构建：`scripts/build_ninja.bat -t debug -m ETestStudio`。
>
> **中间态编译策略（终审 🔴1）**：T1-T7 期间 `RunConfig`/`VisualizationArea` 破坏性变更会使 app 构建中断（消费方引用旧成员/旧 API）。**此阶段允许 app 构建中断**，验证以组件级编译为准（RunConfig 单测、etest_visualizer 单测）；T8（RunConfigEditor 集成）与 T9（运行态适配）为里程碑，完成后再全量编译。

### T1 `RunConfig` 格式重写（旧格式彻底废弃，决策 4/21/22/27/30，终审 🔴1）

- **文件**：`src/app/editors/RunConfig.h/.cpp`；新增 `tests/app/run_config_test.cpp`（RunConfig 单测，`add_etest` 链接 spdlog/logger，终审 🔵13）
- `Monitor` 结构加 `id`（QString）、`x/y/w/h`（double）；`connectionId` 语义改"绑定连线，空=未绑定"；**删 `LayoutItem` 与 `layout` 数组**
- `toJson` 全重写：monitors 每项写 `{id, connectionId(可省略), displayMode, name, x, y, w, h}`；**不写 layout**；`version` 升 **2.0**（终审 🔵9）
- `fromJson` 全重写：**双去重** `seenIds` + `seenConnectionIds`（仅非空进后者，保留首个）；空 connectionId 保留；无 id 兜底 `QUuid::createUuid().toString(QUuid::WithoutBraces)`；**不解析旧 layout（旧格式不存在）**
- **消费方中断**：删 layout 后 `RunConfigEditor.cpp`（`config_.layout` 遍历 / `collectLayout`）与 `ExecutionPanelController.cpp`（`run_config_.layout` 遍历）编译断——按顶部中间态策略，此阶段允许，语义适配在 T8/T9
- **验证**：`RunConfig` 单测（toJson→fromJson 往返含未绑定/绑定混合、双去重、空 connectionId 保留）

### T2 `VisualizationArea` key 迁移 + 信号改名（决策 21/32，终审 🟡3）

- **文件**：`src/visualizer/VisualizationArea.h/.cpp`、`src/app/editors/RunConfigEditor.cpp`、`src/app/ExecutionPanelController.cpp`（后两者仅**最小调用点适配**：`activeChannels()`→`monitorIds()`、`visualizerClosed`→`visualizerRemoved`、`g.connectionId`→`g.id`，语义适配留 T8/T9）
- key 语义 connectionId → **monitor.id**：`addVisualizer/removeVisualizer/visualizer/setVisualizerGeometry/visualizerGeometries` 参数/返回改 id；`VisualizerGeometry::connectionId` → `id`；`activeChannels()` → `monitorIds()`
- `visualizerClosed(connectionId)` → **`visualizerRemoved(QString id)`**
- 内部 `items_` key 改 id
- **验证**：etest_visualizer 单测编译通过（app 消费方按中间态策略可中断）

### T3 `VisualizerProxy` + sizeHint + 副标题状态 API（决策 15/19/26/32，终审 🟡4/🟡8）

- **文件**：`src/visualizer/visualizers/VisualizerProxy.h/.cpp`、`src/visualizer/visualizers/SignalVisualizer.h` + 各子类
- `VisualizerProxy` 加 `setMonitorId(QString)/monitorId()`（`addVisualizer` 内 `setMonitorId(key)` 调用）
- `SignalVisualizer` 加 **`virtual void setSubtitleState(const QString& state)`**，各子类转发 `subtitle_label_->setProperty("state", state)` + `unpolish/polish`（终审 🔵14 repolish 并入此实现）；T4/T8/T9 建卡时按绑定状态调用（"normal"/"warning"/"deleted"）
- 各 visualizer 子类**重载 `QSize sizeHint() const`**（waveform 320×200、meter/frame 依内容；led/gauge 默认需 ≥ `VisualizerProxy::doResize` 最小值 minW 200/minH 120，避免首 resize 跳变，终审 🟡8）
- **验证**：etest_visualizer 单测编译通过；`sizeHint()`/`setSubtitleState` 生效

### T4 拖放系统（决策 10/12/18）

- **文件**：`src/visualizer/VisualizationArea.h/.cpp`
- 视图层 `setAcceptDrops(true)` + override `dragEnterEvent/dragMoveEvent/dragLeaveEvent/dropEvent`（`mapToScene`）
- mimeType = `application/x-etest-visualizer`，payload = `displayMode` 字符串
- 半透明预览占位（虚线框 + 类型名）随鼠标；drop 时创建卡片（UUID、`widget->sizeHint()` 尺寸、未绑定警示副标题、落在 drop 位置）
- **验证**：编译；手拖 mime 解析正确

### T5 主题警示色（决策 14/26/31，终审 🔴2）

- **文件**：`src/app/resources/themes/*.json`（13 个）、`src/app/resources/scripts/gen_themes.py`、各主题 QSS 副标题块
- **不做 palette 实现**（决策 26 已废弃）：`ThemePalette`/`ThemeManager` **不加** `warningColor()`（终审 🔴2——运行期警示色完全由 QSS `[state=]` 控制，palette 无消费方）
- 13 个主题 JSON 配 `warning` 键（红主题黄、黄主题红等），**唯一消费方是 `gen_themes.py`**（运行期不读）
- `gen_themes.py` 输出 `warning` 键 + 各主题 QSS 副标题块注入 `[state="warning"]`/`[state="deleted"]` 属性选择器
- 现有 13 主题 QSS 副标题块补 `[state=]` 规则；LED 拼写统一（`#LEDSubtitle` → `LedSubtitle`）
- **验证**：运行 app 切主题，未绑定副标题警示色随主题变

### T6 调色板 dock 组件（决策 1/9/18）

- **文件**：新增 `src/app/widgets/VisualizerPaletteWidget.h/.cpp`（或放 RunConfigEditor 内）；`src/app/CMakeLists.txt`
- `QListView`（ListMode/IconMode 可切）+ `QStandardItemModel`，item 存 `displayMode`（UserRole）+ 缩略图
- 拖放源：`startDrag` 模板仿拓扑 `DeviceListWidget`，mime 带 displayMode
- 缩略图：`createPreviewVisualizer` 渲染成 `QPixmap`（迁自 MonitorConfigDialog，决策 32）
- **验证**：编译；列表/网格切换、拖出 mime 正确

### T7 属性面板 dock 组件（决策 3/7/8/13 动态属性版/25）

- **文件**：新增 `src/app/widgets/MonitorPropertyWidget.h/.cpp`；`src/app/CMakeLists.txt`
- `QScrollArea` 包裹表单：名称 `QLineEdit`、绑定连线（搜索框 + 连接列表，全部列、已绑定视觉禁用+操作不可选）、类型切换（5 类型选择 → 单卡重建）、删除按钮
- `setMonitor(const RunConfig::Monitor&, 连接列表, 已绑定集合)` 加载；信号：`nameChanged/connectionBound/typeChanged/deleteRequested`
- **验证**：编译；选中卡片加载、修改发信号

### T8 `RunConfigEditor` 集成（决策 1/5/6/10/11/16/25/29/32）

- **文件**：`src/app/editors/RunConfigEditor.h/.cpp`
- 加调色板 dock + 属性面板 dock（左：测试程序+调色板；右：属性；中央可视化区），DockTitleBar + toggle action 仿测试程序 dock
- 拖放集成：可视化区 drop → `addMonitor`（UUID、displayMode、sizeHint 尺寸、未绑定）→ 快照 + 建卡
- 选中卡片 → id 反查 → 属性面板加载；取消选中 → 清空
- `removeMonitorById(id)` 收敛右键 + 属性按钮删除（saveSnapshot + removeAt + markModified）
- 类型切换：属性面板 typeChanged → 单卡重建（撤旧建新，保留 id/连接/几何/名称）
- `collectLayout`/`refreshUi` 改按 Monitor 几何读写（id key）；`refreshUi` 全建卡（含未绑定警示副标题）
- 删 `channel_dialog_`/`onAddMonitorClicked`/`add_monitor_action_`
- **验证**：编译；手动——拖放创建、选中加载、改名/绑线/切类型/删除、保存 `.erun`

### T9 运行态适配（决策 6/23/24/28/32，终审 🟡5/🟡6/🟡7）

- **文件**：`src/app/ExecutionPanelController.h/.cpp`、`src/app/MainWindow.cpp`
- `loadProjectMonitors`：**过滤空 connectionId** 不给 MonitorManager
- `rebuildVisualizers`：按 `run_config_.monitors` **全建卡**（含未绑定/连接已删除，key=id，未绑定不订阅，警示副标题区分「未绑定到连线」/「连接已删除」）；**未绑定/失效卡跳过 `addTrace`**（终审 🟡6，避免空 connectionId 入 trace 表）
- 维护 id→connectionId 映射：`visualizerRemoved(id)` 反查取消订阅
- `syncProjectTopologies` 重订阅循环改 id 反查，**删"移除失效卡"分支**
- 清理 `channel_dialog_`/`showChannelSelectionDialog`/`refreshMonitorTree` + `MainWindow.cpp` ribbon「通道选择」入口（`panel_select->addSmallAction(selectChannelsAction())`）+ `selectChannelsAction()` 访问器及 `act_select_channels_->setEnabled` 门控（终审 🟡5）
- **文件不含** `ExecutionDashboard.cpp`（T2 方法签名变更无调用点波及，终审 🟡7 确认无需改）
- **验证**：全量编译；手动——运行态按 `.erun` 全建卡（未绑定警示、绑定订阅、失效卡也建卡警示）

### T10 清理（决策 17/32，终审 🔵15）

- **文件**：删除 `src/app/dialogs/MonitorConfigDialog.h/.cpp`（`createPreviewVisualizer` 已迁 T6）；`src/app/CMakeLists.txt` 移除注册；各主题 QSS 清理 `#MonitorConfigSearch/#MonitorConfigList/#MonitorConfigRight/#MonitorConfigDelete/#MonitorTypeTile` 死选择器（13 主题）
- `ExecutionPanelController::buildConnectionList` 等死代码清理（若 RunConfigEditor 需要连接列表则提取复用）
- **验证**：全仓 grep 无 `MonitorConfigDialog`/`#MonitorConfig*`/`#MonitorTypeTile` 残留；编译通过

### T11 全量编译 + 手动验证

- `scripts/build_ninja.bat -t debug` 全量
- **`temp/projects/demo_mock/run/*.erun` fixture 用编辑器重存为新格式**（旧 layout 数组被新 fromJson 忽略，布局回落默认位置，终审 🔵10）
- **未绑定软提醒承接**（决策 32/终审 🔵11）：T8 实现"保存时有未绑定卡 → 非阻塞软提醒（统计未绑定数可继续保存）"
- 手动清单：编辑器拖放创建/选中/属性编辑/删除/保存；重开还原（含未绑定卡）；运行态全建卡（未绑定/连接已删除警示）；切主题警示色变化；关闭编辑器不崩（析构时序回归）
