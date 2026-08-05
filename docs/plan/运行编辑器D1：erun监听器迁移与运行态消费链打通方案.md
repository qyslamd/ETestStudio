# 运行编辑器 D1：`.erun` 监听器迁移与运行态消费链打通方案

> 版本：v4（用户范围调整版）。新增 D1-g：运行态 `MonitorConfigDialog` 入口/UI 壳保留但失效（交互槽注释、`syncProjectMonitorsToFile` 注释），监听器配置收敛到运行编辑器（后续 QDockWidget 呈现）。实施计划见第八节。

## 一、问题陈述

运行编辑器（阶段 B 已完成）能编辑 `.erun` 运行配置（programs/monitors/layout/runParams），但**运行态完全不消费 `.erun`**：`ExecutionPanelController` / `ExecutionDashboard` 中无任何 `.erun` 或 `RunConfig` 引用。当前监听器存储是**双源且互不相通**：

- 运行编辑器：监听器写 `.erun` 的 `monitors`
- 运行态：`loadProjectMonitors()`（`ExecutionPanelController.cpp:1376`）从 `.etproj` 的 `monitors` 读；`syncProjectMonitorsToFile()`（:1333）写回 `.etproj`

结果：运行编辑器里配好的监听器/布局/测试程序，运行态打开项目完全看不到，仍走 `.etproj` 老链。运行编辑器沦为孤岛，`.erun` 未成为运行配置真源。

**同时**，运行态程序源是 ribbon 的 `ProgramSelectionPopup`（`run()` 里 `popup_->selectedPaths()`），与 `.erun` 的 `programs` 无关。

**本方案目标**：打通 `.erun` → 运行态消费链，让 `.erun` 成为运行配置的唯一真源；监听器从 `.etproj` 迁到 `.erun`；运行态程序源切换到 `.erun` 的 `programs`，删除 `ProgramSelectionPopup`；运行态可视化区忠实还原 `.erun.layout`。

## 二、现状梳理（代码事实）

### 2.1 监听器加载链

```
MainWindow::onProjectOpened (:1738)          切执行页 currentChanged (:1188)
        └─ ExecutionPanelController::syncProjectTopologies() (:412)
              ├─ 树空 → engine_->loadTopology + loadProjectMonitors() (:471)
              └─ mtime 变 → clearMonitorState + loadTopology + loadProjectMonitors() (:493)
                        → 重订阅活跃可视化 (:497-520)
```

- `loadProjectMonitors()`（:1376）：`monitor_manager_->loadMonitors(project->monitors(), engine_->topologyDoc())`，数据源 = `.etproj` 的 `monitors` 数组（`ProjectInfo::monitors()`）。
- `MonitorManager::loadMonitors(QJsonArray, topologyDoc)`：按数组 + 拓扑对比连接有效性，重建监听器结构（含 invalid 标记）。

### 2.2 监听器写回链

- `syncProjectMonitorsToFile()`（:1333）：遍历 `monitor_manager_->monitorTree()` 拼 `{connectionId, name, displayMode}` 数组 → `ProjectManager::setMonitors(arr)` → 写 `.etproj`。
- 调用点 4 处，全部来自运行态对话框 `MonitorConfigDialog` 交互（controller 处理槽）：
  - `onVisualizerChosen`：改 displayMode（:1096）/ 新建（:1121）
  - `onRenameRequested`（:1164）
  - `onDeleteRequested`（:1179）

### 2.3 运行态程序源

- `run()`（:523）：`popup_->selectedPaths()` 取选中程序 → 单程序 / 合并执行。popup 返回**绝对路径**（`ProgramSelectionPopup::scanPrograms()` 经 `ProjectInfo::scanDirectory` 返回 `fi.absoluteFilePath()`）。
- `runAll()`（:769）：`popup_->allPaths()`（扫描 `cases/*.etprog`）→ 合并全部。
- `verify()`（:722-742）：`has_program` 校验走 popup 选中/全集。
- 门控：`checkCanRunAll()`（:368）用 `popup_->hasAnyProgram()`；`checkCanRun()`（:383）用 `popup_->selectedPaths().isEmpty()`；二者被 `syncControlStates()` 与 `updateRunControls()`（:394-395）调用。
- `ProgramSelectionPopup` 引用面：`ExecutionPanelController.cpp`（:41/113-114/282-284/296/310/323/336/346/368/383/526/531/724-727/772/784-785）、`MainWindow.cpp`（:99/1761/2410）、`CMakeLists.txt`（:86-87）。

