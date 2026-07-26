# SignalTreePanel 扁平化与 channelIndex 索引清理方案

## 问题陈述

当前 `SignalTreePanel` 使用 `QTreeWidget` 构建两层树型结构（监听器 → 通道），但其层级逻辑已与当前架构脱节：

1. **树型结构冗余**：每个监听器只有 1 个通道，树型展开后只有一个叶子节点，展开/收起操作无实际意义
2. **`channelIndex`（旧称 ci）恒为 0**：旧架构中一个 Monitor-4CH 有 4 个通道可 tap 不同连线，新架构下每个监听器通过 `connectionId` 绑定一条连线，通道概念已不存在
3. **接口层残留旧概念**：`subscribe(monitorIndex, channelIndex, cb)`、`checkStateChanged(monitorIndex, channelIndex, checked)`、`MonitorTreeEntry::channelCount`、`displayMode(monitorIndex, channelIndex)` 等接口中的 `channelIndex` 参数恒为 0
4. **命名不规范**：代码中多处使用缩写 `mi`/`ci` 代替 `monitorIndex`/`channelIndex`，不符合 Google C++ 风格指南"避免非显而易见缩写"的原则

## 现状架构

```
MonitorManager
  └─ tree_cache_: QList<MonitorTreeEntry>
       └─ MonitorTreeEntry { monitorIndex, name, deviceType, channelCount(=1) }

SignalTreePanel (QTreeWidget)
  └─ 监听器名称 (topLevelItem, 不可勾选)
       └─ ch0 (child, checkbox, 存储 key = (monitorIndex << 16) | channelIndex)

checkStateChanged(monitorIndex, channelIndex, checked) → 恒定 channelIndex=0
  → ExecutionPanelController
    → MonitorManager::subscribe(monitorIndex, channelIndex, cb)
    → VisualizationArea::addVisualizer(monitorIndex, channelIndex, vis)
    → createVisualizerFor(monitorIndex, channelIndex, mode, ...)
    → wave->addTrace(monitorIndex, channelIndex, kColors[channelIndex % 8])
```

## 推荐方案

### 核心思路

将 `SignalTreePanel` 从 `QTreeWidget` 改为 `QListWidget`，每行一个监听器 + checkbox，同时清理所有接口中的 `channelIndex` 参数，并将缩写 `mi`/`ci` 全部改写为全名 `monitorIndex`/`channelIndex`（`channelIndex` 移除后自然消失）。

### 新架构

```
SignalTreePanel (QListWidget)
  └─ 监听器名称 (item, checkbox, 存储 monitorIndex)

checkStateChanged(int monitorIndex, bool checked)
  → ExecutionPanelController
    → MonitorManager::subscribe(int monitorIndex, SampleCallback cb)
    → VisualizationArea::addVisualizer(int monitorIndex, SignalVisualizer* vis)
    → wave->addTrace(monitorIndex, kColors[monitorIndex % 8])
```

### 命名规范

Google C++ 风格指南要求避免非显而易见的缩写。本次清理对标已有代码中的全名用法（`MonitorManager` 接口已使用 `monitorIndex`/`channelIndex`），将所有缩写的位置统一为全名：

| 缩写 | 替换为 | 说明 |
|------|--------|------|
| `mi` | `monitorIndex` | 函数参数、局部变量 |
| `ci` | 移除 | 不再需要 |
| `mi_mon` | `monitorIndex` | 歧义缩写统一为全名 |
| 信号/方法名中含 `Ch` | 移除 | 如 `uncheckChannel` → `uncheckMonitor` |

### 改动清单

#### 1. MonitorManager — 接口与数据简化

| 位置 | 改动 |
|------|------|
| `MonitorTreeEntry` | 保留 `deviceType`（flushSamples 写入 .etlog 需要）、移除 `channelCount` |
| `subscribe(int monitorIndex, int channelIndex, SampleCallback cb)` | 改为 `subscribe(int monitorIndex, SampleCallback cb)` |
| `unsubscribe(int monitorIndex, int channelIndex)` | 改为 `unsubscribe(int monitorIndex)` |
| `displayMode(int monitorIndex, int channelIndex)` | 改为 `displayMode(int monitorIndex)` |
| `buffer_` 和 `subscribers_` 的 key 类型 | `QHash<QPair<int,int>, ...>` → `QHash<int, ...>` |
| `appendFromTopology` 中的 `channelCount=1` | 移除，不再需要 |
| `monitorTree()` | `MonitorTreeEntry` 减少字段，返回值不变 |
| `flushSamples()` 中 `.etlog` monitors 段 | 保留写入 `deviceType`（字段还在），移除 `channelCount` 写入 |

