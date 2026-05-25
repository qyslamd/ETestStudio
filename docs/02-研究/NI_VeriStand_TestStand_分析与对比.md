# NI VeriStand & TestStand 拆解报告 — 与 IATP 的对比分析

> **目的**：拆解 NI VeriStand 和 TestStand 的核心模块、设计理念和开发流程，与 IATP 的六层架构逐层对比，验证 IATP 架构决策的正确性，发现可借鉴的设计模式。

---

## 目录

1. [NI VeriStand 拆解](#1-ni-veristand-拆解)
2. [NI TestStand 拆解](#2-ni-teststand-拆解)
3. [VeriStand vs TestStand — 定位差异](#3-veristand-vs-teststand--定位差异)
4. [IATP 与 NI 产品线逐层对比](#4-iatp-与-ni-产品线逐层对比)
5. [设计流程对比](#5-设计流程对比)
6. [核心思路差异](#6-核心思路差异)
7. [可借鉴的设计模式](#7-可借鉴的设计模式)

---

## 1. NI VeriStand 拆解

### 1.1 产品定位

VeriStand 是一个 **实时 HIL（Hardware-in-the-Loop）测试和仿真平台**，核心能力是：

- 将仿真模型（Simulink、VeriStand 自定义模型等）连接到真实 I/O 硬件
- 以 **定时循环** 驱动模型执行和 I/O 数据交换
- 提供运行时交互界面（Workspace）用于监控和手动控制
- **不擅长**：复杂的测试序列编排、条件分支、报告生成——这些是 TestStand 的领域

### 1.2 核心模块

```text
NI VeriStand 系统架构

┌─────────────────────────────────────────────────────────────┐
│                    Workspace (运行界面)                        │
│  控件面板: 数值/布尔/波形/仪表 | Alarm指示 | 数据回放        │
│  运行时编辑: 拖拽控件 → 绑定信号通道                           │
├─────────────────────────────────────────────────────────────┤
│                     Engine (实时引擎)                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │模型执行器│ │I/O调度器 │ │激励生成器│ │数据记录器│        │
│  │(Simulink │ │(定时循环 │ │Stimulus │ │TDMS日志 │        │
│  │ .dll)   │ │ 1-10kHz)│ │ Profile │ │         │        │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘        │
├─────────────────────────────────────────────────────────────┤
│                   System Explorer (配置环境)                   │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │硬件映射  │ │通道配置  │ │Calibration│ │Alarm    │        │
│  │PXI/CRio │ │Scaling   │ │XCP/CCP  │ │触发     │        │
│  │/第三方   │ │线性/查表 │ │标定     │ │配置     │        │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘        │
├─────────────────────────────────────────────────────────────┤
│                    I/O 硬件抽象层                               │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │ PXI 板卡 │ │C系列模块 │ │第三方驱动│ │协议接口  │        │
│  │ AI/AO/DI/│ │CAN/429/ │ │(反射内存 │ │XNET/CAN │        │
│  │ DO/计数器│ │1553     │ │  etc)   │ │/LIN/Flex│        │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘        │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 关键设计理念

**① 定时循环 (Timed Loop) 驱动一切**

VeriStand 的核心不是事件驱动，而是 **精确频率的定时循环**：

| 循环类型 | 频率 | 用途 |
|---------|------|------|
| Primary Control Loop (PCL) | 1-10 kHz | 模型执行 + I/O 同步 |
| Background Loop | 更慢 | 日志写入、非实时任务 |
| Stimulus Profile | 可配置 | 序列化激励输出 |

这意味着 VeriStand 天然适合 **连续采集/控制** 场景（如 HIL 仿真），但 **不适合以"步骤"为单位的顺序测试**。

**② 信号通道 (Channel) 是核心抽象**

VeriStand 的一切操作围绕 Channel：

```
硬件物理通道 ──Scaling──▶ 工程值 Channel ──绑定──▶ Workspace 控件
                            │
                            ├──绑定──▶ Stimulus Profile（激励）
                            ├──绑定──▶ Alarm（报警触发）
                            └──绑定──▶ Data Logging（数据记录）
```

Channel 是 **全局命名空间**（每个 Channel 有唯一名称），所有模块通过 Channel 名称引用信号。

**③ Stimulus Profile — 时序激励**

Stimulus Profile 是 VeriStand 的"测试序列"机制，定义某个 Channel 在 **时间轴** 上的值变化：

```
时间轴: t=0s ────── t=5s ────── t=10s ────── t=15s
        温度=25°C    温度=30°C    温度=35°C    温度=40°C
                    └── 斜坡 ──┘    跳变        └── 保持 ──┘
```

它是 **纯时序的**，没有条件分支、没有循环控制、没有决策逻辑。

**④ 支持的总线**

- NI PXI/CompactRIO 板卡（AI/AO/DI/DO/计数器）
- NI-XNET（CAN/CAN FD/LIN/FlexRay）
- ARINC 429（NI 板卡）
- 第三方设备（通过反射内存、Shared Memory、Custom Device SDK）

### 1.4 VeriStand 的局限（对 IATP 的借鉴意义）

| 局限 | 原因 | 对 IATP 的启示 |
|------|------|---------------|
| 不能做复杂测试序列编排 | 定位是 HIL 平台，不是 Test Executive | IATP 需要独立的用例管理层 |
| Stimulus Profile 无条件分支 | 纯时序模型 | IATP 必须支持 IF/LOOP/WHILE |
| 报告能力弱 | 需配合 TestStand | IATP 引擎层内置报告生成 |
| 硬件绑定 NI 生态 | 商业策略 | IATP 需要厂商无关的插件架构 |

---

## 2. NI TestStand 拆解

### 2.1 产品定位

TestStand 是一个 **测试执行框架（Test Executive）**，核心能力是：

- 开发和执行测试序列（Sequence）
- 管理测试流程（顺序/并行/批处理）
- 调用各种语言编写的测试代码（DLL/LabVIEW/.NET/Python/C++）
- 生成测试报告和记录数据库
- **不擅长**：实时硬件控制、连续采集、HIL 仿真——这些是 VeriStand 的领域

### 2.2 核心模块

```text
NI TestStand 系统架构

┌─────────────────────────────────────────────────────────────────┐
│                      Sequence Editor (IDE)                        │
│  序列编辑 (Step-based) | 表达式浏览器 | 变量管理                  │
│  断点/单步/监视 | 调用栈 | 序列文件 (.seq) 管理                  │
├─────────────────────────────────────────────────────────────────┤
│                          Engine (执行引擎)                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐            │
│  │序列执行器│ │表达式求值│ │结果处理  │ │流程控制  │            │
│  │(Step by  │ │(VBScript │ │Pass/Fail │ │Sequence  │            │
│  │ Step)   │ │类语法)   │ │/Error   │ │Call/Goto │            │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘            │
├─────────────────────────────────────────────────────────────────┤
│                     Process Model (流程模型)                       │
│  Sequential (默认): 逐个执行测试                                   │
│  Parallel: 多 UUT 并行测试                                         │
│  Batch: 批处理模式                                                 │
├─────────────────────────────────────────────────────────────────┤
│                    Module Adapter (语言适配器)                      │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐         │
│  │ DLL  │ │ActiveX│ │.NET  │ │Python│ │LabVI │ │ C/C++│         │
│  │Adapter│ │Adapter│ │Adapter│ │Adpt. │ │EW Ad.│ │ Ad.  │         │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘         │
├─────────────────────────────────────────────────────────────────┤
│                      Report & Logging (报告)                      │
│  HTML/XML/ATML/Text | 数据库日志 (ODBC) | Callback 自定义        │
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 关键设计理念

**① Step-Based 序列模型**

TestStand 的核心执行单元是 **Step**。每个 Step 是一个原子操作，包含：

```
Step 结构:
┌─────────────────────────────────────────┐
│  Step.Name          = "测量电压"        │
│  Step.Type          = NumericLimitTest  │  ← 步骤类型决定行为
│  Step.Module        = "meas_voltage.dll"│  ← 实际执行的代码
│  Step.PreConditions = [ ... ]           │  ← 前置条件（可跳过）
│  Step.PostActions   = [ ... ]           │  ← 后置动作（记录/跳转）
│  Step.FlowControl   = {Pass→Next,       │  ← 结果驱动流程
│                        Fail→GoTo StepX, │
│                        Error→Abort}     │
└─────────────────────────────────────────┘
```

关键特点：**每个 Step 的结果决定下一个 Step 的走向**。这恰好是 IATP 需要的——IATP 的 JSON 步骤也是顺序执行，但受限于扁平结构没有分支路由。

**② Process Model（流程模型）—— TestStand 的精髓**

Process Model 是 TestStand 最值得 IATP 借鉴的设计。它是一个 **模板化的测试执行流程**，定义了测试执行的"骨架"：

```text
Sequential Process Model 生命周期:

Entry → Initialize → 遍历 TestGroups → 遍历 Tests → Cleanup → Exit
                        │                    │
                        │  ┌──────────────────┘
                        │  ▼
                        │  Setup → Main(实际步骤) → Report Results → Loop
                        │
                        └──→ TestGroup 之间: 顺序执行
```

Process Model 的好处：
- 将"测试流程的结构"与"具体的测试内容"分离
- 用户只需要填写具体的 Test/Step，流程骨架不变
- 同一个序列文件，换一个 Process Model 就变成不同的执行模式（顺序/并行/批处理）
- 高级用户可以自定义 Process Model

**③ Module Adapter 机制**

TestStand 不绑定任何编程语言，通过 Module Adapter 调用外部代码：

| Adapter | 用途 | 调用方式 |
|---------|------|---------|
| DLL Adapter | C/C++/LabWindows/CVI | `LoadLibrary` + `GetProcAddress` |
| .NET Adapter | C#/VB.NET | CLR 加载 |
| LabVIEW Adapter | LabVIEW VI | VI Server 调用 |
| Python Adapter | Python 脚本 | Python C API |
| ActiveX Adapter | COM 组件 | COM 接口 |

**IATP 的 Lua API 本质上就是 TestStand 的 DLL Adapter**——只不过 TestStand 支持多种语言，而 IATP MVP 只支持 Lua。

**④ 变量体系**

TestStand 有 4 种变量作用域：

| 作用域 | 生命周期 | 用途 |
|--------|---------|------|
| Local | 单个 Step | 临时计算 |
| Sequence | 当前序列 | 跨 Step 传值 |
| File Global | 整个 .seq 文件 | 文件内共享 |
| Station Global | 整个测试站 | 跨序列/跨文件共享 |
| Built-in | 引擎预定义 | StationInfo, ThreadInfo 等 |

### 2.4 TestStand 的局限（对 IATP 的借鉴意义）

| 局限 | 原因 | 对 IATP 的启示 |
|------|------|---------------|
| 无内置 ICD/信号概念 | TestStand 只管执行不关心硬件 | IATP 的 ICD 层是差异化价值 |
| 序列编辑器的学习曲线陡 | 功能丰富，界面复杂 | IATP 用例编辑器应保持简洁 |
| 无实时硬件控制能力 | 非实时 Windows 应用 | IATP 引擎 + HAL 需要处理实时性 |
| 商业授权昂贵 | NI 商业策略 | IATP 开源/自主可控 |

---

## 3. VeriStand vs TestStand — 定位差异

两者是 **互补关系** 而非竞争关系：

```text
                   连续/实时控制能力
                        │
                  高    │    VeriStand
                        │    (HIL 仿真、实时 I/O)
                        │
                        │
              ──────────┼────────── 测试编排能力
                        │
                        │
                  低    │    TestStand
                        │    (测试序列、报告)
                        │
                        │    IATP ← 试图同时覆盖这两个维度
```

**NI 的组合方案**：VeriStand + TestStand 集成使用：

```
VeriStand 负责：硬件 I/O + 实时模型 + 连续采集
     │
     │ (通过 VeriStand .NET API 或 TCP/IP)
     ▼
TestStand 负责：测试序列编排 + 判断逻辑 + 报告生成
     │
     ▼
Operator 界面接收：最终 Pass/Fail 判定
```

**IATP 的定位差异**：IATP 试图在 **一个平台内** 同时覆盖这两个维度，通过分层架构（HAL→ICD→Engine→用例管理）实现硬件无关的测试序列编排。这既是优势（集成度高），也是风险（复杂度高）。

---

## 4. IATP 与 NI 产品线逐层对比

### 4.1 HAL 层 vs VeriStand I/O Abstraction

| 维度 | VeriStand | IATP HAL |
|------|-----------|----------|
| 硬件范围 | NI PXI/CRio 为主，第三方有限 | 厂商无关，按类型特化插件接口 |
| 插件机制 | Custom Device SDK（专有） | QPluginLoader（标准 C++） |
| 仿真模式 | 无独立 Simulate 模式 | 每个插件内置 `simulate(bool)` |
| 通道概念 | Channel 全局命名空间 | 设备树 + 通道号 |
| Scaling | 线性/查表（固化在通道配置） | ICD 层统一做，HAL 只透传原始值 |

**关键差异**：VeriStand 的 Scaling 是 **通道级别的属性**（配置在 HAL 层），而 IATP 将信号转换放在 ICD 层，HAL 只做原始字节收发。IATP 的设计更合理——Scaling 是协议/信号语义，不属于硬件抽象。

### 4.2 ICD 层 vs VeriStand Channel + Scaling

| 维度 | VeriStand | IATP ICD |
|------|-----------|----------|
| 信号标识 | Channel 名称字符串 | UUID，可改名不影响引用 |
| 工程值转换 | Scaling（线性/查表） | SignalMapper（线性/多项式/枚举/Lua 脚本） |
| 协议支持 | 无（只有 Scaling） | 完整 ProtocolRegistry + pack/unpack |
| 故障注入 | 不原生支持 | FaultManager 专用模块 |
| 值缓存 | 读硬件（实时循环） | CVT 缓存，Engine 同步读 |
| Pub/Sub | 无 | DataPool 融入 ICD |

**关键差异**：VeriStand 没有"协议"概念，只有"通道 + Scaling"。IATP 的 ICD 层多了 ProtocolRegistry（帧协议编辑器产生），可以处理如 A429 标签拆包、CAN 报文解析等复杂协议。

### 4.3 引擎层 vs VeriStand Engine + TestStand Engine

| 维度 | VeriStand Engine | TestStand Engine | IATP Engine |
|------|------------------|------------------|-------------|
| 执行模型 | 定时循环（1-10kHz） | Step-by-Step 序列 | Step-by-Step + 协程 |
| 实时性 | 实时（RTOS） | 非实时（Windows） | 非实时（MVP） |
| 脚本能力 | 无 | 表达式 + Module Adapter | Lua + sol2 |
| 调试 | 无 | 断点/单步/变量/调用栈 | 断点/单步/变量 (Lua Debug) |
| 暂停/恢复 | 无 | 有（Step 边界） | 有（协程 yield） |
| 数据记录 | TDMS 二进制 | 自定义 Callback | 内置 JSON/文本报告 |

**关键差异**：IATP 的 Engine 选择 **Lua 协程 + Step 执行器** 的混合模式，而非纯定时循环或纯 Step 序列。这意味着 IATP Engine 可以同时执行时序控制（定时循环采数）和流程控制（IF/WHILE 条件分支），这在单一平台的实现策略上比 NI 的两套引擎组合更紧凑。

### 4.4 用例管理层 vs TestStand Sequence

| 维度 | TestStand | IATP 用例管理 |
|------|-----------|---------------|
| 文件格式 | .seq（专有二进制/XML） | JSON（开放格式） |
| 控制流 | Goto/SequenceCall/Loop | LOOP/WHILE/IF（3 种指令） |
| 嵌套约束 | 无限制（可深度嵌套） | 禁止嵌套（复杂逻辑用 Lua） |
| 参数化 | Sequence Parameters | 无（MVP，远期支持） |
| Schema 校验 | 内置 | JSON Schema Draft-07 |
| 多格式 | 只有 .seq | JSON/Excel/YML 互转 |
| 版本管理 | 无 | 计划支持 |

**IATP 的差异化优势**：
- JSON 开放格式，可 diff、可 review、可在 CI 中做 Schema 校验
- Excel/YML 多源输入，降低非程序员的使用门槛
- 控制流嵌套限制是故意设计的——避免 JSON 的可读性崩塌

### 4.5 报告系统 vs TestStand Report

| 维度 | TestStand | IATP |
|------|-----------|------|
| 报告格式 | HTML/XML/ATML/Text | HTML/Text（MVP） |
| 报告编辑 | 不可编辑 | 支持编辑修改 |
| 数据库日志 | ODBC（MySQL/SQL Server/Oracle） | 无（MVP） |
| 报告定制 | Callback + 自定义模板 | 无（MVP） |
| 数据追溯 | 每步骤自动记录 | 每步骤自动记录 |

### 4.6 UI 层 vs VeriStand Workspace

| 维度 | VeriStand Workspace | IATP UI |
|------|---------------------|---------|
| 运行时监控 | 控件面板拖拽，可绑定 Channel | 计划评估 RUI Web 面板 |
| 运行时编辑 | 支持（添加/删除控件） | 无 |
| 多视图 | 1 个 Workspace 窗口 | 帧编辑器/拓扑/ICD/用例等多编辑器 |

---

## 5. 设计流程对比

### 5.1 NI 标准开发流程（VeriStand + TestStand 组合）

```text
Phase 1: System Explorer
  ├── 配置硬件资源（PXI 机箱/板卡/通道）
  ├── 配置 Scaling（工程值转换）
  ├── 配置 Alarm 触发条件
  ├── 配置数据记录参数
  └── 配置 Stimulus Profile（时序激励）

Phase 2: Model Integration
  ├── 编译 Simulink 模型 → .dll
  ├── 导入到 VeriStand（映射 I/O 端口）
  └── 配置 Primary Control Loop 频率

Phase 3: Workspace Layout
  ├── 拖拽控件（数值/布尔/波形/仪表）
  └── 绑定 Channel

Phase 4: TestStand Sequence
  ├── 编写步骤序列（调用 VeriStand API）
  ├── 配置 Pass/Fail 判断
  └── 配置报告输出

Phase 5: Execute
  ├── VeriStand Engine（实时循环）执行
  │   ├── 驱动硬件 I/O
  │   ├── 执行模型计算
  │   ├── 执行 Stimulus Profile
  │   └── 记录数据到 TDMS
  └── TestStand（非实时）驱动
      ├── 读取数据
      ├── 判定结果
      └── 生成报告
```

### 5.2 IATP 设计流程（对标的流程）

```text
Phase 1: 硬件设备
  ├── 加载设备插件（QPluginLoader）
  ├── 设备树展示硬件/通道/状态
  ├── 自检（online/offline/error）
  └── Simulate 模式（无硬件可开发）

Phase 2: 拓扑编辑
  ├── 拖拽 UUT/Device/Monitor
  ├── 端口连线
  └── 定义信号连接关系

Phase 3: ICD 信号映射
  ├── 定义信号（UUID + 显示名 + 工程值类型）
  ├── 配置 SignalMapper（UUID→设备+通道+协议）
  ├── 配置 Scaling 规则（线性/多项式/枚举/Lua）
  └── 帧协议编辑器（Protocol Registry）

Phase 4: 用例编辑
  ├── Excel/JSON/YML → 统一 JSON 格式
  ├── JSON Schema 校验
  └── 控制流（LOOP/WHILE/IF）

Phase 5: 执行与监控
  ├── JSON → Lua 脚本
  ├── Engine 执行（协程驱动）
  ├── 断点/单步/变量监视
  └── 报告生成
```

### 5.3 流程差异总结

| 阶段 | VeriStand+TestStand | IATP |
|------|---------------------|------|
| 硬件配置 | System Explorer（GUI 配置） | 插件加载 + 设备树（代码 + GUI） |
| 信号映射 | Channel Scaling（HAL 层） | SignalMapper（ICD 层 + UUID） |
| 协议定义 | 无原生支持（需 Custom Device） | 帧协议编辑器（ProtocolRegistry） |
| 测试序列 | TestStand Sequence（.seq） | JSON 用例（可转 Lua） |
| 执行 | 双引擎并行（V.S. + T.S.） | 单引擎（Engine 协程驱动） |
| 调试 | TestStand 断点 + VeriStand 监控 | 统一在 Engine 层（Lua Debug） |
| 报告 | TestStand 报告（可配置） | 引擎内置（JSON/Text/HTML） |

---

## 6. 核心思路差异

### 6.1 信号 vs 通道：两种世界观

**NI 的方式（通道中心）**：

```
所有数据流通过 Channel：
  ┌──────┐    ┌───────┐    ┌──────────┐
  │硬件 I/O│──▶│Channel│──▶│ Workspace│
  └──────┘    │       │    └──────────┘
              │       │──▶│ Stimulus │
              └───────┘    └──────────┘

Channel 是"点"——一个数值流经的位置。
```

**IATP 的方式（信号中心）**：

```
信号是"实体"——有 UUID、显示名、工程值类型、转换规则：
  ┌──────┐    ┌──────────┐    ┌──────────┐
  │HAL  │──▶│ICD 转换  │──▶│Signal    │──▶│ Engine  │
  │硬件 │    │(原始→工程)│    │(UUID)   │    │         │
  └──────┘    └──────────┘    └──────────┘    └──────────┘
                                  │
                                  ├──▶ 帧协议编辑器
                                  ├──▶ 拓扑编辑器连线
                                  └──▶ 用例中引用

信号是"实体"——UUID 贯穿所有模块，改名不产生连锁反应。
```

**差异本质**：NI 的 Channel 是 **基础设施视角**（信号只是流过通道的数据），IATP 的 Signal 是 **测试工程师视角**（信号是承载测试语义的实体）。UUID 信号标识是 IATP 相对于 NI 方案的核心差异化优势。

### 6.2 紧耦合 vs 松耦合

| 维度 | NI（VeriStand + TestStand） | IATP |
|------|----------------------------|------|
| 产品间集成 | 需要 TCP/IP 或 .NET API 桥接 | 同一进程，接口直接调用 |
| 数据格式 | TDMS（二进制，NI 专有）+ .seq（专有） | JSON（开放、可读、可 diff） |
| 语言绑定 | LabVIEW + 多 Adapter | Lua（MVP）+ 远期多语言 |
| 硬件绑定 | NI 硬件为主导 | 厂商无关，按接口加载 |

### 6.3 实时 vs 非实时

| 维度 | VeriStand | IATP |
|------|-----------|------|
| 操作系统 | Pharos RTOS / NI Linux RT | Windows |
| 循环频率 | 1-10 kHz（PCL） | N/A（MVP 不承诺硬实时） |
| 抖动控制 | 硬件定时器保证 | 无 |
| 适用场景 | HIL 仿真（需精确时序） | 功能测试（时序裕度大） |

**IATP 的现实选择**：MVP 阶段在 Windows 上做功能测试，不承诺硬实时。对于需要精确时序的场景（如 HIL），后续可通过独立进程转发到 RT 目标——这是接口设计时预留 IPC 能力的远见。

### 6.4 Process Model vs JSON 指令

| 维度 | TestStand Process Model | IATP JSON 指令 |
|------|------------------------|----------------|
| 灵活性 | 极高（任何执行模式都可模板化） | 有限（10+3 种指令） |
| 复杂度 | 高（学习曲线陡） | 低（50 行就能看懂） |
| 定制 | 需要 LabVIEW 或 C++ 修改 Process Model | 不支持（MVP） |
| 适用范围 | 任何测试场景 | 单线程顺序 + 简单控制流 |

**可借鉴**：IATP 可以考虑在阶段 4（用例管理层）引入类似 Process Model 的 **执行模式模板** 概念，但不用做得像 TestStand 那么复杂——只需区分"顺序执行"和"循环执行"两种模板即可。

---

## 7. 可借鉴的设计模式

### 7.1 TestStand 的 Step Result → Flow Control

TestStand 每个 Step 执行完后根据 Result 自动路由：

```
Pass  → Next Step
Fail  → 跳转到修复步骤 或 继续下一个
Error → 终止序列 或 调用错误处理序列
```

**IATP 现有方案**：JSON 步骤顺序执行，没有 Fail 跳转逻辑。
**借鉴建议**：在阶段 4（用例管理层）为每个 Step 增加 `onPass` / `onFail` / `onError` 可选字段，指向跳转的步骤序号或标签。

```json
{
  "cmd": "VERIFY",
  "target": "电压",
  "value": 5.0,
  "tolerance": 0.1,
  "onPass": null,
  "onFail": "修复电压",
  "onError": null
}
```

### 7.2 VeriStand 的 Timed Loop + Channel 模型

VeriStand 的 PCL 模型适合 **周期性采集 + 实时控制**。IATP 的 Engine 可以借鉴这个模式，在 Engine 中支持 **Background Timer**：

```
IATP Engine 内部:
┌──────────────────────────────┐
│  Main Thread                 │
│  ├── Lua 协程 (步骤执行)     │
│  └── UserAction 阻塞等待     │
├──────────────────────────────┤
│  Monitor Thread              │
│  ├── 定时轮询 ICD CVT        │
│  └── 触发 Alarm/Pub 事件     │
└──────────────────────────────┘
```

这样 Engine 可以一边执行步骤序列，一边后台周期性采集监控信号。

### 7.3 TestStand 的 Callback 机制

TestStand 在执行过程的关键节点插入 Callback：

| Callback | 触发时机 | 用途 |
|----------|---------|------|
| PreUUT | 测试每个 UUT 前 | 初始化 |
| PostUUT | 测试每个 UUT 后 | 清理/汇总 |
| PreStep | 每个 Step 前 | 条件判断 |
| PostStep | 每个 Step 后 | 数据记录 |
| ProcessSetup | 序列流程启动 | 全局初始化 |
| ProcessCleanup | 序列流程结束 | 全局清理 |

**借鉴建议**：IATP 的 JSON 格式可以增加 `callbacks` 段，在序列级别定义类似的生命周期钩子：

```json
{
  "name": "温度测试序列",
  "callbacks": {
    "onSetup": [{"cmd": "LOG", "desc": "测试开始"}],
    "onCleanup": [{"cmd": "LOG", "desc": "测试结束"}]
  },
  "steps": [...]
}
```

### 7.4 VeriStand 的 Alarm 机制

VeriStand 的 Alarm 是 **通道级别的条件触发器**：当通道值超过阈值时触发报警，可以联动停止激励、记录数据、通知用户。这是一种 **独立于测试序列的监控机制**。

**借鉴建议**：IATP 可以在 ICD 层或 Engine 层增加独立的 Alarm 检查——与用例执行并行，当信号超限时自动记录并通知。Alarm 配置可以作为 ICD 信号定义的一部分：

```json
{
  "signalId": "uuid-温度",
  "alarms": [
    {"condition": "> 100", "severity": "CRITICAL", "action": "STOP"},
    {"condition": "< -20", "severity": "WARNING", "action": "LOG"}
  ]
}
```

### 7.5 TestStand 的 Station Global 变量

TestStand 的 Station Global 变量在测试站内跨所有序列共享，适合存储仪器句柄、硬件资源状态等全局信息。

**借鉴建议**：IATP 可以在 Engine 中提供类似的 **全局变量槽**，在跨用例场景中传递共享状态。

---

## 8. 结论

### 8.1 IATP 架构决策验证

| IATP 架构决策 | NI 方案对照 | 结论 |
|-------------|------------|------|
| 六层架构（独立用例管理层） | TestStand 也是独立 Test Executive | ✅ 验证正确 |
| UUID 信号标识 | NI 用 Channel 名称字符串 | ✅ IATP 更优 |
| ICD 层处理协议转换 | VeriStand 无协议概念 | ✅ IATP 差异化优势 |
| JSON 开放格式 | NI 用专有格式 | ✅ IATP 更优 |
| Lua 单语言 MVP | TestStand 多语言 Adapter | ✅ MVP 合理，远期扩展 |
| 同进程 QPluginLoader | NI 同进程 Custom Device SDK | ✅ 合理，远期 IPC 预留 |

### 8.2 NI 方案优于 IATP 的点

1. **Process Model**：TestStand 的 Process Model 设计精良，IATP 阶段 4 应借鉴
2. **Step Result 路由**：TestStand 的 Pass/Fail/Error 跳转机制，IATP 需要引入
3. **并行测试**：TestStand 的并行 Process Model 成熟可靠（IATP 远期需要）
4. **回拨机制**：PreUUT/PostUUT/PreStep/PostStep 生命周期钩子

### 8.3 IATP 差异化价值

1. **UUID 信号标识** — NI 做不到的"改名不破引用"
2. **ICD SignalMapper 多规则映射** — NI 只有线性/查表
3. **帧协议编辑器 + ProtocolRegistry** — NI 无协议可视化编辑
4. **JSON 开放格式** — CI 可集成、可 diff、版本可控
5. **Lua 协程调试** — 用 300KB 的嵌入语言实现完整调试能力
6. **硬件厂商无关** — 只要实现 IDevicePlugin 接口就能接入

---

## 附：术语对照

| NI 术语 | IATP 对应 | 说明 |
|---------|----------|------|
| Channel | Signal (UUID) | NI 通道是"位置"，IATP 信号是"实体" |
| Scaling | SignalMapper 规则 | 工程值转换，但 IATP 支持更多种类 |
| Stimulus Profile | SET 步骤序列 | 时序激励，但 IATP 多了控制流 |
| Process Model | 用例模板 | TestStand 专利，IATP 远期可参考 |
| Step | JSON 指令 | 执行原子单元 |
| Module Adapter | Lua API | 语言适配层 |
| Custom Device | IDevicePlugin | 硬件插件 |
| Workspace | 监控面板 | 运行时 UI |
| Alarm | 信号级报警 | 独立于用例的监控触发 |
| PCL | Engine Monitor Thread | 定时循环 |
| TDMS | JSON/Text 日志 | NI 专有二进制 vs IATP 开放文本 |
| Seq File | JSON 用例文件 | .seq 专有 vs JSON 开放 |
| Station Global | Engine 全局变量槽 | 跨用例共享状态 |

---

*本文档基于 NI VeriStand 2024 和 TestStand 2024 公开资料分析，与 IATP 设计方案 v2.0 对照编写。*