### 2.4 `.etproj` monitors 存储

- `ProjectInfo.h/.cpp`：`monitors()`/`setMonitors()`/`monitors_`，`toJson` 写 `"monitors"` 键（:46）、`fromJson` 读（:66-68）。
- `ProjectManager::setMonitors`（`ProjectManager.cpp:253`）。`ProjectManager` **无 settings 写入 API**（仅 `ProjectInfo::setSettings` 整表替换）。
- 测试：`tests/core/project_manager_test.cpp` 有 monitors 相关 `TEST_F`（:74/:100）。

### 2.5 可视化区两态与布局

- `VisualizationArea::edit_mode_` 耦合两件事：**布局模式**（手动/自动网格）与**交互开关**（传给 `VisualizerProxy::setEditMode`）。
  - `addVisualizer`（:76-78）/`removeVisualizer`（:97-99）/`setEditMode(false)`（:126-128）/`resizeEvent`（:302-304）：`!edit_mode_` 时触发 `relayout()` 自动网格。
  - `setVisualizerGeometry`（:145-155）：按 scene 坐标设卡片位置/大小（手动模式专用）。
- `VisualizerProxy::setEditMode(bool)`（`VisualizerProxy.cpp:28-34`）：只控制 `ItemIsMovable`/`ItemIsSelectable`；编辑态可拖拽/resize，展示态只读。
- 现状：`RunConfigEditor` 显式 `setEditMode(true)`（`RunConfigEditor.cpp:428`）；`ExecutionDashboard` 的 vis_area 保持默认展示态（自动网格 + 只读）。

### 2.6 项目目录与 settings

- 项目根结构：`backup/ cases/ protocol/ topology/ config/ scripts/ reports/`（见 `temp/projects/*`）。
- `ProjectInfo::settings()`（QVariantMap）序列化到 `.etproj` 的 `"settings"` 键（`ProjectInfo.cpp:45/62-63`）——可承载「当前运行配置」引用。

## 三、决策记录（已确认）

| # | 决策 | 说明 |
|---|---|---|
| D1-a | 监听器从 `.etproj` 迁到 `.erun`，老 `.etproj` 的 `monitors` **一刀切，不兼容读取** | `ProjectInfo::monitors` 彻底删除，含测试同步改 |
| D1-b | `.erun` 存 `.etproj` 同级 `run/` 目录，**可命名多个** `.erun` | `ProjectStructureWidget` 增 run 分类 + 「当前运行配置」选配机制 |
| D1-c | `MonitorConfigDialog` 运行态**保留**，后续再删 | 本期只迁移其写回目标（`.etproj` → `.erun`） |
| D1-d | 运行态程序源**只用 `.erun` 的 `programs`，删除 `ProgramSelectionPopup`** | `run()`/`runAll()`/`verify()`/门控全部改读 `.erun` |
| D1-e | 运行态可视化区**忠实还原 `.erun.layout`**：手动布局 + 只读交互 | `VisualizationArea` 布局/交互解耦，运行态新建「手动 + 只读」模式 |
| D1-f | 运行态重载 `.erun` 用 **mtime 检测**（仿 topology） | 切执行页时 `.erun` 内容变化才级联刷新 |
| D1-g | 运行态 `MonitorConfigDialog` 入口/UI 壳**保留但失效**：5 个交互槽注释、`syncProjectMonitorsToFile` 注释 | 监听器配置收敛到运行编辑器（运行态纯只读）；对话框功能后续整体迁移到运行编辑器（QDockWidget 呈现），本阶段为过渡 |

## 四、方案设计

### 4.1 `RunConfig` 模型抽取为独立模块

`struct RunConfig` 现定义在 `RunConfigEditor.h`（含 `toJson/fromJson`，`RunConfigEditor.cpp:49-138`）。运行态 `ExecutionPanelController` 需要复用，**抽取到独立文件**（纯数据模型，无 UI 依赖，符合原阶段 B 文档 3.5「三个消费方」的运行配置与 UI 解耦定位）：

