# 执行步骤 target 显示改为可读名

## 问题陈述

测试执行界面（`ExecutionDebugWidget`、`ExecutionOutputPanel`）以及 `test-executor`/`test-executor-cli` 工具的步骤树中，每个步骤显示的 target 字段是一串 32 字符的 SHA-1 hex（例如 `9f3a2e7c4b1d8a0f5e6b7c8d9e0f1a2b`），用户无法辨识它对应哪个设备/端口/信号。

根因：`TestStepData.target` 字段在 `.etprog` 测试程序文件中存储 UUID hex（由 `SignalRegistry::computeUuid(deviceId, portName, frameName, nodePath)` 计算），`StepRunner` 在发射 `stepStarted`/`stepFinished` 信号时直接透传该 UUID，UI 层未做任何解析就拼接到 `setText` 中。

### 兼容性说明

`TestStepData.target` 是 `QString`，不强制 UUID 格式。旧 `.etprog` 文件的 target 可能是可读名（如 `温度传感器`），新文件通过 `SignalSelectionDialog` 生成的是 UUID hex。本方案在加载阶段统一填"(待解析)"占位，运行时用 `targetName` 覆盖，兼容新旧文件。若旧 target 本身是可读名（非 UUID），`SignalResolver::resolve` 解析失败后按 D3 兜底，`targetName` 取 `step.target` 原值，最终 UI 仍展示原可读名。

## 目标

- UI 层不再展示裸 UUID hex，改用人类可读的信号描述
- 改动尽量内聚，不扩散到信号签名变更和测试改动
- 为后续下游（日志、报告、etlog 回看）统一可读名格式留出扩展点

## 现状架构

### 数据流

```
.etprog 文件
  └─ TestStepData.target = "9f3a2e7c4b1d8a0f5e6b7c8d9e0f1a2b"  (UUID hex)

StepRunner::executeSingleStep (src/engine/StepRunner.cpp:111)
  ├─ L141: emit stepStarted(caseIndex, stepPath, step.command, step.target)
  │       ↑ 此时 target 是裸 UUID，信号尚未解析
  ├─ L190: signal = resolver_->resolve(step.target)   ← 这里才解析
  │       engine::ResolvedSignal 含 deviceId/deviceType/portName + 编码属性
  │       但缺 deviceName/frameName/nodePath（见 R1）
  ├─ L235: result.target = step.target                 ← 又把 UUID 塞回 result
  └─ L239: emit stepFinished(caseIndex, stepPath, result)
          ↑ result.target 仍是 UUID hex

UI 层消费
  ├─ ExecutionDebugWidget::onStepStarted (L236): displayText = "%1 %2".arg(command).arg(target)
  ├─ ExecutionDebugWidget::onStepFinished (L265): displayText = "%1 %2 (%3ms)".arg(command).arg(result.target)...
  ├─ ExecutionOutputPanel::appendResult: html 中展示 result.target
  └─ ResultCollector::buildStepJson (L255): obj["target"] = step.target 写入 etlog
```

### 关键数据结构

项目中存在**两个同名但不同的 `ResolvedSignal` 结构**：

| 结构 | 位置 | 可读字段 | 编码属性 |
|------|------|---------|---------|
| `etest::core::ResolvedSignal` | `src/core/SignalRegistry.h:17-25` | uuid/deviceId/**deviceName**/deviceType/portName/**frameName**/**nodePath** | 无 |
| `etest::engine::ResolvedSignal` | `src/engine/SignalResolver.h:35-65` | deviceId/deviceType/portName | signalType/coeff/offset/unit/engMin/engMax/channel/frameId/byteOffset/bitOffset/bitWidth/byteOrder/valid |

`StepRunner` 在 `namespace etest::engine` 内，`executeSingleStep` 中 `ResolvedSignal signal;` 实例化的是 **`etest::engine::ResolvedSignal`**。该结构**缺少 `deviceName`、`frameName`、`nodePath` 三个可读字段**。

