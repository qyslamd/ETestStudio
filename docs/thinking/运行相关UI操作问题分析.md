# 运行相关 UI 操作问题分析

> 范围：`ExecutionPanelController`、`TestProgramManagerWidget`、`MainWindow` 中运行/验证/运行全部相关 action 的状态同步与行为。
>
> 状态：分析文档。已产出配套设计方案 `运行功能迁移至执行页设计方案.md`，下表为各问题在方案中的处理状态。

## 背景

ribbon 上的「验证」「运行」「运行全部」三个 action 的 enable 状态由 `ExecutionPanelController` 集中管理。经过多轮增量改动（勾选框、串行队列、预条件驱动 enable），运行相关的 UI 逻辑出现了规则分散、判定重复、enable 与实际行为脱钩等问题。本文梳理现状并给出收敛方向。

## 问题追踪

本分析文档的 7 个问题在 `运行功能迁移至执行页设计方案.md` 中的处理状态：

| # | 问题 | 设计方案覆盖 | 方案章节 |
|---|---|---|---|
| 1 | 运行目标判定重复 5 处 | popup 统一来源，全从 popup 读 | 3.2 运行内容来源、popup 接口 |
| 2 | runAll 静默退化 | 删两个退化分支，一律走队列 | 3.2 运行内容来源 |
| 3 | 两个同步入口职责重叠 | updateRunControls / syncControlStates 职责分离 | 3.2 updateRunControls 定义、4.2 |
| 4 | syncControlStates switch 冗余分支 | ⏸️ 搁置，当前不处理 | — |
| 5 | verify / checkCanVerify 两套前提 | checkCanVerify 改为 popup 全集非空，简化但未共用 | 3.2 popup 接口 |
| 6 | canRun 陈旧门 | runAll 不再依赖 canRun，但 canRun 本身未失效 | 3.2 enable 规则 |
| 7 | loadProjectTopologies 一刀切 | ⏸️ 明确搁置"拓扑加载暂不动" | 3.2 运行内容来源（免责注） |

## 现状梳理

### 涉及的关键函数

| 函数 | 位置 | 职责 |
|---|---|---|
| `checkCanVerify` | `ExecutionPanelController.cpp:310` | 决定「验证」按钮是否亮 |
| `checkCanRun` | `ExecutionPanelController.cpp:351` | 决定「运行」「运行全部」是否亮 |
| `updateRunControls` | `ExecutionPanelController.cpp:379` | 只刷 run/runAll 的 enable |
| `syncControlStates` | `ExecutionPanelController.cpp:261` | 全量刷 5 个 action |
| `run` | `ExecutionPanelController.cpp:413` | 单次运行 |
| `runAll` | `ExecutionPanelController.cpp:620` | 运行全部（含串行队列） |
| `runNextInQueue` | `ExecutionPanelController.cpp:650` | 队列推进 |
| `verify` | `ExecutionPanelController.cpp:509` | 校验并生成问题清单 |
| `loadProjectTopologies` | `ExecutionPanelController.cpp:389` | clear + 全量遍历 .etopo |

### 信号连线（`MainWindow`）

- `programSelectionChanged` / `checkedProgramsChanged` → `updateRunControls`（只刷 run/runAll）
- `onProjectOpened` / `onProjectClosed` / `currentEditorChanged` → `syncControlStates`（全量刷）
- `onProjectOpened` 中先调 `tpMgr->refreshList()` 再 `syncControlStates()`

## 问题清单

### 问题 1：「运行目标」判定重复 5 处，规则互相打架

同一个"该跑谁"的语义，分散在 5 个函数里，且各函数接受的来源不同：

| 函数 | 编辑器打开 | 勾选 | 选中 | 树非空 |
|---|---|---|---|---|
| `checkCanVerify` | ✓ | | | ✓ (`hasAnyProgram`) |
| `checkCanRun` | ✓ | ✓ | ✓ | |
| `run` | ✓ | ✗ | ✓ | |
| `runAll` | ✓(退化) | ✓ | ✗ | |
| `verify` | ✓ | ✓(≥2) | ✓ | |

直接后果--**按钮亮着却点了报错**：

- 勾选 2 个文件但没选中任何一个 → `checkCanRun` 因"有勾选"返回 true，`act_run_` 是亮的；
- 点下去 `run()` 不认勾选，弹"请先打开一个测试程序或从列表中选中一个"。

