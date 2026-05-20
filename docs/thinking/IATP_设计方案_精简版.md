# IATP设计方案（精简版）

> **来源**：`IATP_设计方案.md`（V2.0, 2026-05）核心内容提炼，约原文档1/5篇幅，覆盖架构/接口/数据流/计划。

## 1 项目概述

**综合性自动化测试平台**，覆盖工业控制、航空航天、车载总线三大领域。通过硬件抽象层屏蔽设备差异、ICD信号层实现工程值与原始数据双向转换、脚本引擎驱动自动化测试流程。

### 核心痛点
硬件种类多SDK各异 → 测试依赖人工操作 → 数据散落难追溯 → 用例与硬件强耦合 → 无硬件时无法开发 → 用例格式不统一 → 报告手工整理

---

## 2 总体架构

```
┌──────────────────────────────────────┐
│           应用层 (UI)                  │
│ 设备管理器│帧编辑器│ICD编辑器│拓扑编辑器    │
│ 用例编辑器│监控面板│报告查看器            │
├──────────────────────────────────────┤
│           用例管理层 (TestCase)         │
│ 用例CRUD│格式转换(JSON/Excel/YML→Lua)   │
│ JSON Schema校验│版本管理                │
├──────────────────────────────────────┤
│           测试引擎层 (Engine)           │
│ Lua VM│执行控制(暂停/恢复/终止/断点/单步) │
│ 断言验证│数据记录│报告生成               │
│ API: SetDevice/VerifyDevice/InjectFault│
├──────────────────────────────────────┤
│           ICD信号层 (ICD)              │
│ 信号路由(UUID→设备+通道+协议)            │
│ 协议转换(pack/unpack)│故障注入│CVT缓存   │
├──────────────────────────────────────┤
│           硬件抽象层 (HAL)              │
│ AD/DA/DIO/Pulse/CAN/429/1553B/串口/VISA│
│ 虚拟设备(Simulate模式)                  │
├──────────────────────────────────────┤
│           数据持久化层                   │
└──────────────────────────────────────┘
```

| 层 | 职责 |
|---|---|
| 应用层 | 可视化操作界面 |
| 用例管理层 | 用例CRUD/多格式转换/校验/版本管理 |
| 测试引擎层 | Lua VM执行脚本/调试/数据记录/报告 |
| ICD信号层 | 工程值↔原始数据转换/信号路由/协议封解包/故障注入 |
| 硬件抽象层 | 插件化封装各类硬件/虚拟设备支持 |

### 关键架构决策（10项）

| # | 决策 | 原因 |
|---|---|---|
| AD1 | 5层→6层（新增用例管理层） | 用例CRUD不应塞进Engine |
| AD2 | DataPool融入ICD，仅离散事件型Pub/Sub | MVP信号量<100，CVT够用 |
| AD3 | TCP/UDP作为ICD传输通道，不在HAL层 | 不是硬件设备，放HAL导致插件膨胀 |
| AD4 | 新增IVisaPlugin和IPulsePlugin | VISA无通道概念，脉冲独立于DA/DIO |
| AD5 | 信号用UUID标识 | 改名不导致引用失效 |
| AD6 | 三编辑器共享SignalMapper数据 | 三种编辑方式操作同一份数据 |
| AD7 | JSON用例格式：10基本+3控制流 | JSON核心中间格式 |
| AD8 | WHILE必填timeout | 防死循环 |
| AD9 | MVP仅Lua引擎 | Lua Debug Library直接做调试 |
| AD10 | MVP同进程QPluginLoader | 简单低延迟，接口保持抽象远期可换 |

### 层间依赖
- 单向依赖：上层→下层，下层不反向依赖
- 接口参数只用基本类型/QVariant，不传C++指针
- MVP优先简单低成本，接口保持抽象

---

## 3 硬件抽象层（HAL）

### 插件接口体系