- 新增 `src/app/editors/RunConfig.h/.cpp`：`struct RunConfig`（programs/monitors/layout/runParams + `toJson/fromJson`）+ 静态文件助手 `bool RunConfig::loadFromFile(const QString&, RunConfig*)` / `bool RunConfig::saveToFile(const QString&, const RunConfig&)`（从 `RunConfigEditor::loadFromFile/saveToFile` 上移，统一 JSON 解析/写出）。
- `RunConfigEditor.h/.cpp`：删除本地 `struct RunConfig` 定义，改 `#include "RunConfig.h"`；`loadFromFile/saveToFile` 改为调用抽取后的助手（保留编辑器自身的文件路径/快照逻辑）。

### 4.2 `.erun` 目录与「当前运行配置」选配机制

- **目录**：`<项目根>/run/`（`.etproj` 同级）。
- **多文件**：`run/` 下可命名多个 `.erun`（如 `run/综合仿真.erun`）。
- **「当前运行配置」引用**：`.etproj` `settings` 新增键 `runConfigFile`，值为相对项目根的 `.erun` 路径（如 `"run/综合仿真.erun"`）。这是运行态选配的唯一真源。
- **设置时机**：
  1. 运行编辑器保存 `.erun` 到当前项目 `run/` 下时，自动写 `settings.runConfigFile`（绝对路径前缀匹配当前项目根则生效）；
  2. 项目树 `run/` 分类右键菜单「设为当前运行配置」（第 4.3 节）。
- **settings 写入 API**：`ProjectManager` 新增 `bool setSetting(const QString& key, const QVariant& value)`（读 `currentProject()->settings()` → 改单键 → `setSettings` → `saveToFile`），供运行编辑器保存、项目树右键、运行态首次建文件共用。

**运行态重载（mtime 检测，D1-f）**：`ExecutionPanelController` 新增 `loadCurrentRunConfig()`，在 `syncProjectTopologies()` 开头调用（先于 `loadProjectMonitors` 监听器重建就绪）：
- 解析当前运行配置：读 `settings.runConfigFile` → 加载对应 `.erun`；缺失/空 → 取 `run/` 下第一个 `.erun`；均无 → 空 `RunConfig`（默认构造），不阻塞项目打开。
- **首载不级联**：首次调用（快照未建立）仅解析并填充 `run_config_`、建立路径+mtime 快照，**不触发级联刷新**——项目打开时的初始加载/重建走 `syncProjectTopologies` 树空分支（4.5），避免在 `loadTopology`（:464）之前提前执行 `loadProjectMonitors`（其依赖 `engine_->topologyDoc()`，:1388）。
- **mtime 快照**：成员 `run_config_file_`（当前 .erun 绝对路径）+ `run_config_mtime_`（QDateTime）。对**已建立的快照**比较：路径与 mtime 均未变 → 跳过；文件内容变化（mtime 变，与 `topology.etopo` 的 `topo_mtime_` 机制 :477 对齐）→ 重载 `run_config_` 并**级联刷新**：`loadProjectMonitors()` + 重建可视化区（4.5）+ `syncControlStates()`（含 `updateRunControls`）。级联刷新是「运行期 `.erun` 被编辑器改动」的刷新路径。
- **运行中不打扰**：切执行页触发 `syncProjectTopologies` 时 MainWindow:1185-1189 已限定引擎 Idle 才调用；级联刷新内部同样尊重 `engine_->state() == Idle`。

### 4.3 项目树 run 分类

`ProjectStructureWidget::defaultCategories()`（`ProjectStructureWidget.cpp:495`）追加：

```cpp
{QStringLiteral("run"), QStringLiteral("运行"),
 QStringLiteral("run/"), QStringLiteral("run"),
 QStringLiteral("erun"), QStringLiteral("新建运行配置")},
```