enable 状态与实际行为脱钩。

### 问题 2：「运行全部」静默退化为「运行」

`runAll()` 在三种情况都退化成 `run()`：

1. 当前编辑器打开了测试程序；
2. 勾选 < 2 个。

日常场景下（开着编辑器或没勾选），"运行全部"实际就是"运行"。两个按钮语义重合，用户无法从行为区分。要么禁用、要么提示，不该静默退化。

### 问题 3：两个状态同步入口职责重叠

- `syncControlStates()` 全量刷 5 个 action；
- `updateRunControls()` 只刷 run/runAll。

后者是为"勾选/选中变化时不要重算 verify"而加的。但"什么时候刷 verify、什么时候不刷"这条规则是靠 `MainWindow` 里把信号分别接到两个函数来实现的--**规则散在 caller 端，不在 controller 内部**。看懂"verify 何时变"得同时读 Controller 和 MainWindow 两边连线。

### 问题 4：`syncControlStates` 的 switch 有冗余分支

`Idle` / `Finished` / `Error` 三个分支对 5 个按钮的处理完全一样，只有 `Running` / `Paused` 不同。三份重复代码，未来加一个 action 得改三处。

### 问题 5：`verify()` 与 `checkCanVerify()` 是两套前提检查

- `verify()` 手写 6 条检查（项目/ICD/拓扑/信号/程序/硬件）生成问题清单；
- `checkCanVerify()` 另写前 4 条决定按钮亮不亮。

同一语义两份实现，改一处忘另一处是迟早的事。`checkCanVerify` 应复用 `verify` 的检查逻辑只取布尔结果。

### 问题 6：`canRun` 这个关键门是陈旧的

`checkCanRun()` 读 `debug_widget_->canRun()`，而 `canRun()` 只在 `verify()` 末尾被 `setDependencies` 更新。

- 用户改了勾选/选中/编辑器 → `updateRunControls` → `checkCanRun` 读到的还是上次 verify 的旧值；
- `updateRunControls` 路径不触发 verify，所以 run/runAll 的 enable 可能基于过时状态。

`canRun` 没有跟着输入变化主动失效。

### 问题 7：`loadProjectTopologies` 时机一刀切

`run` 和 `runNextInQueue` 都调 `loadProjectTopologies()`（clear + 全量遍历 .etopo）。单次运行也全量重读磁盘，而编辑器里改的拓扑还没保存--跑的是磁盘旧版。两种场景共用同一套加载逻辑，单次场景下既浪费又可能跑错版本。

## 根因总结

**"运行目标"五处各写一遍且规则不一，加上两个同步入口职责重叠，是 enable 状态与实际行为脱钩的根因。** 其余问题（退化、冗余分支、canRun 陈旧、全量重读）都是在此基础上衍生出来的局部症状。

## 收敛方向（仅思路，待定）

1. **统一运行目标解析**：抽 `resolveRunTarget()` 返回统一目标（编辑器/选中/勾选列表/无），`checkCanRun`、`run`、`runAll`、`verify` 都基于它，消除 5 份判定。
2. **`runAll` 不静默退化**：目标不是"≥2 勾选"就禁用 `act_run_all_`，让两按钮语义清晰。
3. **状态同步收口到 controller 内部**：单一 `refreshActions()`，`MainWindow` 只转信号，不决定刷哪些。
4. **`verify` 与 `checkCanVerify` 共用前提清单**：一份实现，两个出口。
5. **`canRun` 跟着输入变化主动失效**：选中/勾选/编辑器切换时失效，不靠 verify 推动。
6. **`syncControlStates` 的 switch 合并同构分支**：Idle/Finished/Error 合并。
7. **单次运行按需加载拓扑**：不全量重读，避免跑磁盘旧版。

## 待决策点

- 问题 2 的处理方式：禁用 `act_run_all_` 还是提示？倾向禁用，语义更清晰。
- 问题 6 的 `canRun` 失效策略：是彻底去掉 `canRun` 门（直接用 `checkCanVerify` + 目标存在性），还是保留但主动失效？倾向前者，减少状态层。
- 问题 7 的单次运行拓扑加载：是否需要区分"编辑器打开的拓扑"与"磁盘拓扑"？涉及编辑器未保存的场景，需进一步确认产品语义。
