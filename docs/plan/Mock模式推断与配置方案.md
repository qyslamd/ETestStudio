# Mock 模式推断与配置方案

> **日期**: 2026-07-28
> **状态**: 设计完成（三轮审查通过）
> **关联**: `ideas.md` 「Mock 所有的插件」、`docs/plan/MockUUTSimulator设计.md`、`docs/01-规划/测试执行业务流程.md`

## 1. 问题陈述

### 1.1 现状

Mock 模式**完全由拓扑 `.etopo` 文件中每个 device 的 `"mock": true/false` 字段决定**：

```
engine_->loadTopology(etopoPath)
  ├─ checkMockConsistency(root)          // 所有设备 mock 值必须一致
  ├─ hw_manager_->loadFromTopology(root)
  │    └─ bool mock = dObj["mock"].toBool(false)
  │         └─ instantiateDevice(..., mock)  // mock=true 找 mock 插件，否则找真实插件
  └─ if hasMockDevices(root):
       MockUUTBuilder builder(icd_repo, root)
       builder.loadResponseConfigFile("<etopo_dir>/mock/MockResponses.json")
       builder.buildAll(mockUUTs)
       hw_manager_->setMockUUT(std::move(mockUUTs))
```

两个问题：
1. **`mock` 字段冗余**：拓扑中设备选用的 `pluginId` 已隐含 mock/真实信息（`PluginMetaData::is_mock`），`mock` 字段是重复标记
2. **Mock 配置无 UI**：`MockResponses.json` 只能手写，拓扑编辑器中也没有设置 `mock` 字段的 UI

### 1.2 用户诉求拆解（来自 `ideas.md`）

ideas.md 原文：
> mock 模式需要一个全局开发来开启，提供界面入口，放在执行页面，在开始执行之前，弄一个 checkbox，勾选上就走 Mock，否则就真实硬件，**不靠拓扑或测试程序中编写字段**
>
> mock 的参数配置应该也需要提供 UI 界面能够让用户自行定义或者配置 UUT(mock的)和 mock 的激励设备之间的交互的数据

经讨论梳理（见 `docs/01-规划/测试执行业务流程.md`），三条诉求拆解如下：

| 原诉求 | 结论 | 理由 |
|---|---|---|
| 不靠拓扑中编写 mock 字段 | **采纳** -- 移除 `mock` 字段 | `pluginId` 已隐含 `is_mock`，字段冗余 |
| 全局 checkbox 切换模式 | **放弃** -- 不需要 | 模式由拓扑选用的插件类型自动决定，运行时无法切换（设备在拓扑加载时实例化） |
| UI 配置 Mock 交互数据 | **保留** -- 需要 | `MockResponses.json` 手写易错，需 UI 编辑 |

### 1.3 目标

- 移除拓扑 `mock` 字段，模式通过 `pluginId` 反查 `is_mock` 自动推断
- 混合模式（Mock + 真实设备共存）拒绝运行并提示用户
- Mock 响应数据可通过 UI 编辑，不再依赖手写 JSON
- 不破坏现有 MockUUTBuilder / HardwareManager 架构

---

## 2. 架构回顾

### 2.1 三层 Mock 结构

| 层 | 作用 | 位置 |
|---|---|---|
| **Mock 插件**（5 个） | 替换真实设备插件，`openDevice()` 是 no-op，`is_mock=true` | `src/plugins/mock/{serial,can,a429,ad,da}/` |
| **MockUUT** | 被测单元模拟器，含 FramePortSimulator（收帧回回复）+ ChannelPortSimulator（AD/DA 通道值） | `src/engine/MockUUTBuilder.{h,cpp}` |
| **MockResponses.json** | 响应数据配置，按 frameId 配置回复字节 / AD fixedValue | `<etopo_dir>/mock/MockResponses.json` |

### 2.2 MockResponses.json 格式（当前格式，本期将扩展，见 4.3.6 与讨论点 4）

```json
{
  "version": "1.0",
  "portBehaviors": [
    {
      "productName": "综合Mock测试UUT",
      "deviceId": "mock-serial-1",
      "port": "串口控制口",
      "direction": "receive",
      "responses": [
        { "frameId": 1, "description": "...", "responseHex": "86 00 00 96 00 00" }
      ]
    },
    {
      "productName": "综合Mock测试UUT",
      "deviceId": "mock-ad-1",
      "port": "AD采集口",
      "fixedValue": 0.0,
      "description": "模拟AD通道采集到5.0V"
    }
  ]
}
```

- 帧型端口（serial/can/a429）：`responses[]` 按 `frameId` 配置 `responseHex`
- 通道型端口（ad/da）：`fixedValue` 配置固定值

### 2.3 模式确定机制（现状 vs 目标）

**现状**：读 `dObj["mock"]` 字段 -> 传给 `instantiateDevice` -> 决定搜 mock 还是真实插件池

**目标**：读 `pluginId` -> `PluginManager::plugin(pluginId)->metaData().is_mock` -> 自动推断 -> 一致性校验

```
loadFromTopology(root)
  └─ 遍历 devices[]:
       pluginId = dObj["pluginId"]
       is_mock = PluginManager::plugin(pluginId)->metaData().is_mock
       instantiateDevice(deviceId, pluginId, properties, is_mock)
```

### 2.4 现有约束

- `checkMockConsistency`：当前读 `mock` 字段校验一致性 -> 改为读 `is_mock`
- `instantiateDevice`：`mock=true` 只搜 mock 插件池，`mock=false` 只搜真实插件池，严格互斥不 fallback
- `MockUUTBuilder` 需要 `icd::Repository` 解析帧名到 frameId，无 ICD 时构建失败
- 设备在拓扑加载时（项目打开）实例化，运行时复用，不支持运行时切换模式