- 双击 `.erun` → 走 `fileOpenRequested` → `EditorManager` 打开运行编辑器（复用现有 `.erun` 注册，`EditorManager.cpp:130`）。
- 右键菜单：现有「新建/删除/重命名」随 `newFileExt="erun"` 自动获得；追加「设为当前运行配置」（调 `ProjectManager::setSetting("runConfigFile", 相对路径)`）。
- **图标**：复用已有 `run_dark.svg`/`run_light.svg`（运行按钮播放三角图标，暗/亮两套变体齐全，qrc 已注册），无需新建；不复用 `file_generic`。

### 4.4 监听器迁移

`ExecutionPanelController` 新增成员 `RunConfig run_config_;`、`QString run_config_file_;`、`QDateTime run_config_mtime_;`。

- **`loadProjectMonitors()`（:1376）改**：数据源从 `project->monitors()` 换成 `run_config_.monitors`：
  ```cpp
  monitor_manager_->loadMonitors(run_config_.monitors 转 QJsonArray, engine_->topologyDoc());
  ```
  加载时机不变（`syncProjectTopologies` 树空/拓扑变化时），但前置 `loadCurrentRunConfig()`（4.2，含 mtime 检测）。这是运行态**展示**监听器的唯一入口（只读）。
- **运行态对话框交互槽注释（D1-g）**：`onChannelSelected`/`onVisualizerChosen`/`onCheckToggled`/`onRenameRequested`/`onDeleteRequested` 五个槽**注释掉**（配置逻辑暂禁用，留作后续迁移参考）；`showChannelSelectionDialog`/`channel_dialog_` 创建与信号连接保留（入口 + UI 壳，打开后交互失效）。
- **`syncProjectMonitorsToFile()`（:1333）注释**：槽注释后无调用者，运行态不再写监听器（监听器写入方 = 运行编辑器，落 `.erun`）。**取消**「首次写回兜底建 `run/default.erun`」——`.erun` 由运行编辑器创建，运行态无 `.erun` 时为空配置（4.2）。
- 失效监听器（invalid）由 `MonitorManager::loadMonitors` 按拓扑判定（与现状一致），在对话框「失效监听器」分组可见（UI 壳仍能显示）。

### 4.5 运行态消费链打通

**可视化区「手动 + 只读」模式（D1-e）**：`VisualizationArea` 把 `edit_mode_` 耦合的**布局模式**（手动/自动网格）与**交互开关**（proxy 可编辑/只读）解耦，例如拆为 `setManualLayout(bool)` + `setInteractive(bool)`（或保留 `setEditMode` 布局语义 + 单独交互开关），`relayout()` 仅由布局模式驱动，`VisualizerProxy::setEditMode` 仅由交互开关驱动。各消费方：

| 场景 | 布局模式 | 交互 |
|---|---|---|
| 运行编辑器（RunConfigEditor） | 手动 | 可编辑 |
| 运行态（ExecutionDashboard） | **手动** | **只读** |
| 现状展示态语义 | 自动网格 | 只读 |

运行态 `ExecutionDashboard` 的 vis_area 设为「手动 + 只读」，按 `.erun.layout` 应用几何（`setVisualizerGeometry`）。

**项目打开重建可视化区**：
- `loadCurrentRunConfig()` 后，按 `run_config_.monitors` 遍历 `createAndShowVisualizer()`（复用「创建即所见」，依赖 `monitor_manager_`/`dashboard_`/`engine_->topologyDoc()`，在项目打开时序下均已就绪）建卡片，**跳过 invalid 监听器**（与 `syncProjectTopologies` 失效移除逻辑 :501-514 对齐，失效监听器只在对话框「失效监听器」分组可见可删），再按 `run_config_.layout` 设几何。
- **放置点**：在 `syncProjectTopologies` 的树空分支（:462-474，:473 早返回）与 mtime 分支（:481-494，无早返回）内、`loadProjectMonitors()` 之后都插入（不能只放末尾，树空分支会提前 return）。
- mtime 变化级联刷新时同样重建（4.2）。

**运行期布局兜底**：运行态手动模式下 `addVisualizer` 不触发 `relayout`，重建时对无 `.erun.layout` 项的监听器按递增默认位置摆放（仿 `RunConfigEditor::refreshUi` :491-499，防御异常 `.erun`）。正常路径下 `.erun.layout` 覆盖全部监听器（编辑器 `collectLayout` 保存时收集全部卡片几何）。