`SignalResolver::resolve`（`src/engine/SignalResolver.cpp:18-57`）内部调用 `registry_->resolve(uuid)` 拿到 `etest::core::ResolvedSignal`，但只复制了 `deviceId/deviceType/portName` 三个字段（L46-48），丢弃了 `deviceName/frameName/nodePath`。`frameName` 和 `nodePath` 仅作为 `fillFromIcd` 的参数传入（L51），未存入返回值。

### 信号订阅者清单

`stepStarted` / `stepFinished` 信号（`StepRunner.h:109-112`）通过 `TestExecutionEngine` 转发暴露给 UI 层（`TestExecutionEngine.cpp:222-225`）。下表订阅者中，`ResultCollector` 直接订阅 `StepRunner`，其余订阅的是 `TestExecutionEngine` 的转发信号。转发层签名不变，本次改动无需修改转发代码。

| 订阅者 | 文件 | 用途 | 消费 target？ |
|--------|------|------|--------------|
| `ExecutionDebugWidget` | `src/app/ExecutionDebugWidget.cpp:84` | 主程序执行调试面板 | 是，`onStepStarted`/`onStepFinished` 拼接 target 到显示文本 |
| `ExecutionPanelController` | `src/app/ExecutionPanelController.cpp:235` | 转发 stepFinished 给输出面板 | 否，仅转发 |
| `ExecutionOutputPanel` | `src/app/widgets/ExecutionOutputPanel.cpp` | 输出面板 HTML 展示 | 是，`appendResult` 中展示 `result.target` |
| `ResultCollector` | `src/engine/ResultCollector.cpp:28` | etlog 序列化 | 是，`buildStepJson`（L255）写 `obj["target"] = step.target` |
| `MainWindow` (test-executor) | `src/tools/test-executor/main.cpp:553` | CLI 执行器步骤树 | `onStepStarted`（L942）用 `Q_UNUSED(target)` 丢弃；`onStepFinished`（L960-1010）不访问 `result.target`；`addStepTreeItems`（L674）加载阶段填目标列（col 2） |
| `test-executor-cli` | `src/tools/test-executor-cli/main.cpp:560` | CLI 工具 | 需评估消费方式 |

## 方案选项

### 方案 A：UI 层自解析

`ExecutionDebugWidget` 和 test-executor 的 `MainWindow` 各自持有 `SignalRegistry*`，收到 UUID 后调用 `resolve(uuid)` 拿可读字段拼接显示。

| 维度 | 评价 |
|------|------|
| 信号层改动 | 无 |
| StepResult 改动 | 无 |
| UI 改动 | 两个订阅者各加一份解析逻辑 |
| 测试改动 | 无 |
| 关注点分离 | 差：UI 知道"UUID -> 可读名"的解析细节 |
| 后续复用 | 差：日志/报告/etlog 回看各自再解析一遍 |
| 复杂度 | 低 |

### 方案 B：`StepResult` 增加 `targetName` 字段（推荐）

分两步：

**步骤 1**：补全 `etest::engine::ResolvedSignal` 的可读字段（修复 R1）

给 `etest::engine::ResolvedSignal`（`src/engine/SignalResolver.h`）追加 `deviceName`、`frameName`、`nodePath` 三个字段。`SignalResolver::resolve`（`src/engine/SignalResolver.cpp`）在复制 `etest::core::ResolvedSignal` 时一并填充这三个字段。编码属性（coeff/offset/bitWidth 等）保持不变。

```cpp
// SignalResolver.h - struct ResolvedSignal 改动
struct ResolvedSignal {
    // ── 设备身份 ──
    QString deviceId;
    QString deviceName;    // ← 新增
    QString deviceType;
    QString portName;
    QString frameName;     // ← 新增
    QString nodePath;      // ← 新增
    // ... 编码属性不变
};

// SignalResolver.cpp - resolve() 改动
result.deviceId   = resolved->deviceId;
result.deviceName = resolved->deviceName;   // ← 新增
result.deviceType = resolved->deviceType;
result.portName   = resolved->portName;
result.frameName  = resolved->frameName;     // ← 新增
result.nodePath   = resolved->nodePath;      // ← 新增
```