---

## 3. 讨论点

### 讨论点 1：移除 `mock` 字段，模式由 pluginId 自动推断

**选项 A：移除 `mock` 字段，通过 pluginId 反查 is_mock 推断模式**

- `.etopo` 的 `devices[]` 不再包含 `mock` 字段
- `HardwareManager::loadFromTopology` 移除 `dObj["mock"]` 读取，改为通过 `pluginId` 反查 `PluginMetaData::is_mock`
- `TestExecutionEngine::loadTopology` 的 `checkMockConsistency` / `hasMockDevices` 改为通过 `is_mock` 判断
- 模式在拓扑加载时自动确定，无需运行时开关
- 混合模式（部分 `is_mock=true` + 部分 `is_mock=false`）拒绝运行

**选项 B：保留 `mock` 字段，但运行时用 pluginId 校验**

- `mock` 字段保留作为显式标记
- 加载时用 `is_mock` 校验 `mock` 字段是否一致，不一致报错
- 冗余字段，增加维护成本

**倾向：A**

`mock` 字段是 `pluginId` 的冗余派生信息 -- 选了 Mock 插件自然就是 Mock 设备。保留两个字段必然产生不一致风险。移除 `mock` 字段后，`pluginId` 是唯一真相源，模式自动推断，零冗余。

**影响范围**：
- `HardwareManager::loadFromTopology` 移除 `mock` 字段读取，改用 `pluginId` 反查 `is_mock`
- `HardwareManager::instantiateDevice` 的 `mock` 参数改用反查结果
- `TestExecutionEngine::loadTopology` 移除 `checkMockConsistency` / `hasMockDevices` 的 `mock` 字段逻辑，改为遍历设备反查 `is_mock`
- 现有 `.etopo` 文件清理 `mock` 字段（反序列化本就忽略未知字段，不删也不报错，但格式上明确移除）
- `docs/plan/MockUUTSimulator设计.md` 第 2.3 节「Mock 标记」、第 3.4 节引擎匹配逻辑中 `mock` 字段描述标注废弃

---

### 讨论点 2：Mock 配置 UI 的形态与入口

**核心认知：协议感知是前提，不是选项**

Mock UUT 要回复什么数据，需要完整协议信息。当前 `MockResponses.json` 是裸 hex（`"responseHex": "86 00 00 96 00 00"`），但用户要配的是**工程值**（如电流 1.50A），需经 ICD 编码才是 hex。让用户手算 hex 不现实。

Mock 配置与测试程序编辑本质相似：

| | 测试程序 | Mock 配置 |
|---|---|---|
| 触发 | 用户编写步骤 | UUT 收到某个帧 |
| 动作 | SET/CHECK 信号 = 工程值 | 回复某个帧，各字段 = 工程值 |
| 需要 | 选信号 + 填工程值 | 选回复帧 + 填各字段工程值 |
| 编码 | 引擎 `SignalCodec` 编码为帧 | 同样需要 ICD 编码为帧 |

因此 Mock 配置 UI 必须解析 ICD 帧结构，用户填字段工程值，系统自动编码为 hex 存入 `MockResponses.json`。裸 hex 编辑不可行。

**选项 A：弹出对话框，拓扑编辑器入口**

- 拓扑编辑器工具栏加「Mock 配置」按钮，点击弹出模态对话框
- 空间不足以展示 UUT -> 端口 -> 帧 -> 字段的多级编辑

**选项 B：停靠面板，常驻编辑页**

- QADS 停靠面板，可与拓扑编辑器并排
- 与拓扑编辑器争夺编辑区空间

**选项 C：独立编辑器（page0 的一个 tab）**

- 作为与拓扑编辑器、协议编辑器、测试程序编辑器并列的独立编辑器类型
- 两级导航：侧边 UUT -> 端口 -> 帧响应，右侧 ICD 字段编辑区
- 协议感知：解析 ICD 帧结构，用户填字段工程值，系统自动编码为 hex
- 空间充足，编辑器生命周期与项目一致，与测试程序编辑器风格一致

**决议：C**

独立编辑器最匹配 Mock 配置的本质 -- 它不是拓扑的附属操作，而是一个需要协议感知、多级导航、足够编辑空间的独立编辑场景。

**编辑器结构**（单拓扑约束下，侧边不再需要拓扑文件列表层）：

```
Mock 配置编辑器
├─ 侧边：UUT 列表（topology.etopo 内 products[]）
│   ├─ UUT1
│   │   ├─ 端口1（serial）── 收到帧A -> 回复帧B（ICD 字段值编辑）
│   │   ├─ 端口2（ad）──── fixedValue 编辑
│   └─ UUT2
│       └─ 端口1（can）── 收到帧C -> 回复帧D（ICD 字段值编辑）
└─ 右侧：当前选中端口/帧的编辑区（ICD 字段列表 + 工程值输入 + hex 预览）
```

**边界问题确认**：

1. **一个拓扑文件允许几个 UUT？** 不限。`.etopo` 的 `products[]` 是数组，`MockUUTBuilder::buildAll` 为每个 product 创建独立 `MockUUT`。`MockResponses.emock` 内部用 `productName` 区分不同 UUT 的配置。

2. **多个拓扑文件怎么区分 Mock 配置？** 单拓扑约束下此问题不存在 —— 一个项目只有一个 `topology.etopo`，对应一个 `MockResponses.emock`，无需按拓扑区分。

**编解码能力来源**：Mock 配置编辑器需要 ICD 帧编解码能力（工程值 <-> hex）。现有 `SignalCodec`（引擎层）具备此能力，编辑器需通过接口复用，避免在 UI 层重新实现编解码逻辑。复用方式见 4.1 节决策：直接依赖 engine 层，不下沉为独立工具层。