**写回一致性**：监听器写入方**只有运行编辑器**（落 `.erun`）；运行态只读展示。`.etproj` 只保留 `runConfigFile` 引用，不再承载任何运行配置内容。

### 4.6 运行程序源切换（删 popup）

`.erun.programs` 存**相对项目根路径**，消费端（`loadTestProgram`、`checkUnsavedAndPrompt` 的 editor 绝对路径比对、`mergePrograms`）需要**绝对路径**。新增统一转换助手：

```cpp
// 相对项目根路径 → 绝对路径（.erun.programs 消费前必经）
QStringList ExecutionPanelController::resolveRunPrograms() const {
  QStringList out;
  const QString root = etest::core::project::ProjectManager::instance()
                           .currentProjectRoot();
  if (root.isEmpty()) {
    return out;  // 项目未打开，防御性提前返回（QDir("") 会相对 cwd 拼路径）
  }
  for (const QString& p : run_config_.programs) {
    out.append(QDir(root).absoluteFilePath(p));
  }
  return out;
}
```

- **`run()`（:523）**：`paths = resolveRunPrograms()`；`programs` 空 → 提示「请先在运行编辑器配置测试程序」并 return。
- **`runAll()`（:769）**：语义保留「项目全部程序」——不再走 popup，直接扫描 `cases/*.etprog`（`ProgramSelectionPopup::allPaths()` 的扫描逻辑上移到 controller 或复用 `TestProgramManagerWidget` 已有扫描），与 `.erun` 选配正交。
- **`verify()`（:722-742）**：`has_program` 校验改为遍历 `resolveRunPrograms()` 检查有可用用例。
- **门控**：`checkCanRunAll()`（:368）与 `checkCanRun()`（:383）均改为**纯内存**判断 `!run_config_.programs.isEmpty()`——不做文件加载（「有可用用例」检查在每次 `syncControlStates`/`updateRunControls` 触发时会全量 `loadTestProgram`，属 I/O 代价），可用性校验留给 `verify()` 与 `run()` 内加载后空用例提示（:592-597）。
- **`updateRunControls` 触发源**：popup 的 `selectionChanged`（:114-115）删除后，运行配置变化由 4.2 的 mtime 级联刷新（调 `syncControlStates()`）与项目打开 `syncProjectTopologies` 补位。
- **`ProgramSelectionPopup` 删除**：`ExecutionPanelController`（include/:113-114/282-284/296/310/323/336/346/368/383/526/531/724-727/772/784-785 及 `popup_` 成员/`programPopup()`）、`MainWindow`（:99/1761/2410）、`CMakeLists.txt`（:86-87）、删除 `widgets/ProgramSelectionPopup.h/.cpp`。`TestProgramManagerWidget` 不受影响（独立于 popup）。
- **ribbon 布局**：`MainWindow:2410` 的 `panel_select->addSmallWidget(programPopup())` 移除；`MainWindow:1761` 的「发现 N 个测试程序」提示改为读 controller 新增公开访问器 `int runProgramCount() const`（返回 `run_config_.programs.size()`），文案调整为「当前运行配置含 N 个测试程序」。

### 4.7 `.etproj` monitors 清理

- `ProjectInfo.h/.cpp`：删 `monitors()`/`setMonitors()`/`monitors_` 及 `toJson` 写 `"monitors"`、`fromJson` 读 `"monitors"`。
- `ProjectManager.h/.cpp`：删 `setMonitors()`；**新增 `setSetting(key, value)`**（4.2）。
- `tests/core/project_manager_test.cpp`：删 monitors 相关 `TEST_F`（:74/:100，一刀切）。
- 确认无其他 `project->monitors()` / `setMonitors` 引用后删除。

### 4.8 `runParams` 键定义

本期**保持空对象 `{}`**（`fromJson` 已透传）。候选键（循环次数/超时/执行方式）待运行参数功能需求明确，YAGNI，不预定义。

### 4.9 `MonitorConfigDialog` 迁移规划（后续阶段，本阶段不实施）

监听器配置最终收敛到运行编辑器，`MonitorConfigDialog` 退役：