**步骤 2**：`StepResult` 增加 `targetName`，`executeSingleStep` 填充

`StepResult` 增加 `QString targetName`（人类可读）；`executeSingleStep` 在信号解析成功后用 `signal.deviceName + "/" + signal.portName + " · " + signal.nodePath` 拼接填入；control-flow 步骤（DELAY/LOOP/WHILE/IF）和解析失败时按 D2/D3 处理。`stepStarted` 信号签名不动。

| 维度 | 评价 |
|------|------|
| 信号层改动 | `StepResult` 加字段 + `engine::ResolvedSignal` 加字段 + `SignalResolver::resolve` 填值 |
| StepResult 改动 | 加 `QString targetName` |
| UI 改动 | 所有消费 `result.target` 的订阅者改消费 `targetName` |
| 测试改动 | 现有断言不受影响（`Q_DECLARE_METATYPE` 注册的是类型 ID，加字段不影响元类型注册；`QString` 默认构造为空，不影响现有断言） |
| 关注点分离 | 好：可读名在数据层就准备好 |
| 后续复用 | 好：日志/报告/etlog 回看直接用 `result.targetName` |
| 复杂度 | 中 |

### 方案 C：`stepStarted` 信号也改造

把信号解析提到 `stepStarted` 发射之前，让 `stepStarted` 也携带 `targetName`（新增参数或改签名）。UI 完全不接触 UUID。

| 维度 | 评价 |
|------|------|
| 信号层改动 | `stepStarted` 签名变更 |
| StepResult 改动 | 加 `targetName`（同方案 B） |
| UI 改动 | 两个订阅者改信号槽签名 |
| 测试改动 | `step_runner_test.cpp` 等断言信号参数的测试要改 |
| 关注点分离 | 最好：UI 完全不接触 UUID |
| 后续复用 | 好 |
| 复杂度 | 高 |

### 方案对比小结

|  | A | B | C |
|---|---|---|---|
| 信号签名变更 | 无 | 无 | 有 |
| 测试改动 | 无 | 无 | 有 |
| 可读名复用性 | 差 | 好 | 好 |
| stepStarted 阶段可读 | 否 | 否（用 stepPath 末段占位） | 是 |

## 决策记录

### 推荐：方案 B

**理由**：

1. `stepStarted` 发射时机是设计选择，先解析再发射会破坏"开始"语义、增加 cancel 时机窗口。现状保留。
2. UI 在 `onStepStarted` 时本来就显示 PENDING 占位，用 `command + stepPath 末段` 已够用，等 `stepFinished` 拿到 `targetName` 再展示完整可读信息是自然的渐进披露。
3. 改动集中在 `StepResult`（加字段）+ `engine::ResolvedSignal`（加字段）+ `SignalResolver::resolve`（填值）+ `executeSingleStep`（填 targetName）+ UI（消费字段），不扩散到测试和信号签名。
4. 不破坏现有信号签名，所有 `connect(stepRunner, &StepRunner::stepFinished, ...)` 的代码不用动。

### 细节决策

#### D1：`targetName` 格式

推荐 `deviceName/portName · nodePath`，例如 `综合测试台 A/ch0 · 业务数据/燃油阀门1`。

理由：

- `deviceName` 比 `deviceId` 可读（`dev-001` 对用户无意义）
- `portName` 区分同设备多端口
- `nodePath` 是信号的语义路径，比 `frameName` 更贴近用户视角（帧名是 ICD 概念，节点路径是业务概念）
- 不包含 `frameName` 是因为 `nodePath` 已经携带了帧内层级，再加 `frameName` 信息冗余