---

### 讨论点 3：Mock 配置数据的存储位置与文件格式

**文件后缀**：`.emock`（专属后缀，与 `.etopo`/`.eprotox`/`.tcase` 风格统一）

**底层格式**：JSON（与当前 `MockResponses.json` 数据结构兼容，迁移成本最低 -- 改后缀即可）

**关联方式**：共享文件，内部按 `productName + deviceId + port` 三元组区分。单拓扑下一个项目只有一个 `topology.etopo`，对应一个 `topology/MockResponses.emock`，无需按拓扑分文件，不存在关联断链问题。

**选项 A：平铺到拓扑文件同级，改后缀为 `.emock`**

- `MockResponses.emock` 平铺在 `topology/` 目录下，与 `topology.etopo` 同级
- 文件内部 `portBehaviors[]` 数组，通过 `productName + deviceId + port` 三元组区分不同 UUT/端口的配置
- 仅 Mock 模式需要此文件，真实模式不需要
- 代码迁移：`loadResponseConfigFile` 路径从 `/mock/MockResponses.json` 改为 `/MockResponses.emock`
- 与当前架构兼容（去掉了多余的 `mock/` 子目录层）

```
topology/
  ├─ topology.etopo
  └─ MockResponses.emock   ← 平铺同级
```

**选项 B：项目级统一配置**

- `<project>/mock/MockResponses.emock`，项目级共用
- 与选项 A 本质相同，只是位置从 `topology/` 提到项目根 `mock/`
- 意义不大，反而与拓扑目录脱节

**选项 C：每个拓扑独立 .emock 文件**

- 单拓扑约束下不适用（项目只有一个 `topology.etopo`，无需按拓扑分文件）
- 原多拓扑设想：按拓扑文件名分文件，存在拓扑改名断链问题

**决议：A**

共享文件与当前架构一致，`MockUUTBuilder` 内部已有 `productName + deviceId + port` 过滤逻辑，无需改动。单拓扑下一个项目对应一个 `MockResponses.emock`，平铺在 `topology/` 同级。没有关联断链问题，迁移成本最低（改后缀 + 去掉 `mock/` 子目录层）。

**编辑器交互**：
1. 编辑器加载项目唯一的 `topology/topology.etopo`（若含 Mock 设备）
2. 编辑器加载 `topology/MockResponses.emock`，按 `productName` 列出所有 UUT
3. 用户选中 UUT -> 端口 -> 帧，编辑字段工程值，保存回写 `.emock` 文件

---

### 讨论点 4：AD/DA 通道值的配置增强

**现状（两层 AD 模拟，独立运行）**：

| 层 | 位置 | 服务场景 | 当前能力 |
|---|---|---|---|
| ADChannelSimulator（引擎层） | `MockUUTBuilder.cpp:138` | 测试程序 CHECK AD 信号（`HardwareManager.cpp:222` 调用） | `fixedValue != 0` 返回固定值；否则**硬编码正弦波**（kAmplitude=5000/经 ICD scale 为 ±5V，kFrequency=1Hz） |
| MockADPlugin（插件层） | `MockADPlugin.cpp:244` | AD 采集/监听（ring_buffer 填充） | `generateSample`：有注入数据循环回放，否则正弦波+噪声；已有 `injectChannelData` 注入任意波形 |

- DA 侧当前**无 DAChannelSimulator 类**，仅 `MockDAPlugin::writeChannel/readbackChannel`（回读上次写入值）
- `injectChannelData`（序列回放基础）已存在但未接入 MockResponses 配置

**选项 A：本期维持现状（fixedValue + 默认正弦波）**

- UI 只提供 fixedValue 输入框
- 正弦波作为 fixedValue=0 时的隐式行为
- 省工作量，但正弦波参数硬编码不可调，且两层行为独立不同步

**选项 B：增加波形选项**

- AD/DA 可选：固定值 / 正弦波 / 方波 / 三角波
- 配置幅值、频率、偏置
- 需扩展 MockResponses 格式 + ADChannelSimulator + MockADPlugin

**选项 C：增加时序序列**

- AD/DA 按时间顺序输出预设值序列，循环回放
- 复用 `injectChannelData` 思路
- 需扩展格式和模拟器逻辑

**选项 D：AD 支持 B+C，两层统一；DA 维持现状**

- AD 支持 fixed / waveform / series 三种模式；DA 维持现状（fixedValue + 回读上次写入值）
- 提取公共 `WaveformGenerator` 工具类（方案 1），ADChannelSimulator 和 MockADPlugin 都调它，两层行为天然一致
- DA 不对称增强 -- DA 是激励设备输出到 UUT（由测试程序 SET 控制），物理语义与 AD（UUT 输出被采集）不同，Mock 模拟 DA 波形场景少，强行对称会引入回读语义变更风险
- 序列是波形的超集（序列能离散表达任意波形），两者互补：波形参数化易用，序列覆盖任意曲线

**决议：D**

理由：
1. fixedValue + 硬编码正弦波太局限，无法模拟传感器特性曲线、故障波形等真实场景
2. 两层独立是技术债 -- 借此次增强统一为 WaveformGenerator，消除行为漂移风险
3. `injectChannelData` 基础设施已存在，序列模式（C）是复用而非新建，代价可控
4. DA 不对称增强 -- DA 物理语义与 AD 不同（DA 是输入到 UUT，AD 是 UUT 输出），Mock 模拟 DA 波形场景少，维持现状避免回读语义变更

**两层统一方案（方案 1：WaveformGenerator）**：