1. `RunConfigEditor` 以 **QDockWidget 呈现**监听器配置面板（仿「测试程序」dock：toggle action + DockTitleBar + 事件过滤），内容复用对话框的左右栏交互（连接列表 + 类型瓦片 + 改名/删除）。
2. 补齐运行编辑器交互：`renameRequested`/`deleteRequested`/`checkToggled`（本阶段运行编辑器只接 `visualizerChosen`）。
3. 运行态删除：`channel_dialog_`、`showChannelSelectionDialog`、五个槽、ribbon「通道选择」入口。
4. 监听器配置后的可视化区联动：运行编辑器内直接作用于自身 vis_area；运行态经 mtime 级联刷新（4.2）看到结果。

## 五、影响范围

| 文件 | 改动 |
|---|---|
| `src/app/editors/RunConfig.h/.cpp` | **新增**：`RunConfig` 模型 + `loadFromFile`/`saveToFile` 助手 |
| `src/app/editors/RunConfigEditor.h/.cpp` | `RunConfig` 移出；`loadFromFile/saveToFile` 复用助手；保存到项目 `run/` 时写 `settings.runConfigFile` |
| `src/app/ExecutionPanelController.h/.cpp` | 删 popup 及 `programPopup()`；新增 `run_config_`/`run_config_file_`/`run_config_mtime_`/`loadCurrentRunConfig()`/`resolveRunPrograms()`/`runProgramCount()`；`loadProjectMonitors`/`syncProjectMonitorsToFile` 改 `.erun`（含首次建 `run/default.erun`）；`run/runAll/verify`/`checkCanRun`/`checkCanRunAll` 改 `.erun.programs`；项目打开重建可视化区（跳过 invalid）；mtime 级联刷新 |
| `src/app/VisualizationArea.h/.cpp` | 布局模式与交互开关解耦（`setManualLayout`/`setInteractive`），运行态「手动 + 只读」 |
| `src/app/ExecutionDashboard.h/.cpp` | vis_area 设为「手动 + 只读」（`setManualLayout(true)` + `setInteractive(false)`），运行期新增卡片兜底布局 |
| `src/app/MainWindow.cpp` | 删 popup include/:1761/:2410 |
| `src/app/ProjectStructureWidget.cpp` | `defaultCategories` 加 run 分类；右键「设为当前运行配置」 |
| `src/app/resources/icons/svg/run_dark.svg` `run_light.svg` | 复用已有（运行按钮图标，非新增） |
| `src/core/project/ProjectInfo.h/.cpp` | 删 monitors 三件套 |
| `src/core/project/ProjectManager.h/.cpp` | 删 `setMonitors`；**新增 `setSetting`** |
| `tests/core/project_manager_test.cpp` | 删 monitors 相关测试 |
| `src/app/CMakeLists.txt` | 加 `RunConfig`；删 `ProgramSelectionPopup` |
| `src/app/widgets/ProgramSelectionPopup.h/.cpp` | **删除** |

## 六、开放问题

1. **runAll 语义**（已决）：保留「扫描 cases/ 全部程序」，扫描逻辑从 popup 上移到 controller。
2. **run 分类图标**（已决）：复用已有 `run_dark/run_light` 两套变体（运行按钮图标），不新建。
3. **项目打开重建时序 / invalid**（已决）：`MonitorManager::loadMonitors` invalid 机制兜底，重建跳过 invalid 监听器（对话框可见可删）。
4. **运行编辑器保存写 settings 的边界**：仅当保存路径落在当前打开项目 `run/` 下才写 `runConfigFile`。脱离项目打开 `.erun`（findProjectRoot 为空）时不写，属现状可接受。
5. **mtime 秒级粒度**：与 `topology.etopo` 的 `topo_mtime_` 机制一致，同秒内连续保存可能漏检一次；可接受（拓扑同策略，编辑侧有主动 `syncProjectTopologies` 触发路径）。

## 七、阶段规划更新

阶段 B 剩余项（原阶段 B 文档 3.4/七）：

- **D1 监听器迁移与运行态消费链打通**（本方案，进行中）
- **runParams 键定义**（4.8：本期 `{}`，随 D1 落地）
- **MonitorConfigDialog 运行态交互**（D1-g：入口/UI 壳保留但失效，交互槽注释；完整迁移见下）