**依赖**：D1 依赖 R1 修复（`engine::ResolvedSignal` 加 `deviceName/frameName/nodePath` 字段）。修复前 `executeSingleStep` 拿到的 `signal` 对象无法访问这三个字段。

**边界情况**：`deviceName` 可能为空（`registerDevice` 注册时 `deviceName` 参数为空字符串）。此时 targetName 格式退化为 `portName · nodePath`；若 `portName` 也为空，退化为 `nodePath`；若 `nodePath` 也为空（`fillFromIcd` 中 `findNodeByPath` 返回 nullptr 但 `valid=true` 的情况），退化为 `deviceName/portName`（去掉尾部的 ` · `）。

#### D2：control-flow 步骤的 `targetName`

`DELAY`/`LOOP`/`WHILE`/`IF` 这些步骤没有信号 target，当前 `step.target` 本来就是空字符串。`executeSingleStep` 中 control-flow 在 resolve 之前 return（`StepRunner.cpp:154-185`）。

推荐：`targetName` 留空，UI 显示时只展示 `command` 和 `extra`（如 `DELAY 100ms`）。

#### D3：信号解析失败时的 `targetName`

`executeSingleStep` 在 `resolver_->resolve(step.target)` 返回的 `ResolvedSignal.valid` 为 `false` 时（`StepRunner.cpp:193-198`）会走 ERROR 分支提前返回。

推荐：`targetName` 填 UUID 兜底（`targetName = step.target`），并在 UI 上以 ERROR 状态标识。理由是解析失败时用户反而更需要看到原始 UUID 用于排查。

**说明**：`SignalResolver::resolve`（`SignalResolver.h:72`）返回值类型是 `ResolvedSignal`（值类型，非 `std::optional`），解析失败时返回 `result.valid = false`。`executeSingleStep` 中 `StepRunner.cpp:193` 的判断是 `if (!signal.valid)`，走 ERROR 分支提前返回（L194-198）。

#### D4：`stepStarted` 阶段 UI 显示策略

`onStepStarted` 收到的是裸 UUID（信号签名不变），此时还没有 `targetName`。

推荐：

- `command` 非空时：`displayText = command`（不拼 target）
- `command` 与 `target` 均为空时（边界情况）：`displayText = stepPath 末段`（沿用现有 fallback 逻辑，`ExecutionDebugWidget.cpp:246-251` 的条件是 `command.isEmpty() && target.isEmpty()`）

等 `onStepFinished` 拿到 `result.targetName` 后再更新为 `command + targetName + (elapsedMs)`。

#### D5：test-executor 加载阶段处理

`test-executor/main.cpp:674` 的 `addStepTreeItems` 在程序加载时就用 `step.target` 填了树节点的"目标"列。这个时机完全没有运行上下文，没法解析 UUID（SignalRegistry 此时可能还没注册信号）。

推荐：加载阶段统一填 `"(待解析)"` 占位目标列（col 2），运行时 `onStepFinished` 用 `result.targetName` 更新目标列。

```cpp
// 加载阶段 (addStepTreeItems)
// 步骤树为 3 列：步骤(0) / 命令(1) / 目标(2)
item->setText(2, QStringLiteral("(待解析)"));

// 运行时 (onStepFinished)
item->setText(2, result.targetName);
```

**注意**：`onStepStarted`（`main.cpp:942`）用 `Q_UNUSED(target)` 丢弃 target 参数；`onStepFinished`（L960-1010）不访问 `result.target`（只用 `result.status`/`result.elapsedMs`/`result.message`）。两者都不消费 target，改造时需在 `onStepFinished` 槽内追加用 `result.targetName` 更新目标列（col 2）的逻辑。

**兼容性说明**：加载阶段统一填"(待解析)"会临时覆盖旧文件的可读 target（如 `温度传感器`），直到 `onStepFinished` 用 `result.targetName`（经 D3 兜底取回原可读名）恢复。这个临时窗口在加载到运行之间，用户体验略有不一致，但运行后立即恢复正确显示。