```cpp
// src/core/WaveformGenerator.h
class WaveformGenerator {
 public:
  enum class Mode { Fixed, Waveform, Series };
  enum class WaveformType { Sine, Square, Triangle };

  double generate(qint64 sampleIndex, double sampleRate) const;

  void setFixed(double value);
  void setWaveform(WaveformType type, double amplitude,
                   double frequency, double offset);
  void setSeries(const QVector<double>& data);

 private:
  Mode mode_ = Mode::Fixed;
  double fixed_value_ = 0.0;
  WaveformType waveform_type_ = WaveformType::Sine;
  double amplitude_ = 1.0;
  double frequency_ = 1.0;
  double offset_ = 0.0;
  QVector<double> series_data_;
};
```

- ADChannelSimulator 内部持有 `WaveformGenerator`，`readChannelValue` 调 `generate()`
- MockADPlugin 的生成逻辑也调 `WaveformGenerator`，`injectChannelData` 改为设置 series 模式
- 单一真相源，AD 两层行为一致（DA 不改造，维持 readbackChannel 回读上次写入值）

---

### 讨论点 5：跨信号联动（动态回复）

现状：收到 frameId X -> 回复固定字节，不能根据收到的帧内容动态生成回复。

**选项 A：本期不做，保持固定回复**

- `responseHex` 是静态的
- 足够验证编解码链路和基本流程

**选项 B：支持字段映射**

- 回复帧的某些字段从收到的帧提取
- 需要 ICD 帧字段解析能力
- 格式需扩展（如 `responseFields` 按字段名映射）

**倾向：A**

跨信号联动在 `MockUUTSimulator设计.md` 第 10 节已明确列为「未纳入 Phase 1」的内容。本期聚焦模式推断 + UI 配置，动态回复作为后续演进项。

---

## 4. 推荐方案总结

| 讨论点 | 决议 | 核心决策 |
|---|---|---|
| 1. 移除 mock 字段 | **A** | 移除 `mock` 字段，通过 pluginId 反查 `is_mock` 自动推断模式 |
| 2. UI 形态与入口 | **C** | 独立编辑器（page0 tab），协议感知，字段级工程值编辑 + 自动编码 |
| 3. 存储位置与文件格式 | **A** | `topology/MockResponses.emock`，平铺同级，内部按 productName+deviceId 区分 |
| 4. AD/DA 增强 | **D** | AD 支持 B+C（WaveformGenerator 统一两层），DA 维持现状 |
| 5. 跨信号联动 | **A** | 本期不做，保持固定回复 |

### 4.1 改动范围预估

**引擎层（`src/engine/`）**：
- `HardwareManager::loadFromTopology` 移除 `dObj["mock"]` 读取，改为通过 `pluginId` 反查 `PluginManager::plugin(pluginId)->metaData().is_mock`
- `HardwareManager::instantiateDevice` 的 `mock` 参数改用反查结果
- `TestExecutionEngine::loadTopology` 的 `checkMockConsistency` / `hasMockDevices` 改为遍历设备反查 `is_mock`，不再读 `mock` 字段
- `TestExecutionEngine::loadTopology` 中 `loadResponseConfigFile` 路径从 `/mock/MockResponses.json` 改为 `/MockResponses.emock`
- `MockUUTBuilder` 改动：response 结构从 `responseHex` 改为 `replyFrameName` + `fieldValues[]`（工程值），`buildSingleUUT` 里对每个 response 逐字段调 `SignalCodec::encodeToFrame` 编码后按 bitOffset 合并为整帧字节（见 4.3.6 决策 A）
- 新增 `WaveformGenerator`（`src/core/WaveformGenerator.{h,cpp}`，放 core 层而非 engine 层 -- 插件只依赖 etest_core 不依赖 etest_engine，放 core 让插件和引擎都能用，避免循环依赖）：支持 fixed/waveform/series 三种模式，供 ADChannelSimulator / MockADPlugin 共用（见讨论点 4 决议 D）
- `ADChannelSimulator` 改造：移除硬编码正弦波，内部持有 `WaveformGenerator`，`readChannelValue` 调 `generate(sampleIndex, sampleRate)`，sampleRate 固定 1000（保持与当前硬编码 1kHz 一致）
- `MockUUTBuilder::buildChannelSimulator` 扩展：构建时读取已加载的 `frame_responses_` 成员，解析 AD 端口的 mode + 参数，配置 WaveformGenerator（DA 端口维持 fixedValue，不配置 WaveformGenerator）
- `MockUUTBuilder` 编码链路闭合：response 的 `fieldValues[]` 编码为 hex 时，复用 `SignalResolver::buildFromIcd`（4.1 编解码能力复用节已暴露的公共接口）从 ICD 构造 `ResolvedSignal`，再调 `SignalCodec::encodeToFrame`

**应用层（`src/app/`）**：
- 新增 `MockConfigEditor`（独立编辑器，实现 `IEditor` 接口）：
  - 侧边：UUT -> 端口 -> 帧响应 树形导航（单拓扑下无需拓扑文件列表层）
  - 右侧：ICD 字段列表 + 工程值输入 + hex 预览
  - 协议感知：解析 ICD 帧结构，用户填字段工程值，系统自动编码为 hex，保存回写 `MockResponses.emock`
  - 仅 Mock 模式项目可见此编辑器
- 编辑器注册到 `EditorManager`，可通过项目结构树或 Ribbon 入口打开