#### 2. SignalTreePanel — 扁平化改造

| 位置 | 改动 |
|------|------|
| `QTreeWidget` 成员 | 改为 `QListWidget` |
| `buildTree()` | 改为线性遍历 tree，每项一个 QListWidgetItem + checkbox |
| `node_map_` key | 从 `(monitorIndex << 16) \| channelIndex` 简化为纯 `monitorIndex`；类型改为 `QHash<int, QListWidgetItem*>` |
| `updateNodeValue(int monitorIndex, int channelIndex, text)` | 改为 `updateNodeValue(int monitorIndex, text)` |
| `uncheckChannel(int monitorIndex, int channelIndex)` | 改为 `uncheckMonitor(int monitorIndex)` |
| `checkStateChanged` 信号 | 从 `(int monitorIndex, int channelIndex, bool checked)` 改为 `(int monitorIndex, bool checked)` |
| `onFilterChanged` | 遍历 topLevelItem（不再有子节点），逻辑简化 |
| `setMonitorTree(tree, preCheckedChannels)` | 第二个参数从 `QList<QPair<int,int>>` 改为 `QList<int>` |
| `tree_data_` 缓存 | 类型不变（`MonitorTreeEntry` 列表） |
| 局部变量/参数缩写 | 所有 `mi` → `monitorIndex`，`ci` → 移除 |

#### 3. ExecutionPanelController — 适配新接口

| 位置 | 改动 |
|------|------|
| `checkStateChanged` 连接 lambda 参数 | `(int monitorIndex, bool checked)`，移除 `channelIndex` 相关逻辑 |
| `createVisualizerFor(monitorIndex, channelIndex, mode, ...)` 调用 | 改为 `createVisualizerFor(monitorIndex, mode, ...)` |
| 波形颜色 | `kColors[channelIndex % 8]` → `kColors[monitorIndex % 8]` |
| 可视化标题 | `"Monitor %1 Ch%2"` → `"Monitor %1"` |
| `addVisualizer(monitorIndex, channelIndex, vis)` 调用 | 改为 `addVisualizer(monitorIndex, vis)` |
| 订阅回调 lambda | `(const Sample& s)` 即可，不再传 `(monitorIndex, channelIndex)` |
| `visualizerClosed(monitorIndex, channelIndex)` | 改为 `visualizerClosed(monitorIndex)` |
| `clearData()` 中 `activeChannels()` 遍历 | 单循环 `monitorIndex` |
| `refreshMonitorTree()` 内部逻辑 | `activeChannels()` 返回类型变更后自动适配，预勾选列表类型从 `QList<QPair<int,int>>` 改为 `QList<int>` |
| 局部变量缩写 | 所有 `mi` → `monitorIndex`，`ci` → 移除 |

#### 4. VisualizationArea — 适配

| 位置 | 改动 |
|------|------|
| `addVisualizer(int monitorIndex, int channelIndex, SignalVisualizer* vis)` | 改为 `addVisualizer(int monitorIndex, SignalVisualizer* vis)` |
| `visualizerClosed` 信号 | 从 `(int monitorIndex, int channelIndex)` 改为 `(int monitorIndex)` |
| `activeChannels()` 返回值 | 从 `QList<QPair<int,int>>` 改为 `QList<int>`（只返回 monitorIndex） |
| `visualizer(int monitorIndex, int channelIndex)` | 改为 `visualizer(int monitorIndex)` |
| `removeVisualizer(int monitorIndex, int channelIndex)` | 改为 `removeVisualizer(int monitorIndex)` |
| `clearAll()` 内部逻辑 | `monitorIndex` 单索引查找 |
| 局部变量缩写 | 所有 `mi` → `monitorIndex`，`ci` → 移除 |

#### 4b. TopologyOutlineWidget — 监听器节点扁平化

| 位置 | 改动 |
|------|------|
| `addMonitorItem()` 中创建连线信息子节点 | 移除第 207-212 行的 `QTreeWidgetItem` 子节点创建代码 |
| 子节点 `ItemTag::Connection` 引用 | 随子节点创建一并移除 |