**后续迁移阶段（本阶段不实施）**：`MonitorConfigDialog` 功能整体迁入运行编辑器，以 QDockWidget 呈现监听器配置面板（4.9）；随后删除运行态 `channel_dialog_`/`showChannelSelectionDialog`/五个槽/ribbon「通道选择」入口，运行态回归纯只读。

D1 完成后，运行编辑器与运行态共享同一 `.erun`，阶段 C（可视化区下沉共享层）与 D（独立运行程序）的前置依赖即满足：独立程序读 `.erun` 直接跑，编辑/运行职责分离达成。

## 八、实施计划

> 按依赖顺序分批。每任务独立可编译验证（增量构建）。GUI 无法自动运行，手动验证清单列于各任务。构建命令：`scripts/build_ninja.bat -t debug -m ETestStudio`。

**依赖图**：`T1/T2/T3`（底层，可并行）→ `T4/T5`（依赖 T1/T2）→ `T6`（依赖 T1/T2/T3，核心大任务）→ `T7/T8`（依赖 T6）→ `T9`（全量编译 + 手动验证）。

### T1 `RunConfig` 模型抽取

- **文件**：新增 `src/app/editors/RunConfig.h/.cpp`；改 `RunConfigEditor.h/.cpp`
- 把 `struct RunConfig`（含 `Monitor`/`LayoutItem` 子结构）从 `RunConfigEditor.h` 移入 `RunConfig.h`，`toJson/fromJson`（`RunConfigEditor.cpp:49-138`）移入 `RunConfig.cpp`；新增静态 `bool RunConfig::loadFromFile(const QString&, RunConfig*)` / `bool RunConfig::saveToFile(const QString&, const RunConfig&)`（上移自 `RunConfigEditor::loadFromFile/saveToFile` 的 JSON 解析/写出）
- `RunConfigEditor` 改 `#include "RunConfig.h"`，删本地定义，`loadFromFile/saveToFile` 复用助手
- **验证**：编译通过

### T2 `ProjectInfo`/`ProjectManager` 清理 + `setSetting`

- **文件**：`src/core/project/ProjectInfo.h/.cpp`、`ProjectManager.h/.cpp`、`tests/core/project_manager_test.cpp`
- 删 `ProjectInfo::monitors()`/`setMonitors()`/`monitors_` 及 `toJson` 写 `"monitors"`（:46）/`fromJson` 读（:66-68）；删 `ProjectManager::setMonitors`（:253）
- `ProjectManager` 新增 `bool setSetting(const QString& key, const QVariant& value)`：读 `currentProject()->settings()` 副本 → 改单键 → `setSettings` → `saveToFile`
- 删 `project_manager_test.cpp` 中 monitors 相关 `TEST_F`（:74/:100）
- **验证**：编译 + `ctest` 跑 core 测试通过

### T3 `VisualizationArea` 布局/交互解耦

- **文件**：`src/app/VisualizationArea.h/.cpp`
- `edit_mode_` 拆为两个维度：`setManualLayout(bool)`（驱动 `relayout()` 触发点：addVisualizer :76-78/removeVisualizer :97-99/setEditMode :126-128/resizeEvent :302-304）+ `setInteractive(bool)`（驱动 `VisualizerProxy::setEditMode`，:65/:123）；保留 `setEditMode` 语义或改造为组合调用
- 适配 `RunConfigEditor.cpp:428` 的 `setEditMode(true)` 调用点（= 手动 + 可交互）
- **验证**：编译通过

### T4 `RunConfigEditor` 接入 + 保存写 settings

- **文件**：`src/app/editors/RunConfigEditor.h/.cpp`
- 接入 T1 的 `RunConfig`；`saveToFile` 成功后：若保存路径落在当前项目根 `run/` 下（绝对路径前缀匹配 `ProjectManager::currentProjectRoot()`）→ `ProjectManager::setSetting("runConfigFile", 相对项目根的路径)`
- `findProjectRoot()` 逻辑不变（`.erun` 在 `run/` 下向上找含 topology 目录仍定位到项目根）
- **验证**：编译；手动——打开 `.erun` 保存到项目 `run/`，检查 `.etproj` settings 含 `runConfigFile`