```
IPlugin (生命周期: init/start/stop/uninit)
  └── IDevicePlugin (open/close/selfTest/simulate + DeviceInfo/DeviceStatus)
        ├── IADevicePlugin  — 模拟量采集: 配置(采样率/量程/耦合/触发) + 采集控制 + 数据读取
        ├── IDADevicePlugin — 模拟量输出: writeChannel/readbackChannel
        ├── IDioPlugin      — 开关量IO: setDirection/readInput/setOutput
        ├── IPulsePlugin    — 脉冲信号: FreqOut/PulseOut/CountOut/QuadratureIn
        ├── ISerialPlugin   — 串口族(RS232/422/485): 波特率/数据位/校验/停止位 + asyncWrite/readData
        ├── ICanPlugin      — CAN/CAN FD: 比特率/ID过滤/帧收发(标准帧+扩展帧+CAN FD)
        ├── IArinc429Plugin — ARINC 429: 速率/Label收发/429Word结构(label/sdi/data/ssm/parity)
        ├── IMil1553Plugin  — MIL-STD-1553B: BC/RT/MT三模式
        └── IVisaPlugin     — VISA SCPI: openResource/sendCommand/query(*IDN?自检)
```

### 设备树结构
```
厂家 → 分类(AD/DA/DIO/Pulse/Serial/CAN/429/1553B/VISA) → 设备 → 通道
```
VISA类设备无通道概念。每个节点显示名称+状态(在线/离线/异常/模拟)+自检结果。

### 自检流程
`系统启动 → PluginManager.loadAll() → 遍历IDevicePlugin → openDevice() → selfTest() → 更新deviceStatus()`

### Dry Run模式
- `simulate(true)`：切换到虚拟数据源，Engine和ICD无感知
- 触发方式：全局开关或单设备开关（混合模式），开关在HAL插件级别

---

## 4 ICD信号层

### 内部结构

```
ICD Manager
├── SignalMapper          — 信号路由: UUID→deviceId+channelId+protocolId
│   ├── 转换规则(线性/多项式/枚举/脚本)
│   └── 传输通道选择(串口/TCP/UDP)
├── ProtocolRegistry      — 协议定义仓库: Protocol{字段列表+字节序+校验+pack/unpack}
├── FaultManager          — 故障注入: 7种类型
├── SignalValueCache      — CVT: 信号最新值缓存(写覆盖, 即时读)
└── DataPool              — 离散事件型Pub/Sub(融入ICD)
```

### Engine-ICD交互接口（IICDEngineInterface）

| 方法 | 功能 |
|---|---|
| `setSignal(signalId, value)` | 设置信号值（工程值→封包→发送） |
| `getSignal(signalId)` | 读取信号最新值（从CVT缓存） |
| `verifySignal(signalId, expected, tolerance)` | 验证信号值（读取+比较+允差） |
| `waitForSignal(signalId, op, value, timeoutMs)` | 等待信号满足条件（轮询+超时） |
| `injectFault(signalId, config)` | 注入故障 |
| `clearFault(signalId) / clearAllFaults()` | 清除故障 |

### 传输通道分层
- **串口** → HAL层ISerialPlugin（物理硬件）
- **TCP/UDP** → ICD层传输通道选项（通信方式）
- **应用层协议(MQTT/WebSocket)** → ICD协议转换内处理，底层走TCP

### 故障注入（7种）

| 类型 | 位置 | 说明 |
|---|---|---|
| 信号值死滞(Stuck-at) | ICD转换 | 值固定为某值 |
| 信号值加偏置/噪声 | ICD转换 | 叠加偏移量 |
| 协议CRC错误 | ICD封包 | 篡改CRC字段 |
| 校验位错误 | ICD封包 | 篡改校验位 |
| 通信延迟 | DataPool | 数据延迟N ms |
| 通信丢包 | DataPool | 按概率丢弃数据 |
| 采样率异常 | DataPool | 频率突变 |

用例结束自动清除所有活跃故障。

---

## 5 用例管理层

### JSON核心格式（v1.0）