**编解码能力复用**：
- 决策：MockConfigEditor 直接依赖引擎层，复用 `SignalCodec` + `SignalResolver` 的 ICD 属性提取逻辑（`fillFromIcd`，需暴露为公共接口，`src/engine/`）
- 依据：`src/app/CMakeLists.txt:214` 已链接 `etest_engine`，非新增跨模块依赖；编解码链路 engine 已完备，下沉为独立工具层是过度设计
- 实施要点：MockConfigEditor 直接从 ICD 选帧+字段（不走 SignalRegistry/UUID），需从 `icd::Frame/Node` 构造 `ResolvedSignal`。当前 `SignalResolver::fillFromIcd` 是 private（`src/engine/SignalResolver.h:79`），需暴露为公共接口（如新增 `buildFromIcd(frameName, nodePath)`），供编辑器复用 ICD 属性提取逻辑，避免 UI 层重写编解码

**拓扑层（`src/topology/`）**：
- 序列化/反序列化移除 `mock` 字段（现状本就不处理，确认即可）
- 设备选择 UI 增强（`DevicePaletteWidget::populateDeviceTypes`，`src/topology/DevicePaletteWidget.cpp:78-115`）：当前完全不区分 mock/真实插件（`src/topology` 零 `is_mock` 引用），mock 插件仅因满足 `category=="device"` 过滤条件而显示。改为按 mock/真实分组或标记（推荐：列表分「真实设备」「Mock 设备」两组 + Mock 项名加 `[Mock]` 前缀），遍历 `PluginManager::loadedPlugins()` 时按 `PluginMetaData::is_mock` 分组（`devicesByMockType()` 适合按类型精确查询，但分组场景直接遍历 loadedPlugins 更自然）
- 修复 `onAddDeviceFromTemplate` 的 `.first()` 歧义（`TopologyEditorWidget.cpp:1521-1528`）：同 `device_type` 多插件时取第一个，不区分 mock/真实，需明确选择或校验

**插件层（`src/plugins/`）**：
- `MockADPlugin::generateSample` 改为调 `WaveformGenerator`（而非自己生成正弦波），`injectChannelData` 改为设置 series 模式。量程映射约定：`WaveformGenerator` 输出归一化值（amplitude=1.0），MockADPlugin 乘以 `range` 缩放为实际采集值（range 运行时可变，amplitude 无法预设为绝对值）
- `MockDAPlugin` 无改动（DA 维持现状，readbackChannel 回读上次写入值）

**数据迁移**：
- 现有 `.etopo` 文件删除 `devices[].mock` 字段（反序列化本就忽略未知字段，不删也不报错，但格式上明确移除）
- 现有 `mock/MockResponses.json` 改名为 `MockResponses.emock` 并移到 `topology/` 目录下，与 `topology.etopo` 同级（去掉 `mock/` 子目录层）
- 单拓扑约束下，`topology/` 目录只保留 `topology.etopo`（多余的 `.etopo` 文件按 `单拓扑约束方案.md` 处理）
- 涉及文件：`temp/projects/demo_mock/` 下的 `.etopo` 和 `mock/MockResponses.json`

**文档更新**：
- `docs/plan/MockUUTSimulator设计.md`：全文凡涉及 `mock` 字段与 `/mock/` 路径处均标注废弃/更新，包括第 1.2 节设计约束、第 2.2 节拓扑 JSON 示例、第 2.3 节 Mock 标记、第 3.4 节引擎匹配逻辑、第 7 节 loadTopology 代码示例、第 9 节文件清单，指向本文档
- `docs/01-规划/测试执行业务流程.md` 第 179 行已更新为 `topology/MockResponses.emock` 平铺在 `topology/` 目录下
- `ideas.md` 「Mock 所有的插件」条目更新结论

### 4.2 编辑器可见性与模式判断

MockConfigEditor 的入口可见性取决于项目模式，判断逻辑如下：

- **判断时机**：项目打开、拓扑加载完成后（`syncProjectTopologies` 执行后），根据拓扑内设备的 `is_mock` 判断项目模式
- **真实模式项目**（所有设备 `is_mock=false`）：MockConfigEditor 入口**隐藏** -- 真实模式无需 Mock 配置，用户不应看到此入口
- **Mock 模式项目**（所有设备 `is_mock=true`）：入口可见，正常打开编辑器
- **空项目**（无设备）：入口可见，编辑器打开后显示空列表 + 提示「请先在拓扑中添加 Mock 设备」
- **混合模式**（部分 mock + 部分真实）：MockConfigEditor 入口**可见**（用户仍可编辑 Mock 部分的响应配置）。编辑阶段**不强制阻止**混选，靠运行时 `loadTopology` 的 `checkMockConsistency` 拒绝运行并提示。取舍：设备选择 UI 增强（4.1 拓扑层决策）后用户已能区分 mock/真实，混选是有意行为或失误，运行时兜底足够；强制阻止反而限制临时搭场景

### 4.3 MockConfigEditor UI 设计

#### 4.3.1 整体布局

QADS 停靠布局，与测试程序编辑器风格一致：

```
┌───────────────────────────────────────────────────────────┐
│ Ribbon: [新建响应] [删除响应] | [保存]                     │
├─────────────┬─────────────────────────────────────────────┤
│ 导航树       │  编辑区                                      │
│             │                                             │
│ [UUT] UUT1  │  (帧响应编辑 / 通道值编辑 / 端口信息)         │
│  ├ [serial] │                                             │
│  │  ├ 收到..│                                             │
│  │  └ 收到..│                                             │
│  ├ [ad]     │                                             │
│  └ [da]     │                                             │
│ [UUT] UUT2  │                                             │
│  └ ...      │                                             │
├─────────────┴─────────────────────────────────────────────┤
│ topology.etopo | Mock 模式 | 未保存 *                      │
└───────────────────────────────────────────────────────────┘
```