## 影响面分析

### 直接影响（本次改动）

| 文件 | 改动 |
|------|------|
| `src/engine/SignalResolver.h` | `engine::ResolvedSignal` 加 `deviceName/frameName/nodePath` 三个字段 |
| `src/engine/SignalResolver.cpp` | `resolve()` 中从 `core::ResolvedSignal` 复制三个新字段 |
| `src/engine/StepRunner.h` | `StepResult` 加 `QString targetName` 字段 |
| `src/engine/StepRunner.cpp` | `executeSingleStep` 在信号解析成功后填 `targetName`；control-flow 和解析失败按 D2/D3 处理 |
| `src/app/ExecutionDebugWidget.cpp` | `onStepStarted` 按 D4 显示；`onStepFinished` 改用 `result.targetName` 拼接 |
| `src/app/widgets/ExecutionOutputPanel.cpp` | `appendResult` 改用 `result.targetName` 展示 |
| `src/engine/ResultCollector.cpp` | `buildStepJson`（L255）追加 `obj["targetName"] = result.targetName` 序列化 |
| `src/tools/test-executor/main.cpp` | `addStepTreeItems` 加载阶段填"(待解析)"；`onStepFinished` 槽内用 `result.targetName` 更新第 2 列 |
| `src/tools/test-executor-cli/main.cpp` | 评估并同步改造 `targetName` 展示 |

### 间接影响（需评估但不一定本次改）

| 消费点 | 文件 | 评估 |
|--------|------|------|
| 日志输出 | `src/engine/StepRunner.cpp:201` 的 `LOG_INFO("ENGINE", "执行步骤 [cmd={} target={}]", ...)` | 同时打印 `targetName`，便于日志排查 |
| 测试报告导出 | 尚未实现（PDF/Excel 导出功能不存在） | 后续实现时需消费 `result.targetName` 而非 UUID |

### etlog 序列化说明

`ResultCollector::buildStepJson`（`src/engine/ResultCollector.cpp:255-310`）当前序列化 `step.target` 到 etlog JSON（L260-262）：

```cpp
if (!step.target.isEmpty()) {
    obj["target"] = step.target;
}
```

本次改动追加 `targetName` 序列化：

```cpp
if (!result.targetName.isEmpty()) {
    obj["targetName"] = result.targetName;
}
```

**旧 etlog 回看兼容**：旧 etlog 文件无 `targetName` 字段，回看时 `targetName` 为空，UI 回退到 `target`（UUID）。新 etlog 文件同时含 `target` 和 `targetName`，UI 优先展示 `targetName`。

### 不受影响

- `SignalRegistry` / `etest::core::ResolvedSignal`：无改动
- `StepRunner` 的信号签名：无改动（方案 B 保留签名）
- `TestExecutionEngine` 信号转发层（`TestExecutionEngine.cpp:222-225`）：转发签名不变，无需修改
- `.etprog` 文件格式 / 测试程序编辑器：`src/test_program/TestProgramData.cpp:107,143` 的 `obj["target"] = step.target` 序列化和 `src/test_program/TestProgramEditorWidget.cpp:576,1105` 的编辑器表格读写操作的是 `TestStepData.target`（UUID 存储），本次改动不改 `TestStepData` 结构和文件格式，保持 UUID 存储
- `tests/engine/step_runner_test.cpp`：现有断言不受影响（只计信号数不查字段，新字段默认空字符串不破坏既有断言）
- `tests/engine/result_collector_test.cpp`：现有断言不受影响（现有用例的 stepResult 不设 targetName 所以不序列化）；建议追加新字段断言
- `CMakeLists.txt`：无新增源文件，不需要改