### T5 项目树 run 分类 + 图标

- **文件**：`src/app/ProjectStructureWidget.cpp`（复用已有 `run_dark.svg`/`run_light.svg`，qrc 已注册，无需新增 SVG）
- `defaultCategories()`（:495）追加 `{run, 运行, run/, run, erun, 新建运行配置}`
- 右键菜单追加「设为当前运行配置」→ `ProjectManager::setSetting("runConfigFile", 相对路径)`
- `resource.qrc` 注册 3 个 SVG
- **验证**：编译；手动——项目树显示 run/ 分类，图标正常（暗/亮主题）

### T6 `ExecutionPanelController` 消费链改造（核心）

- **文件**：`src/app/ExecutionPanelController.h/.cpp`
- 新增成员 `RunConfig run_config_;`、`QString run_config_file_;`、`QDateTime run_config_mtime_;`
- 新增 `loadCurrentRunConfig()`（4.2：首载不级联 + mtime 检测 + 级联刷新）、`resolveRunPrograms()`（4.6：相对转绝对 + 空根保护）、`runProgramCount()`
- 删 `popup_`/`programPopup()` 及 include；删 popup 相关引用（:113-114/282-284/296/310/323/336/346/368/383/526/531/724-727/772/784-785）
- `loadProjectMonitors()`（:1376）改读 `run_config_.monitors`；**注释** `syncProjectMonitorsToFile`（:1333）与 5 个对话框槽（`onChannelSelected`/`onVisualizerChosen`/`onCheckToggled`/`onRenameRequested`/`onDeleteRequested`）；`channel_dialog_` 创建/连接/`showChannelSelectionDialog` 保留
- `run()`/`runAll()`/`verify()`/`checkCanRun`/`checkCanRunAll` 改 `.erun.programs`（4.6）
- `syncProjectTopologies`（:412）开头调 `loadCurrentRunConfig()`；树空分支（:462-474）与 mtime 分支（:481-494）内 `loadProjectMonitors()` 后插入可视化区重建（按 `run_config_.monitors` 建卡跳过 invalid + `run_config_.layout` 设几何，无 layout 项递增兜底）；`ExecutionDashboard` vis_area 设「手动 + 只读」（4.5）
- **验证**：编译；手动——项目打开按 `.erun` 建监听器卡片 + 布局还原；切执行页在 `.erun` 被编辑器改动后级联刷新

### T7 `MainWindow` 收尾

- **文件**：`src/app/MainWindow.cpp`
- 删 popup include（:99）、`programPopup()->allPaths().size()`（:1761，改 `runProgramCount()` + 文案）、`addSmallWidget(programPopup())`（:2410）
- **验证**：编译通过

### T8 CMake 注册 + 删除 `ProgramSelectionPopup`

- **文件**：`src/app/CMakeLists.txt`、删除 `src/app/widgets/ProgramSelectionPopup.h/.cpp`
- CMakeLists：加 `editors/RunConfig.h/.cpp`；删 `widgets/ProgramSelectionPopup.h/.cpp`（:86-87）
- **验证**：编译通过（确认无残留引用）

### T9 全量编译 + 手动功能验证

- `scripts/build_ninja.bat -t debug -m ETestStudio` 全量编译
- 手动验证清单（`temp/projects/demo_mock`）：
  1. 打开项目 → 项目树出现 run/ 分类；打开 `.erun` → 运行编辑器配置测试程序（多选）+ 监听器 + 布局 → 保存（`.etproj` settings 写入 `runConfigFile`）
  2. 切执行页 → 按 `.erun` 重建监听器卡片，布局与编辑器一致（手动 + 只读，不可拖拽）
  3. 修改 `.erun`（运行编辑器）→ 再切执行页 → mtime 级联刷新
  4. 运行态 ribbon「通道选择」打开 → UI 壳显示连接/监听器，点类型/改名/删除**无反应**（槽注释）
  5. `run()` → 用 `.erun.programs` 执行；`runAll()` → 扫描 cases/ 全部；`verify()` 门控正常