- 左侧导航树：dock 面板，可隐藏/浮动
- 右侧编辑区：主区域，按选中节点类型切换内容
- 底部状态栏：当前拓扑文件、项目模式、未保存标记

#### 4.3.2 左侧导航树

树形结构：UUT -> 端口 -> 响应。单拓扑下不以拓扑文件为根，直接以 UUT 为顶层节点。

```
[拓扑] topology.etopo (Mock 模式)
├─ [UUT] 综合Mock测试UUT
│   ├─ [serial] 串口控制口 (收)
│   │   ├─ 收到 控制指令(1) → 回复 状态回传
│   │   └─ 收到 电压设定(2) → 回复 电压输出
│   ├─ [ad] AD采集口 (固定值: 5.0V)
│   └─ [da] DA输出 (固定值: 0.0V)
└─ [UUT] 备份UUT
    └─ [can] CAN总线 (收)
        └─ 收到 指令帧(10) → 回复 状态帧
```

节点类型与图标（SVG，非 emoji）：

| 节点类型 | 标识 | 显示内容 |
|---|---|---|
| 拓扑根 | `[拓扑]` | topology.etopo + 模式标记 |
| UUT | `[UUT]` | productName |
| 帧型端口 | `[serial]`/`[can]`/`[a429]` | 端口名 + 方向(收/发) |
| 通道型端口 | `[ad]`/`[da]` | AD：模式摘要（如「波形: 正弦 5V 1Hz」「序列: 10 点」）；DA：固定值摘要（如「固定值: 5.0V」） |
| 帧响应 | `收到..→回复..` | 收到帧名(frameId) → 回复帧名 |

交互：
- 点击端口节点：右侧显示端口信息
- 点击帧响应节点：右侧显示帧响应编辑区
- 点击 AD 端口节点：右侧显示模式切换 + 参数编辑；点击 DA 端口节点：右侧显示固定值编辑
- 右键帧型端口：新建响应（选收到帧 + 选回复帧）
- 右键响应：删除

#### 4.3.3 右侧编辑区 -- 帧响应（核心场景）

选中「帧响应」节点时，显示字段级工程值编辑 + hex 预览：

```
┌──────────────────────────────────────────────────────┐
│ 触发条件: 收到 [控制指令 ▼] (frameId=1)               │
│ 回复帧:   [状态回传 ▼] (frameId=101)                  │
├──────────────────────────────────────────────────────┤
│ 回复帧字段 (ICD 结构自动加载):                         │
│ ┌────────────┬──────────┬──────┬────────┬──────────┐ │
│ │ 字段路径    │ 工程值    │ 单位 │ 范围   │ hex 预览 │ │
│ ├────────────┼──────────┼──────┼────────┼──────────┤ │
│ │ /电压      │ [1.50]   │ V    │ 0~5    │ 99       │ │
│ │ /电流      │ [0.96]   │ A    │ 0~3    │ 60       │ │
│ │ /状态字    │ [1]      │ -    │ 0~1    │ 01       │ │
│ └────────────┴──────────┴──────┴────────┴──────────┘ │
├──────────────────────────────────────────────────────┤
│ 帧预览: 86 00 00 96 00 00          [复制 hex]         │
│ 帧长度: 6 字节                                       │
└──────────────────────────────────────────────────────┘
```

- **触发条件**：下拉框从端口绑定的 ICD 帧列表选「收到帧」
- **回复帧**：下拉框从 ICD 帧列表选「回复帧」，选中后自动加载该帧的字段结构
- **字段列表**：从 `icd::Repository` 解析回复帧的 Node 树，列出所有叶子字段
- **工程值输入**：用户填工程值，`SignalCodec::encodeToFrame` 实时编码为 hex
- **hex 预览**：每字段 hex + 整帧 hex，实时更新
- **量程/范围**：从 ICD `NodeAttrs`（min/max/scale_a/scale_b）读取并显示

#### 4.3.4 右侧编辑区 -- 通道型端口

选中 `[ad]` 端口节点时，显示模式切换 + 对应参数编辑（见讨论点 4 决议 D）；选中 `[da]` 端口节点时，仅显示固定值编辑：

```
┌──────────────────────────────────────────────────────┐
│ 端口: AD采集口 (ad)                                   │
├──────────────────────────────────────────────────────┤
│ 模式: (○)固定值  (○)波形  (○)序列                     │
│                                                      │
│ [波形模式]                                            │
│ 类型: [正弦 ▼]  幅值: [5.0] V  频率: [1.0] Hz         │
│ 偏置: [0.0] V                                        │
│ ┌────────────────────────┐                           │
│ │  波形预览图              │                           │
│ └────────────────────────┘                           │
│                                                      │
│ [序列模式]                                            │
│ ┌────────┬────────┐                                   │
│ │ 索引    │ 值(V)   │                                   │
│ ├────────┼────────┤                                   │
│ │ 0      │ [1.0]  │                                   │
│ │ 1      │ [1.5]  │                                   │
│ └────────┴────────┘                                   │
│ [添加] [删除]  循环回放                                │
└──────────────────────────────────────────────────────┘
```

- 三种模式切换：固定值 / 波形 / 序列（对应 `WaveformGenerator::Mode`）
- **固定值模式**：单一 `fixedValue` 输入框
- **波形模式**：类型下拉（正弦/方波/三角波）+ 幅值/频率/偏置输入 + 波形预览图（实时绘制）
- **序列模式**：表格编辑预设值序列（索引/值），循环回放，支持添加/删除行
- 量程从 ICD 或设备配置读取，用于约束输入范围
- DA 端口维持现状：仅 fixedValue 输入框（DA 不增强，见讨论点 4 决议 D）

#### 4.3.5 右侧编辑区 -- 端口 / UUT 信息