**10种基本指令：**

| cmd | 必填 | 可选 | 说明 |
|---|---|---|---|
| SET | target, value | — | 设置信号值 |
| VERIFY | target, value | tolerance | 验证信号值 |
| WAIT | target, op, value | timeout(ms) | 等待信号满足条件 |
| DELAY | value | unit(ms/s) | 延时 |
| ACTION | — | desc | 暂停等用户确认 |
| PHOTO | — | — | 拍照 |
| RECORD | value(bool) | — | 开始/停止录像 |
| INJECT_FAULT | target, fault_type | fault_value | 注入故障 |
| CLEAR_FAULT | target | — | 清除故障 |
| LOG | desc | — | 输出日志 |

**3种控制流：**

| cmd | 必填 | 可选 | 说明 |
|---|---|---|---|
| LOOP | count, steps | — | 固定次数循环 |
| WHILE | condition, timeout, steps | interval(ms) | 条件循环（timeout必填防死循环） |
| IF | condition, then_steps | else_steps | 条件分支 |

### 条件表达式统一
```json
{ "target": "sig-uuid", "op": ">=", "value": 30.0 }
```
`op`: ==, !=, >, <, >=, <=。IF/WHILE/WAIT共用此格式。

### 嵌套约束
steps/then_steps/else_steps内不允许再出现LOOP/WHILE/IF。需要嵌套控制流必须写Lua脚本。

### 格式转换流程
```
Excel ──▶ JSON ◀── YML
            │
            ▼
       JSON Schema校验
            │
            ▼
       Lua脚本代码 ──▶ Engine.execute()
```

---

## 6 测试引擎层

### Lua API绑定（10个函数→对应10种指令）

| Lua API | 对应指令 | 内部调用 |
|---|---|---|
| `SetDevice("温度", 37.5)` | SET | setSignal() |
| `VerifyDevice("温度", 37.5, {min, max})` | VERIFY | verifySignal() |
| `WaitFor("温度", ">=", 30.0, 5000)` | WAIT | waitForSignal() |
| `Delay(1000)` | DELAY | msleep() |
| `UserAction("请观察指示灯")` | ACTION | 弹窗确认 |
| `TakePhoto()` | PHOTO | VISA插件 |
| `SetRecord(true)` | RECORD | VISA插件 |
| `InjectFault("温度", {type, value})` | INJECT_FAULT | injectFault() |
| `ClearFault("温度")` | CLEAR_FAULT | clearFault() |
| `Log("温度正常")` | LOG | 日志输出 |

### 执行控制

| 特性 | MVP优先级 | 实现方式 |
|---|---|---|
| 暂停/恢复 | 高 | 步骤完成后挂起 |
| 终止 | 高 | 立即停止 |
| 行断点 | 高 | lua_sethook()行级hook |
| 条件断点 | 中 | hook+条件判断 |
| 单步(Over/Into/Out) | 高 | hook控制 |
| 变量监视+调用栈 | 高 | debug.getlocal等 |
| 并行执行 | 低(远期) | — |

### 数据记录
自动记录：每步开始/结束时间、SET值、VERIFY期望值+实际值+判定、故障注入记录。

### 报告生成（MVP）
文本/HTML格式：用例基本信息 → 步骤执行结果表格 → 通过率统计 → 失败详情。

---

## 7 数据流

### 写路径（Engine→硬件）
```
Lua: SetDevice("温度", 37.5)
  → Engine.setSignal("sig-uuid", 37.5)
    → ICD查UUID映射(device+channel+protocol)
    → FaultManager检查(有故障则篡改)
    → ProtocolRegistry.pack(37.5→原始字节)
    → 传输通道(串口/TCP/UDP) → 硬件
```

### 读路径（硬件→Engine）
```
HAL收到原始字节 → Pub到DataPool
  → ICD Subscribe → unpack(原始字节→工程值)
    → SignalValueCache(CVT) → Engine.getSignal() 快速读取
    → Pub到UI监控面板 实时显示
```