## 变更文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/engine/SignalResolver.h` | 修改 | `engine::ResolvedSignal` 加 `deviceName/frameName/nodePath` 字段 |
| `src/engine/SignalResolver.cpp` | 修改 | `resolve()` 复制三个新字段 |
| `src/engine/StepRunner.h` | 修改 | `StepResult` 加 `QString targetName` 字段 |
| `src/engine/StepRunner.cpp` | 修改 | `executeSingleStep` 填充 `targetName`；`LOG_INFO`（L201）同时打印 `targetName` |
| `src/app/ExecutionDebugWidget.cpp` | 修改 | `onStepStarted`/`onStepFinished` 消费 `targetName` |
| `src/app/widgets/ExecutionOutputPanel.cpp` | 修改 | `appendResult` 改用 `result.targetName` 展示 |
| `src/app/editors/EtlogViewerWidget.cpp` | 修改 | etlog 回看优先读 `targetName`，旧 etlog 回退到 `target` |
| `src/engine/ResultCollector.cpp` | 修改 | `buildStepJson` 追加 `targetName` 序列化 |
| `src/tools/test-executor/main.cpp` | 修改 | `addStepTreeItems` 加载阶段填"(待解析)"；`onStepFinished` 更新第 2 列 |
| `src/tools/test-executor-cli/main.cpp` | 修改 | 同步改造 `targetName` 展示 |
| `tests/engine/signal_resolver_test.cpp` | 修改 | 追加断言验证 `resolve` 返回的 `deviceName/frameName/nodePath` 三个新字段 |
| `tests/engine/result_collector_test.cpp` | 修改 | 追加用例验证 `targetName` 非空时写入 etlog JSON |

## 已确认决策

下表汇总跨章节的关键决策，编号沿用正文对应章节（R1 对应方案 B 步骤 1，Y3 对应兼容性说明与 D5，Q1/Q2 对应间接影响）。

| 编号 | 问题 | 决策 |
|------|------|------|
| R1 | 如何补全 ResolvedSignal 可读字段 | 给 `engine::ResolvedSignal` 加 `deviceName/frameName/nodePath`，`SignalResolver::resolve` 复制时填充 |
| R2 | 遗漏消费点改造范围 | ResultCollector（etlog 写入）+ ExecutionOutputPanel（展示）+ test-executor-cli 均纳入 |
| Y3 | 旧 .etprog 文件兼容性 | 加载阶段统一填"(待解析)"，运行时用 `targetName` 覆盖 |
| Q1 | 日志输出是否同时打印 targetName | 是，`LOG_INFO` 同时打印 `targetName` |
| Q2 | 测试报告导出是否展示 targetName | 是。但测试报告导出功能（PDF/Excel）尚未实现，后续实现时需消费 `result.targetName` 而非 UUID |

## 参考

- `src/engine/StepRunner.h:39-52` - `TestStepData` 定义（target 注释 "UUID hex"）
- `src/engine/StepRunner.h:66-82` - `StepResult` 定义
- `src/engine/StepRunner.h:150` - `Q_DECLARE_METATYPE(etest::engine::StepResult)`
- `src/engine/StepRunner.cpp:111-241` - `executeSingleStep` 实现
- `src/engine/SignalResolver.h:35-65` - `engine::ResolvedSignal` 定义（缺可读字段）
- `src/engine/SignalResolver.cpp:18-57` - `resolve(uuid)` 实现
- `src/engine/SignalResolver.cpp:46-48` - 仅复制 3 个字段（R1 问题点）
- `src/core/SignalRegistry.h:17-25` - `core::ResolvedSignal` 定义（含可读字段）
- `src/core/SignalRegistry.cpp:122-129` - `resolve(uuid)` 实现
- `src/app/ExecutionDebugWidget.cpp:236-286` - UI 接收侧
- `src/app/widgets/ExecutionOutputPanel.cpp` - 输出面板展示
- `src/app/ExecutionPanelController.cpp:235` - 信号转发
- `src/engine/ResultCollector.cpp:28, 255-310` - etlog 序列化
- `src/tools/test-executor/main.cpp:674-778` - test-executor 步骤树构建
- `src/tools/test-executor-cli/main.cpp:560` - CLI 工具订阅