选中「端口」节点（非响应子节点）时（以帧型端口为例）：

```
┌──────────────────────────────────────────────────────┐
│ 端口: 串口控制口 (serial)                             │
│ 方向: 接收 (receive)                                  │
│ 绑定帧: 控制指令, 电压设定                             │
│ 响应数: 2                                             │
└──────────────────────────────────────────────────────┘
```

通道型端口：AD 显示模式 + 参数摘要（如「模式: 波形 (正弦 5V 1Hz)」），DA 显示固定值摘要（如「固定值: 5.0V」），均不显示响应数。

选中「UUT」节点时：

```
┌──────────────────────────────────────────────────────┐
│ UUT: 综合Mock测试UUT                                  │
│ deviceId: mock-serial-1                               │
│ 端口数: 3 (serial×1, ad×1, da×1)                      │
│ 响应数: 2                                             │
└──────────────────────────────────────────────────────┘
```

#### 4.3.6 数据结构扩展

当前 `MockResponses.json` 的 response 只有 `frameId`(收到帧) + `responseHex`(裸 hex)，**没有标识回复的是哪个 ICD 帧**。要做字段级工程值编辑，编辑器必须知道回复帧才能加载字段结构。

两种存储策略：

| 策略 | 存储内容 | 运行时 | 编辑器 |
|---|---|---|---|
| **A. 存工程值** | `replyFrameName` + `fieldValues[]`(工程值) | `MockUUTBuilder` 运行时用 `SignalCodec` 编码为 hex | 直接编辑工程值，无需解码 |
| B. 存 hex + 元数据 | `replyFrameName` + `responseHex` | `MockUUTBuilder` 直接用 hex（兼容现状） | 编辑器需解码 hex 反显字段值，编解码双向 |

**决议 A**。理由：存工程值是单一真相源，运行时和编辑器都从工程值出发编码，无 hex <-> 工程值双向转换的同步问题。`MockUUTBuilder` 改动小（`buildSingleUUT` 里对每个 response 调 `SignalCodec::encodeToFrame` 编码）。策略 B 的 hex 解码反显在字段边界对齐、字节序处理上容易出错。

扩展后的 `MockResponses.emock` 帧型端口 response 结构（策略 A）：

```json
{
  "frameName": "控制指令",
  "replyFrameName": "状态回传",
  "fieldValues": [
    { "nodePath": "/电压", "engValue": 1.50 },
    { "nodePath": "/电流", "engValue": 0.96 },
    { "nodePath": "/状态字", "engValue": 1 }
  ]
}
```

- 去掉 `frameId`（运行时 MockUUTBuilder 从 `frameName` 查 ICD 得到 frameId，避免冗余）
- `replyFrameName` 标识回复帧，编辑器加载其 ICD 字段结构

通道型端口 AD 扩展后结构（讨论点 4 决议 D，三种模式；DA 维持现状仅 fixedValue）：

```json
{
  "productName": "综合Mock测试UUT",
  "deviceId": "mock-ad-1",
  "port": "AD采集口",
  "mode": "waveform",
  "fixedValue": null,
  "waveform": { "type": "sine", "amplitude": 5.0, "frequency": 1.0, "offset": 0.0 },
  "series": null
}
```

- `mode`：`"fixed"` / `"waveform"` / `"series"`，对应 `WaveformGenerator::Mode`
- `fixedValue`：mode=fixed 时有效
- `waveform`：mode=waveform 时有效（type/amplitude/frequency/offset）
- `series`：mode=series 时有效（预设值数组，循环回放）
- DA 端口维持现状：仅 `fixedValue`，不扩展 mode/waveform/series（DA 不增强，见讨论点 4 决议 D）

> 此扩展已列入 4.1 改动范围（MockUUTBuilder 从 fieldValues 编码为 hex；AD 端口 mode + 参数配置 WaveformGenerator）。

---

## 5. 未纳入本期

| 特性 | 说明 |
|---|---|
| 跨信号联动 | 收到帧字段动态填充回复帧，依赖 ICD 字段解析 |
| Mock 配置导入导出 | 跨项目复用 Mock 配置 |
| 1553B / 1394 / IO 的 Mock | 需先定义插件接口，见 `MockUUTSimulator设计.md` 第 10 节 |

---

## 6. 实施计划

按依赖顺序分 6 个阶段。同一阶段内的任务无相互依赖，可并行；阶段间有依赖。

### 进度（截至 2026-07-29）

| 阶段 | 任务 | 状态 |
|---|---|---|
| 1. 基础设施 | T1-T4 | 已完成 |
| 2. 模式推断 | T5-T7 | 已完成 |
| 3. AD 增强 | T8-T9 | 已完成 |
| 4. MockUUTBuilder 扩展 | T10-T12 | 已完成 |
| 5. MockConfigEditor | T13-T18 | 已完成 |
| 6. 数据迁移 + 文档 | T19-T20 | 已完成 |

### 阶段 1：基础设施（无依赖，可并行）

| 任务 | 内容 | 改动文件 | 依赖 |
|---|---|---|---|
| T1 | 新增 `WaveformGenerator`（fixed/waveform/series 三种模式） | `src/core/WaveformGenerator.{h,cpp}`、`src/core/CMakeLists.txt` | 无 |
| T2 | 暴露 `SignalResolver::fillFromIcd` 为公共接口 `buildFromIcd(frameName, nodePath)` | `src/engine/SignalResolver.{h,cpp}` | 无 |
| T3 | `DevicePaletteWidget` 设备选择 UI 增强（遍历 `loadedPlugins` 按 `is_mock` 分组 + `[Mock]` 前缀）；确认 `TopologyJsonSerializer` 不处理 mock 字段，无需改动 | `src/topology/DevicePaletteWidget.cpp` | 无 |
| T4 | 修复 `onAddDeviceFromTemplate` 的 `.first()` 歧义 | `src/topology/TopologyEditorWidget.cpp` | 无 |