### 故障注入位置
FaultManager作用于ICD转换过程内部：写路径篡改封包前的值，读路径修改解包后的值。Engine和HAL无需感知。

---

## 8 技术选型

| 项 | 方案 | 理由 |
|---|---|---|
| 开发框架 | Qt 5.12 MSVC2017_64, C++17 | 直接调C SDK、QPluginLoader、GraphicsView、跨平台 |
| 脚本引擎 | Lua 5.4 + sol2 | ~300KB、无GC、Lua Debug Library原生调试 |
| 构建系统 | CMake + Ninja | C++标准、增量编译快 |
| 插件机制 | QPluginLoader(同进程MVP) | 简单低延迟，接口保持抽象远期可换独立进程 |
| 序列化 | JSON (QJsonDocument) | Qt原生、可读、与Excel/YML互转 |
| 日志 | spdlog 1.17.0 | 异步高性能、多sink |
| 编辑器 | QScintilla 2.11.3 | 语法高亮、多语言Lexer |
| Excel | QXlsx 1.5.0 | 纯Qt无Office依赖 |
| PDF | libharu 2.4.6 | 轻量无外部依赖 |
| 测试 | Google Test 1.17.0 | CTest集成 |
| 压缩 | zlib 1.3.2 | 通用 |

---

## 9 开发计划（7阶段 · 共约20周）

| 阶段 | 内容 | 工期 | 状态 |
|---|---|---|---|
| **1** 基础框架搭建 | 构建环境/基础设施/插件框架/主窗口UI/编辑器/项目管理 | 3周 | ✅ 已完成 |
| **2** HAL接口+Mock | 全部10类设备插件接口定义+Mock实现+设备树UI+自检+Dry Run | 2.5周 | 待开发 |
| **3** ICD信号层 | SignalMapper/ProtocolRegistry/转换引擎/传输通道/故障注入/三编辑器UI | 4周 | 待开发 |
| **4** 用例管理层 | JSON格式定义/格式转换器(Lua/Excel/YML)/Schema校验/CRUD/编辑器UI | 2.5周 | 待开发 |
| **5** 测试引擎层 | Lua引擎集成/Lua API绑定/执行控制/调试器/报告生成 | 3周 | 待开发 |
| **6** 真实硬件对接 | 真实驱动实现(板卡/VISA/串口/CAN等)/发现枚举/模式切换/全链路联调 | 3周 | 待开发 |
| **7** 测试与优化 | 回归/集成/兼容性/压力测试+体验优化+打包文档 | 2周 | 待开发 |

### 里程碑

| | 里程碑 | 完成标志 |
|---|---|---|
| M1 | 框架就绪 ✅ | 主窗口可运行，插件可加载，编辑器可用 |
| M2 | HAL就绪 | 全部插件接口定义+Mock实现，设备树可展示 |
| M3 | 信号就绪 | ICD映射可用，协议转换正确，拓扑编辑器可连线 |
| M4 | 用例就绪 | JSON用例可编辑、校验、转换为Lua脚本 |
| M5 | 引擎就绪 | Lua脚本可执行、调试、生成报告 |
| M6 | 硬件就绪 | 真实驱动可用，Dry Run/Real模式可切换，全链路闭环通过 |
| M7 | 产品发布 | 闭环测试通过，安装包可交付 |

---

## 10 核心设计原则

1. **UUID标识**：信号/设备/协议全部UUID关联，改名不失效
2. **分层解耦**：单向依赖，接口参数序列化（基本类型+QVariant）
3. **JSON中间格式**：用例以JSON为核心，Excel/YML为输入源，Lua为临时产物
4. **Dry Run贯穿**：Mock插件 + Simulate模式，无硬件也可全链路开发调试
5. **故障注入透明**：FaultManager在ICD转换过程内操作，Engine/HAL无感知
6. **MVP务实**：同进程插件、仅Lua引擎、简单文本/HTML报告，接口保留远期扩展