现在监听器名称已包含完整的连线信息（格式：`设备名_端口_UUT端口_UUT名_监听器`），子节点不再需要。

#### 5. SignalVisualizer 及其子类 — displayedSignals 与 channelIndex 清理

| 位置 | 改动 |
|------|------|
| `SignalVisualizer::displayedSignals()` 返回值 | `QList<QPair<int,int>>` → `QList<int>`（4 个子类 WaveformWidget/ValueLabelWidget/StateLEDWidget/DigitalMeterWidget 同步实现变更） |
| `channel_index_` 成员 | 移除（各子类） |
| 局部变量 `mi`/`ci` | 改为全名或移除 |
| `WaveformWidget::addTrace(int monitorIndex, int channelIndex, QColor)` | 改为 `addTrace(int monitorIndex, QColor)`；内部 `Trace` 结构体移除 `channelIndex` 字段；`findTraceIndex` 按 `monitorIndex` 查找 |
| `WaveformWidget::removeTrace(int monitorIndex, int channelIndex)` | 改为 `removeTrace(int monitorIndex)` |

#### 6. VisualizerFactory — 签名简化

| 位置 | 改动 |
|------|------|
| `createVisualizerFor(..., int channelIndex, ...)` | 改为 `createVisualizerFor(..., ...)`，移除 `channelIndex` 参数（当前已 Q_UNUSED） |

### 波形颜色策略

当前：`kColors[channelIndex % 8]` → channelIndex 恒为 0 后全蓝
改为：`kColors[monitorIndex % 8]` → 按监听器索引取色，多监测点同屏可区分

### 不清理的内容

| 代码 | 保留理由 |
|------|----------|
| `monitorIndex` 概念 | 监听器在全局列表中的索引仍然需要 |
| `MonitorTreeEntry` 结构体 | 仍然需要传递监听器列表给 UI；`deviceType` 保留因 flushSamples 写入 .etlog |
| `tree_cache_` | MonitorManager 内部缓存仍然需要 |
| 搜索框（QLineEdit） | 过滤功能仍然需要 |

### 全链路清理确认

以下位置全部从 `(monitorIndex, channelIndex)` 二元组改为 `monitorIndex` 单索引（同时修正常见的缩写 `mi`/`ci`）：

- `MonitorManager::subscribe/unsubscribe/displayMode`
- `MonitorManager::buffer_ / subscribers_` 的 key
- `SignalTreePanel::checkStateChanged` 信号
- `SignalTreePanel::updateNodeValue / uncheckMonitor`
- `SignalTreePanel::node_map_` key
- `ExecutionPanelController::checkStateChanged` lambda
- `ExecutionPanelController::createVisualizerFor` 调用
- `VisualizationArea::addVisualizer / visualizer / visualizerClosed`
- `VisualizationArea::activeChannels` 返回值
- `SignalVisualizer::displayedSignals` 返回值及其子类
- `WaveformWidget::addTrace / removeTrace`
- `VisualizerFactory::createVisualizerFor` 签名
- 可视化标题 `"Monitor N"`
- 各文件中局部变量的 `mi`/`ci` → 全名

### 影响评估

- **SignalTreePanel**：~150 行净简化代码
- **MonitorManager**：~10 行接口签名变更 + `subscribers_`/`buffer_` key 类型变更
- **ExecutionPanelController**：~30 行适配
- **VisualizationArea**：~30 行适配
- **SignalVisualizer 子类**：~40 行（displayedSignals 返回值 + channel_index_ 移除）
- **WaveformWidget**：~20 行（addTrace/removeTrace 签名 + Trace 内部清理）
- **VisualizerFactory**：~5 行签名简化
- **总净减少**：约 50 行代码（移除树型结构的父子层级逻辑和 channelIndex 传导）

### 决策记录

- **决策**：全链路移除 `channelIndex` 参数，并统一使用全名 `monitorIndex` 替代缩写 `mi`
- **理由**：`channelIndex` 是新架构下不再存在的概念，保留会导致后续维护者困惑；缩写 `mi` 不符合 Google C++ 风格指南
- **替代方案 A**：仅改 SignalTreePanel 为 QListView，保留 `channelIndex=0` 的接口签名——改动最小但遗留死参数
- **替代方案 B**：仅压平树型结构不移除 channelIndex——部分改善 UI 但接口层仍残留旧概念