### 阶段 2：模式推断

| 任务 | 内容 | 改动文件 | 依赖 |
|---|---|---|---|
| T5 | `HardwareManager::loadFromTopology` 移除 `dObj["mock"]` 读取，改用 `pluginId` 反查 `is_mock` | `src/engine/HardwareManager.{h,cpp}` | 无 |
| T6 | `TestExecutionEngine::loadTopology` 的 `checkMockConsistency`/`hasMockDevices` 改用 `is_mock` | `src/engine/TestExecutionEngine.cpp` | T5 |
| T7 | `loadResponseConfigFile` 路径从 `/mock/MockResponses.json` 改为 `/MockResponses.emock` | `src/engine/TestExecutionEngine.cpp` | 无 |

### 阶段 3：AD 模拟器改造（依赖 T1；T12 因依赖 T10 划入阶段 4）

| 任务 | 内容 | 改动文件 | 依赖 |
|---|---|---|---|
| T8 | `ADChannelSimulator` 改造：移除硬编码正弦波，持有 `WaveformGenerator`，`readChannelValue` 调 `generate(sampleIndex, 1000)` | `src/engine/MockUUTBuilder.{h,cpp}` | T1 |
| T9 | `MockADPlugin::generateSample` 改为调 `WaveformGenerator`（归一化幅值），`injectChannelData` 改为设置 series 模式，generateSample 乘 `range` 缩放 | `src/plugins/mock/ad/MockADPlugin.{h,cpp}` | T1 |

### 阶段 4：MockUUTBuilder 扩展（依赖 T2）

| 任务 | 内容 | 改动文件 | 依赖 |
|---|---|---|---|
| T10 | 新增 response 配置结构（`replyFrameName` + `fieldValues[]`）到 `MockTypes.h`，改动 `MockUUTBuilder` 的 QJson 解析逻辑从读 `responseHex` 改为读 `fieldValues[]` | `src/engine/MockTypes.h`、`src/engine/MockUUTBuilder.{h,cpp}` | T2 |
| T11 | `buildSingleUUT` 逐字段调 `SignalCodec::encodeToFrame` 编码后按 `bitOffset` 合并为整帧字节 | `src/engine/MockUUTBuilder.cpp` | T2、T10 |
| T12 | `buildChannelSimulator` 解析 AD 端口 mode + 参数，配置 `WaveformGenerator`（DA 维持 fixedValue） | `src/engine/MockUUTBuilder.cpp` | T1、T8 |

### 阶段 5：MockConfigEditor（依赖阶段 4）

| 任务 | 内容 | 改动文件 | 依赖 |
|---|---|---|---|
| T13 | `MockConfigEditor` 骨架（实现 `IEditor` 接口，注册到 `EditorManager`） | `src/app/editors/MockConfigEditor.{h,cpp}`、`src/app/EditorManager.cpp`、`src/app/CMakeLists.txt` | T10 |
| T14 | 左侧导航树（UUT -> 端口 -> 响应，加载 `topology.etopo` + `MockResponses.emock`） | `src/app/editors/MockConfigEditor.cpp` | T13 |
| T15 | 帧响应编辑区（回复帧选择 + ICD 字段工程值编辑 + hex 预览，复用 `buildFromIcd` + `SignalCodec`） | `src/app/editors/MockConfigEditor.cpp` | T2、T14 |
| T16 | AD 端口编辑区（三种模式切换 + 波形预览图 + 序列表格编辑） | `src/app/editors/MockConfigEditor.cpp` | T14 |
| T17 | DA 端口编辑区（fixedValue 输入框） | `src/app/editors/MockConfigEditor.cpp` | T14 |
| T18 | 编辑器可见性与模式判断（真实模式隐藏入口，Mock/空/混合模式入口可见，复用 T5 的 is_mock 反查逻辑） | `src/app/editors/MockConfigEditor.cpp`、`src/app/EditorManager.cpp` | T5、T13 |

### 阶段 6：数据迁移 + 文档

| 任务 | 内容 | 改动文件 | 依赖 |
|---|---|---|---|
| T19 | `demo_mock` 项目数据迁移：以 `topology/mock/MockResponses.json` 为准迁移为 `topology/MockResponses.emock`（response 结构升级），删除根目录 `mock/MockResponses.json` 及空 `mock/` 目录，清理多余 `.etopo` | `temp/projects/demo_mock/` | T7、T10、T12 |
| T20 | 文档更新：`MockUUTSimulator设计.md` 全文 mock 字段/路径标注废弃，`ideas.md` 更新结论 | `docs/plan/MockUUTSimulator设计.md`、`ideas.md` | 全部 |

### 验证里程碑

| 里程碑 | 验证内容 | 前置任务 |
|---|---|---|
| M1 | 模式推断可用：打开 mock 项目，设备按 `is_mock` 实例化，混合模式拒绝运行并提示 | T5-T7 |
| M2 | AD 波形可用：CHECK AD 信号读到 waveform/series 模式生成的值 | T1、T8、T9、T12 |
| M3 | Mock 响应编码可用：运行测试程序，MockUUT 从 `fieldValues` 编码回复帧 | T10-T12 |
| M4 | MockConfigEditor 可用：编辑器打开、编辑响应/AD 端口、保存回写 `.emock` | T13-T18 |
| M5 | 全链路打通：编辑器配置 -> 保存 -> 运行时加载 -> MockUUT 响应正确 | 全部 |
