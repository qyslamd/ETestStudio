# 综合性自动化测试平台（IATP）设计方案

**版本**: V1.0
**日期**: 2026年5月
**作者**: 研发团队

---

## 目录

1. [项目概述](#1-项目概述)
2. [需求分析](#2-需求分析)
3. [总体架构设计](#3-总体架构设计)
4. [技术选型与论证](#4-技术选型与论证)
5. [各层详细设计](#5-各层详细设计)
6. [数据流设计](#6-数据流设计)
7. [关键技术方案](#7-关键技术方案)
8. [可视化编辑器设计](#8-可视化编辑器设计)
9. [远期展望](#9-远期展望)
10. [开发计划](#10-开发计划)
11. [验证方案](#11-验证方案)

---

## 1 项目概述

### 1.1 建设目标

IATP（Integrated Automated Test Platform，综合性自动化测试平台）旨在打造一套**全硬件兼容、高可靠、易扩展**的统一自动化测试平台，覆盖工业控制、航空航天、车载总线三大领域。平台通过硬件抽象层屏蔽设备差异，通过ICD信号层实现工程值与硬件原始数据的双向转换，通过脚本引擎驱动自动化测试流程，使测试人员能够专注于测试业务逻辑而非硬件操作细节。

### 1.2 项目背景与行业痛点

在工业控制自动化、航空航天、车载总线等测控领域，测试工作面临以下核心痛点：

**痛点1：硬件种类多、厂商SDK各异，缺乏统一管理手段**

测控场景涉及的硬件类型繁多——AD/DA采集卡、IO板卡、串口设备、CAN总线、ARINC 429、MIL-STD-1553B、VISA程控仪器等——每种硬件对应不同的厂商SDK、不同的API风格、不同的配置方式。测试人员需要掌握多套工具，学习成本高；不同硬件之间无法统一调度，测试流程碎片化。

**痛点2：测试流程依赖人工操作，效率低、易出错**

当前大量测试工作仍依赖人工操作：手动设置参数、手动读取数据、人工判断结果。这种模式效率低下，且容易出现人为疏漏——漏测关键步骤、误读数据、误判结果等问题难以避免。

**痛点3：测试数据散落各处，缺乏可追溯性和一致性保障**

测试数据分散在各台设备、各份Excel表格中，无法统一管理。测试结果缺乏标准化记录，无法追溯"某次测试中某一步骤的输入值和实际输出值分别是什么"，出现问题后难以回溯定位。

**痛点4：测试用例与硬件强耦合，换硬件就要重写用例**

传统方式下，测试用例直接操作硬件通道号和寄存器地址。一旦更换硬件型号或调整通道配置，所有相关用例都需要修改。测试逻辑无法在不同硬件配置间复用，维护成本极高。

**痛点5：缺乏仿真/Dry Run能力，无硬件时无法开发调试**

开发和调试测试用例时，通常需要连接真实硬件。但在早期开发阶段或现场无设备的情况下，无法进行用例验证和逻辑调试，严重制约开发效率。

**痛点6：测试用例编写维护困难，格式不统一、复用性差**

不同项目、不同团队使用不同的用例编写方式——Excel表格、Word文档、脚本代码、甚至口头描述——缺乏统一的格式标准。用例之间无法互相引用、无法参数化复用，维护和交接成本高。

**痛点7：测试报告依赖手工整理，数据不可追溯**

测试完成后，报告通常由人工汇总整理，耗时且容易遗漏关键信息。测试步骤的执行细节、中间数据、失败原因等信息缺乏自动化记录，无法生成标准化、可追溯的测试报告。

### 1.3 平台定位

IATP定位为面向工业控制自动化、航空航天、车载总线三大领域的综合性自动化测试平台，核心能力包括：

| 领域 | 典型硬件 | 典型测试场景 |
|------|---------|-------------|
| 工业控制自动化 | AD/DA采集卡、IO板卡、脉冲信号卡、串口设备 | 传感器校准、执行器控制、系统联调测试 |
| 航空航天 | ARINC 429、MIL-STD-1553B、VISA程控仪器 | 航电系统总线测试、程控仪器自动化验证 |
| 车载总线 | CAN/CAN FD、LIN、FlexRay | ECU通信测试、诊断协议验证、网关路由测试 |

平台通过统一的插件化架构和ICD信号抽象，使同一套测试用例能够在不同硬件配置上运行，实现"一次编写、多环境执行"。

---

## 2 需求分析

### 2.1 硬件支持范围

| 硬件类别 | 具体支持范围 | 接入方式 |
|---------|-------------|---------|
| 模拟量采集（AD） | 低速采集卡（支持多通道、多量程、多触发模式） | 厂商SDK |
| 模拟量输出（DA） | 低速输出卡（含任意波输出） | 厂商SDK |
| 开关量IO（DIO） | 数字量输入/输出板卡 | 厂商SDK |
| 脉冲信号 | 频率输出/脉冲输出/计数输出/正交编码输入 | 厂商SDK |
| 串行总线 | RS232、RS422、RS485 | 厂商SDK |
| 航空总线 | ARINC 429、MIL-STD-1553B | 厂商SDK |
| 车载总线 | CAN、CAN FD | 厂商SDK |
| 网络通信 | TCP、UDP | Qt网络模块 |
| 程控仪器 | VISA SCPI标准仪器（电源、示波器、万用表、摄像头） | VISA库 |

硬件通过PCI/cPCI/PXI/PCIe/USB/LAN/串口等接口接入工控计算机。

### 2.2 测试流程需求

一个完整的自动化测试流程如下：

```
硬件接入与自检 → ICD信号映射配置 → 帧协议定义 → 测试用例编制 → 测试执行与监控 → 报告生成
```

1. **硬件接入与自检**：系统加载所有硬件设备插件，执行自检验证设备工作正常，在设备树上展示硬件列表及状态
2. **ICD信号映射配置**：定义"工程值信号"与"硬件通道/协议字段"之间的映射关系
3. **帧协议定义**：定义通信协议中数据帧的字段结构（起始位、长度、字节序、编码方式等）
4. **测试用例编制**：以JSON/Excel/YML格式编写测试步骤，或直接编写Lua脚本
5. **测试执行与监控**：脚本引擎驱动执行，实时监控信号值和执行状态，支持断点调试
6. **报告生成**：自动记录全流程数据，生成测试报告

### 2.3 功能需求清单

| 编号 | 功能需求 | 说明 |
|------|---------|------|
| FR1 | 设备管理与自检 | 插件化加载硬件设备、设备树展示、自检流程、Dry Run仿真模式 |
| FR2 | ICD信号映射 | 信号定义、工程值与原始值双向转换、UUID标识、转换规则（线性/多项式/枚举/脚本） |
| FR3 | 协议管理 | 帧协议可视化编辑、协议pack/unpack、协议定义仓库 |
| FR4 | 拓扑编辑 | 硬件通道与被测设备接口的可视化连线、拓扑关系定义 |
| FR5 | 用例管理 | JSON/Excel/YML多格式支持、CRUD操作、JSON Schema校验、格式转换、版本管理 |
| FR6 | 测试执行 | Lua脚本引擎、暂停/恢复/终止、断点/单步/变量监视、Lua API绑定 |
| FR7 | 故障注入 | 信号值篡改、CRC校验错误、通信延迟/丢包、故障清除与自动恢复 |
| FR8 | 监控与报告 | 实时信号监控面板、步骤执行记录、数据记录、报告生成（文本/HTML） |

### 2.4 非功能需求

| 需求类别 | 说明 |
|---------|------|
| 稳定性 | 单个硬件插件异常不应导致整个系统崩溃；测试执行过程支持断点恢复 |
| 可扩展性 | 新增硬件类型仅需开发对应插件，不修改核心代码；脚本API可扩展 |
| 实时性 | 满足1-10ms级信号采集控制需求；信号值缓存支持快速读取最新值 |
| 可维护性 | 层间松耦合，模块可独立开发和测试；接口参数序列化，支持远期Web迁移 |
| 可移植性 | 基于Qt跨平台框架，支持Windows/Linux/QNX/VxWorks |

---

## 3 总体架构设计

### 3.1 六层架构总览

IATP采用六层松耦合架构，自上而下依次为：

```
┌──────────────────────────────────────────────────────────┐
│                     应用层 (UI)                           │
│  设备管理器 | 帧编辑器 | ICD编辑器 | 拓扑编辑器             │
│  用例编辑器 | 监控面板 | 报告查看器                         │
├──────────────────────────────────────────────────────────┤
│                  用例管理层 (TestCase)                      │
│  用例CRUD | 格式转换(JSON/Excel/YML→Lua)                   │
│  JSON Schema校验 | 版本管理                                │
├──────────────────────────────────────────────────────────┤
│                  测试引擎层 (Engine)                        │
│  Lua虚拟机 | 执行控制(暂停/恢复/终止)                       │
│  断点/单步/变量监视(Lua Debug Library)                     │
│  断言验证 | 数据记录 | 报告生成                             │
│  API: SetDevice / VerifyDevice / InjectFault              │
├──────────────────────────────────────────────────────────┤
│                 ICD信号层 (ICD)                            │
│  信号路由(UUID标识, 工程值↔通道映射)                        │
│  协议转换(pack/unpack, 多传输通道)                         │
│  故障注入(FaultManager) | SignalValueCache(CVT)           │
│  DataPool功能(Pub/Sub, 离散事件型)                         │
├──────────────────────────────────────────────────────────┤
│                硬件抽象层 (HAL)                             │
│  设备插件(按类型特化):                                     │
│    AD/DA/DIO/Pulse/CAN/1553B/A429/串口/VISA               │
│  虚拟设备(Simulate模式) | 厂商SDK封装                      │
└──────────────────────────────────────────────────────────┘
```

各层职责：

| 层级 | 职责 |
|------|------|
| 应用层 | 提供可视化操作界面，包括设备管理、协议编辑、用例编辑、测试监控等 |
| 用例管理层 | 负责测试用例的CRUD、多格式转换（JSON/Excel/YML→Lua）、校验与版本管理 |
| 测试引擎层 | 嵌入Lua虚拟机执行测试脚本，提供执行控制（暂停/恢复/终止/断点/单步）和数据记录 |
| ICD信号层 | 实现工程值与硬件原始数据的双向转换、信号路由、协议封包解包、故障注入、信号值缓存 |
| 硬件抽象层 | 通过插件机制封装各类硬件设备，提供统一的设备管理和虚拟设备支持 |

### 3.2 与凯云ETest对比

IATP的设计参考了凯云ETest测试系统，但在架构和实现上存在以下关键差异：

| 对比维度 | 凯云ETest | IATP |
|---------|----------|------|
| 架构层次 | 五层架构 | 六层架构（新增用例管理层，将用例CRUD/格式转换与引擎执行解耦） |
| 信号标识 | 信号名称 | UUID标识，显示名可随意修改，改名不导致引用失效 |
| 传输通道 | TCP/UDP在HAL层作为设备插件 | TCP/UDP作为ICD传输通道选项，与串口并列选择 |
| 数据池 | 独立DataPool层 | 融入ICD层，MVP阶段仅支持离散事件型Pub/Sub，减少层级复杂度 |
| 用例格式 | 表格编辑为主 | JSON核心中间格式 + Excel/YML多源输入，支持10基本指令+3控制流指令 |
| 脚本引擎 | 多语言支持 | MVP仅Lua（sol2 + Lua Debug Library），降低初期开发成本 |
| 插件隔离 | 未明确 | MVP同进程QPluginLoader，远期评估独立进程隔离 |
| 源码 | 商业闭源 | 自主开发，完全可控 |
| 扩展性 | 固定功能集 | 插件化架构，新增硬件类型仅需开发对应插件 |

IATP的核心差异化价值：

1. **UUID信号标识**：解决改名导致引用失效的痛点，帧编辑器/ICD编辑器/拓扑编辑器/测试用例全部基于UUID关联
2. **用例管理层独立**：用例的CRUD、格式转换、校验不与引擎执行耦合，Engine只负责执行脚本
3. **传输通道归属合理**：TCP/UDP不是硬件设备而是通信方式，归属ICD层作为传输通道选项，避免HAL层插件膨胀
4. **JSON核心中间格式**：以JSON为中间格式统一定义用例结构，Excel/YML作为输入源，Lua脚本作为中间产物

### 3.3 层间依赖原则

1. **单向依赖**：上层调用下层接口，下层不反向依赖上层。Engine调用ICD接口，ICD不感知Engine的存在
2. **接口参数序列化**：接口参数只使用基本类型（int/double/QString等）和QVariant，不传递C++对象指针。这为远期Web迁移保留了序列化能力——如果将来Engine和UI之间需要通过JSON/gRPC通信，接口无需修改
3. **MVP渐进策略**：每个架构决策都区分MVP阶段和远期方向。MVP优先选择简单、低成本的方案，接口层面保持抽象，确保后续升级不影响上层

### 3.4 关键架构决策

| 编号 | 决策 | 原因 |
|------|------|------|
| AD1 | 五层架构→六层架构（新增用例管理层） | 用例CRUD/校验/格式转换不应塞进Engine，Engine只负责执行 |
| AD2 | DataPool融入ICD，仅离散事件型Pub/Sub | MVP阶段信号量<100，ICD内SignalValueCache已够用，独立DataPool增加复杂度 |
| AD3 | TCP/UDP作为ICD传输通道选项，不在HAL层 | TCP/UDP不是硬件设备，是通信协议/传输方式，放HAL导致插件膨胀 |
| AD4 | 新增IVisaPlugin和IPulsePlugin | VISA设备在设备树上分类显示（无通道），脉冲是独立于DA/DIO的信号类型 |
| AD5 | 信号用UUID标识 | 改名不导致引用失效，帧编辑器/ICD/拓扑/用例全部基于UUID关联 |
| AD6 | 帧编辑器/ICD编辑器/拓扑编辑器共享SignalMapper数据 | 三种编辑方式操作同一份数据，可视化元数据与映射数据分开存储 |
| AD7 | JSON用例格式：10基本指令+3控制流指令 | JSON核心中间格式，LOOP/WHILE/IF支持简单控制流，嵌套控制流用Lua脚本 |
| AD8 | WHILE必填timeout防止死循环 | 条件循环必须有超时保护，interval防止CPU空转 |
| AD9 | MVP仅Lua脚本引擎 | 直接用Lua Debug Library实现调试，不做多语言抽象 |
| AD10 | MVP同进程插件隔离(QPluginLoader) | 简单、延迟低，后续评估独立进程隔离 |

---

## 4 技术选型与论证

### 4.1 开发框架：Qt 5.12.12 / C++

**选型**：Qt 5.12.12 (msvc2017_64)，C++17标准，实际使用MSVC2019编译环境（完全兼容MSVC2017）

**论证要点**：

| 对比维度 | Qt/C++ | Electron/Node.js | 纯C++/Win32 |
|---------|--------|-----------------|-------------|
| 跨平台 | Windows/Linux/QNX/VxWorks | Windows/Linux/macOS | 仅Windows |
| 插件机制 | QPluginLoader原生支持 | 需自建插件框架 | 需自建插件框架 |
| 图形编辑 | GraphicsView框架 | Canvas/SVG | 需自绘或用第三方库 |
| 硬件交互 | 直接调用厂商C SDK | 需FFI/N-API桥接，性能损失 | 直接调用 |
| 实时性 | C++原生性能，满足1-10ms级需求 | V8 GC停顿，不适合实时控制 | C++原生性能 |
| 工业生态 | Qt在工控/医疗/汽车领域广泛使用 | Web应用为主 | 遗留系统为主 |
| 开发效率 | 信号槽机制、MVC框架 | 前端生态丰富 | 纯手工 |

**选择Qt/C++的核心原因**：
- 测控平台需要直接调用厂商提供的C/C++ SDK（如板卡驱动、VISA库），Qt/C++是最自然的绑定方式，无需FFI桥接
- QPluginLoader提供成熟的动态库加载和元数据解析机制，是HAL插件化的天然基础
- GraphicsView框架为拓扑编辑器的图形化连线提供了现成的场景-视图架构
- 工控/航空航天领域对实时性有要求，C++原生性能优于Electron的V8引擎
- Qt在工业自动化、汽车电子、医疗器械领域有大量成熟应用案例

### 4.2 脚本引擎：Lua 5.4 + sol2

**选型**：Lua 5.4.4，sol2作为C++绑定层，Lua Debug Library提供调试能力

**论证要点**：

| 对比维度 | Lua 5.4 + sol2 | Python 3 | JavaScript (V8/QJSEngine) |
|---------|---------------|----------|--------------------------|
| 嵌入体积 | ~300KB，极轻量 | ~30MB，需Python运行时 | ~1MB(QJSEngine) / ~50MB(V8) |
| 启动速度 | 毫秒级 | 秒级 | 毫秒级(QJSEngine) / 秒级(V8) |
| 实时性 | 无GC停顿，可预测 | GIL限制，GC不可控 | GC停顿不可控 |
| 调试能力 | Lua Debug Library原生支持断点/单步/变量监视 | pdb，需额外嵌入 | 需自建调试协议 |
| C++绑定 | sol2，类型安全，零开销 | pybind11，需处理引用计数 | QJSEngine/QV4Engine API |
| 学习门槛 | 语法简洁，2天可上手 | 广泛认知 | 广泛认知 |

**选择Lua的核心原因**：
- 嵌入式体积极小（~300KB），不增加发布包体积，适合工控设备的资源约束
- 无GC停顿，脚本执行时间可预测，满足实时控制需求
- Lua Debug Library原生提供钩子（hook）机制，可拦截每行执行实现断点、单步、变量监视，无需自建调试协议
- sol2是C++/Lua绑定的业界标准库，类型安全、零开销抽象，API设计简洁
- Lua在游戏（World of Warcraft/Nginx/Redis嵌入）和嵌入式领域有成熟的嵌入使用经验

**MVP阶段仅支持Lua**，远期通过IScriptEngine抽象接口支持Python/JS，接口层面不暴露脚本引擎细节。

### 4.3 构建系统：CMake + Ninja

**选型**：CMake 3.20+，Ninja生成器

**论证要点**：
- CMake是C++项目的事实标准构建系统，与Qt生态深度集成（Qt 5.12官方支持CMake）
- Ninja比Make/MSBuild编译速度快2-3倍，增量编译效率高
- 支持现代CMake（target-based），依赖管理清晰
- CMakePresets.json统一开发环境配置，团队协作一致性好

### 4.4 插件机制：QPluginLoader

**选型**：Qt QPluginLoader动态加载，MVP阶段同进程运行

**论证要点**：

| 方案 | 优点 | 缺点 |
|------|------|------|
| QPluginLoader（同进程） | 简单、低延迟、开发成本低 | 单个插件崩溃可能影响整个进程 |
| QProcess（独立进程） | 插件隔离，崩溃不影响主进程 | IPC通信延迟、开发复杂度高、数据序列化开销 |

MVP阶段选择QPluginLoader，理由：
- 开发初期硬件插件数量少、稳定性风险可控
- 同进程调用延迟最低，满足实时性要求
- 接口层面保持抽象（IDevicePlugin），不暴露IPC细节，后续切换到独立进程不影响上层

### 4.5 其他技术选型

| 选型项 | 方案 | 选用理由 |
|--------|------|---------|
| 数据序列化 | JSON（QJsonDocument） | Qt原生支持、可读性好、与Excel/YML互转方便 |
| 日志 | spdlog 1.17.0 | 异步高性能、多sink（控制台/文件/QtUI）、运行时级别切换 |
| 编辑器组件 | QScintilla 2.11.3 | 语法高亮、代码折叠、多语言Lexer、类IDE编辑体验 |
| Excel读写 | QXlsx 1.5.0 | 纯Qt实现、无Office依赖、支持用例Excel模板读写 |
| PDF生成 | libharu 2.4.6 | 轻量级C库、无外部依赖、用于报告生成 |
| 单元测试 | Google Test 1.17.0 | 业界标准、与CMake集成良好、支持DISABLED_前缀崩溃测试 |
| 压缩 | zlib 1.3.2 | 通用压缩库、QXlsx/libpng依赖 |

---

## 5 各层详细设计

### 5.1 HAL硬件抽象层

#### 5.1.1 插件接口体系

HAL层采用两级插件接口设计：通用基类 + 类型特化派生类。

**通用基类 IPlugin**

所有插件的基础接口，定义生命周期和元数据：

```cpp
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // 生命周期
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void uninitialize() = 0;

    // 元数据
    virtual PluginMetaData metaData() const = 0;

    // 状态查询
    virtual bool isRunning() const = 0;
};
```

**设备插件基类 IDevicePlugin**

在IPlugin基础上增加设备管理能力：

```cpp
enum class DeviceStatus { Offline, Online, Error, Simulated };

struct DeviceInfo {
    int channel_count = 0;      // 通道数
    int resolution = 0;         // 分辨率（位数）
    QString model;              // 设备型号
    QString manufacturer;       // 厂家
    int bus_number = 0;         // PXI总线号
    int slot_number = 0;        // 设备号/槽位号
    int card_serial = 0;        // 板卡序列号
};

class IDevicePlugin : public IPlugin {
public:
    ~IDevicePlugin() override = default;

    // 设备生命周期
    virtual bool openDevice() = 0;
    virtual void closeDevice() = 0;

    // 设备信息与状态
    virtual DeviceInfo deviceInfo() const = 0;
    virtual DeviceStatus deviceStatus() const = 0;

    // 通用能力（V1.0新增）
    virtual bool selfTest() = 0;
    virtual bool simulate(bool enable) = 0;
    virtual QVariantMap configMetaData() const = 0;
};
```

**IADevicePlugin — 模拟量采集（v3.0）**

AD采集是最复杂的设备类型，接口覆盖耦合方式、触发模式、读取模式、扫描表等：

```cpp
enum class ADCoupling { DC, AC, ICP_DC, ICP_AC, GND_DC, GND_AC };
enum class ADTriggerMode { Software, ExternalPos, ExternalNeg, SystemPos, SystemNeg, StarPos, StarNeg, Internal };
enum class ADTriggerEdge { Rising, Falling };
enum class ADSampleStatus { Idle, Waiting, Sampling, Completed };
enum class ADReadMode { Direct, DMA, MAP, FIFO };
enum class ADMemoryMode { ChannelStorage, ScanStorage };

struct ADChannelConfig {
    double range = 10.0;                    // 量程(V)，如±10V → range=10.0
    ADCoupling coupling = ADCoupling::DC;   // 耦合方式
    double icp_current = 0.004;             // ICP电流值(A)，默认4mA
    bool differential = false;              // true=差分输入，false=单端输入
    int gain = 1;                           // 可编程增益(1/2/20/200等)
    int filter = 0;                         // 抗混叠滤波器档位，0=不滤波
    ADTriggerEdge trigger_edge = ADTriggerEdge::Rising;
    double trigger_level = 0.0;             // 触发电平(V)
};

struct ADTriggerConfig {
    ADTriggerMode mode = ADTriggerMode::Software;
    bool enabled = true;
    int pretrigger_length = 0;              // 预触发采样点数
    int trigger_length = 1024;              // 触发后采样点数
};

class IADevicePlugin : public IDevicePlugin {
public:
    // 采样率
    virtual bool setSampleRate(double rate) = 0;
    virtual double sampleRate() const = 0;

    // 存储深度
    virtual bool setSampleLength(int length) = 0;
    virtual int sampleLength() const = 0;

    // 通道配置
    virtual bool setChannelConfig(int channel, const ADChannelConfig& config) = 0;
    virtual ADChannelConfig channelConfig(int channel) const = 0;

    // 触发配置
    virtual bool setTriggerConfig(const ADTriggerConfig& config) = 0;
    virtual ADTriggerConfig triggerConfig() const = 0;
    virtual bool softwareTrigger() = 0;

    // 采集控制
    virtual bool startAcquisition() = 0;
    virtual void stopAcquisition() = 0;
    virtual bool isAcquiring() const = 0;
    virtual ADSampleStatus sampleStatus() const = 0;

    // 数据传输模式
    virtual bool setReadMode(ADReadMode mode) = 0;
    virtual ADReadMode readMode() const = 0;

    // 存储模式
    virtual bool setMemoryMode(ADMemoryMode mode) = 0;
    virtual ADMemoryMode memoryMode() const = 0;

    // 扫描表
    virtual bool setScanList(const QVector<int>& scanList) = 0;
    virtual QVector<int> scanList() const = 0;
    virtual int maxScanDepth() const = 0;

    // 单点读取（CVT模式）
    virtual double readChannel(int channel) = 0;
    virtual QVector<double> readAllChannels() = 0;

    // 批量读取
    virtual QVector<double> readChannelData(int channel, int count) = 0;
    virtual QVector<double> readAllChannelsData(int count) = 0;

    // 原始AD码读取
    virtual QVector<qint16> readChannelRaw(int channel, int count) = 0;
    virtual QVector<qint16> readAllChannelsRaw(int count) = 0;
};
```

**IDADevicePlugin — 模拟量输出**

```cpp
class IDADevicePlugin : public IDevicePlugin {
public:
    // 设置输出电压/电流
    virtual bool writeChannel(int channel, double value) = 0;
    // 回读当前输出值
    virtual double readbackChannel(int channel) const = 0;
};
```

**IDioPlugin — 开关量IO**

```cpp
enum class DioDirection { Input, Output };

class IDioPlugin : public IDevicePlugin {
public:
    // 通道方向配置
    virtual bool setDirection(int channel, DioDirection dir) = 0;
    virtual DioDirection direction(int channel) const = 0;

    // 读取输入
    virtual bool readInput(int channel) = 0;
    virtual QVector<bool> readAllInputs() = 0;

    // 设置输出
    virtual bool setOutput(int channel, bool value) = 0;
    virtual bool setAllOutputs(const QVector<bool>& values) = 0;
};
```

**IPulsePlugin — 脉冲信号**

```cpp
enum class PulseMode { FreqOut, PulseOut, CountOut, QuadratureIn };

struct PulseConfig {
    PulseMode mode = PulseMode::FreqOut;
    double frequency = 1000.0;         // 频率(Hz)
    double duty_cycle = 0.5;           // 占空比
    int pulse_count = 0;               // 脉冲数(0=连续)
};

class IPulsePlugin : public IDevicePlugin {
public:
    // 模式与参数配置
    virtual bool setPulseConfig(int channel, const PulseConfig& config) = 0;
    virtual PulseConfig pulseConfig(int channel) const = 0;

    // 输出控制
    virtual bool startOutput(int channel) = 0;
    virtual bool stopOutput(int channel) = 0;

    // 读取
    virtual double readFrequency(int channel) = 0;
    virtual qint64 readCount(int channel) = 0;
};
```

**ISerialPlugin — 串口族（RS232/422/485）**

```cpp
enum class SerialParity { None, Even, Odd };
enum class SerialStopBits { One, OneHalf, Two };
enum class SerialMode { RS232, RS422, RS485 };

class ISerialPlugin : public IDevicePlugin {
public:
    // 串口模式（232/422/485）
    virtual bool setSerialMode(SerialMode mode) = 0;
    virtual SerialMode serialMode() const = 0;

    // 通信参数配置
    virtual bool setBaudRate(int baudRate) = 0;
    virtual int baudRate() const = 0;
    virtual bool setDataBits(int bits) = 0;
    virtual int dataBits() const = 0;
    virtual bool setParity(SerialParity parity) = 0;
    virtual SerialParity parity() const = 0;
    virtual bool setStopBits(SerialStopBits bits) = 0;
    virtual SerialStopBits stopBits() const = 0;

    // 数据收发
    virtual qint64 asyncWrite(const QByteArray& data) = 0;
    virtual QByteArray readData(int maxBytes = -1) = 0;

    // 端口配置
    virtual bool setPortName(const QString& name) = 0;
    virtual QString portName() const = 0;
};
```

**ICanPlugin — CAN/CAN FD**

```cpp
enum class CanMode { Normal, ListenOnly, Loopback };
enum class CanFrameType { Standard, Extended };

struct CanFrame {
    quint32 id;
    QByteArray payload;
    CanFrameType frameType = CanFrameType::Standard;
    bool fd_format = false;            // CAN FD标志
    bool bit_rate_switch = false;      // CAN FD速率切换
};

class ICanPlugin : public IDevicePlugin {
public:
    // 总线配置
    virtual bool setBitrate(int bitrate) = 0;
    virtual int bitrate() const = 0;
    virtual bool setCanMode(CanMode mode) = 0;
    virtual CanMode canMode() const = 0;

    // ID过滤
    virtual bool setIdFilter(quint32 startId, quint32 endId, bool accept = true) = 0;
    virtual void clearIdFilters() = 0;

    // 帧收发
    virtual bool sendFrame(const CanFrame& frame) = 0;
    virtual CanFrame receiveFrame(int timeoutMs = 0) = 0;

    // 信号：异步帧接收通知
    // Q_SIGNAL void frameReceived(const CanFrame& frame);
};
```

**IArinc429Plugin — ARINC 429**

```cpp
enum class Arinc429Speed { Low, High };   // 低速12.5Kbps / 高速100Kbps

struct Arinc429Word {
    int label;                   // 8位Label
    int sdi = 0;                 // 2位SDI
    quint32 data = 0;            // 18位数据
    int ssm = 0;                 // 2位SSM
    int parity = 0;              // 1位奇偶校验
};

class IArinc429Plugin : public IDevicePlugin {
public:
    // 速率配置
    virtual bool setSpeed(Arinc429Speed speed) = 0;
    virtual Arinc429Speed speed() const = 0;

    // Label收发
    virtual bool sendLabel(const Arinc429Word& word) = 0;
    virtual Arinc429Word receiveLabel(int label, int timeoutMs = 0) = 0;

    // 信号：异步Label接收通知
    // Q_SIGNAL void labelReceived(const Arinc429Word& word);
};
```

**IMil1553Plugin — MIL-STD-1553B**

```cpp
enum class Mil1553Mode { BC, RT, MT };   // 总线控制器/远程终端/总线监视器

struct Mil1553Message {
    quint16 command_word;        // 命令字
    QByteArray data;             // 数据字(每字16bit)
    quint16 status_word = 0;     // 状态字(RT响应)
};

class IMil1553Plugin : public IDevicePlugin {
public:
    // 模式配置
    virtual bool setMode(Mil1553Mode mode) = 0;
    virtual Mil1553Mode mode() const = 0;

    // BC模式
    virtual bool bcSchedule(const QVector<Mil1553Message>& messages) = 0;
    virtual bool bcSendMessage(int rtAddress, int subAddress,
                               const QByteArray& data) = 0;

    // RT模式
    virtual bool rtSetResponse(int subAddress, const QByteArray& data) = 0;
    virtual QByteArray rtReadReceived(int subAddress) = 0;

    // MT模式
    virtual bool mtStartMonitor() = 0;
    virtual void mtStopMonitor() = 0;
    virtual QVector<Mil1553Message> mtReadMessages() = 0;
};
```

**IVisaPlugin — VISA SCPI程控仪器**

```cpp
class IVisaPlugin : public IDevicePlugin {
public:
    // 资源管理
    virtual bool openResource(const QString& visaAddress) = 0;
    virtual void closeResource() = 0;
    virtual QString resourceAddress() const = 0;

    // SCPI命令
    virtual bool sendCommand(const QString& command) = 0;
    virtual QString query(const QString& command) = 0;

    // 自检（发送*IDN?验证设备身份）
    bool selfTest() override;
};
```

VISA类设备在设备树上无通道概念，以命令接口为主要交互方式。

#### 5.1.2 设备树结构

设备管理器以树形结构展示所有已加载的硬件设备：

```
厂家A
├── AD采集              (板卡类，有通道)
│   └── EPH5022
│       ├── CH0
│       └── CH1
├── CAN通信             (板卡类，有通道)
│   └── CPC-Card
│       ├── CAN0
│       └── CAN1
├── 脉冲信号            (板卡类，有通道)
│   └── Pulse-Card
│       ├── CH0
│       └── CH1
└── 程控仪器            (VISA类，无通道，命令接口)
    ├── Agilent电源
    └── Tektronix示波器
```

树形层级：**厂家 → 分类 → 设备 → 通道**（VISA类无通道层级）

每个节点显示：设备名称、连接状态（在线/离线/异常/模拟）、自检结果。

#### 5.1.3 设备自检流程

```
系统启动 → PluginManager.loadAll() → 遍历所有IDevicePlugin
    → openDevice() → selfTest() → 更新deviceStatus()
        → 成功: 标记Online，设备树绿色图标
        → 失败: 标记Error，设备树红色图标，日志记录错误详情
```

VISA设备自检特殊处理：发送`*IDN?`命令，验证返回的身份信息与预期一致。

#### 5.1.4 Dry Run（simulate模式）

每个设备插件提供`simulate(bool enable)`方法：

- `simulate(true)`：切换到虚拟数据源。AD插件返回模拟的电压值，串口插件回环数据，CAN插件返回预定义的报文
- `simulate(false)`：切换回真实硬件
- 触发方式：全局开关（所有设备进入Dry Run）或单设备开关（混合模式）
- 开关位置在HAL层插件级别，不向上层透传——Engine和ICD无感知

### 5.2 ICD信号层

#### 5.2.1 内部结构

ICD层由5个子模块组成：

```
ICD Manager
├── SignalMapper              // 信号路由: UUID → deviceId + channelId + protocolId
│   ├── 转换规则(线性/多项式/枚举/脚本)
│   └── 传输通道选择(串口/TCP/UDP)
├── ProtocolRegistry          // 协议定义仓库
│   └── Protocol{字段列表 + 字节序 + 校验 + pack/unpack}
│       └── 来源: 帧协议编辑器(可视化编辑前端)
├── FaultManager              // 故障注入管理
│   └── 活跃故障列表 {signalId, type, params, active}
├── SignalValueCache          // CVT: 信号最新值缓存
└── DataPool                  // 离散事件型Pub/Sub(融入ICD，非独立层)
```

**SignalMapper — 信号路由核心**

SignalMapper是ICD层的核心模块，负责将抽象的信号UUID映射到具体的硬件通道和协议定义：

- 输入：信号UUID → 输出：deviceId + channelId + protocolId
- 转换规则：支持线性（y=kx+b）、多项式、枚举映射、Lua脚本四种方式
- 传输通道选择：同一信号可选择通过串口、TCP或UDP发送

**ProtocolRegistry — 协议定义仓库**

存储所有帧协议定义，每个Protocol对象包含：
- 字段列表（字段名、起始位、位长、字节序、编码方式）
- 校验规则（CRC、校验和等）
- pack()/unpack()方法（工程值↔原始字节双向转换）
- 帧协议编辑器是ProtocolRegistry的可视化编辑前端

**FaultManager — 故障注入管理**

维护活跃故障列表，每个故障项包含：signalId、故障类型、故障参数、是否活跃。故障注入作用在ICD转换过程中——修改信号值、篡改协议字段、延迟/丢包数据。

**SignalValueCache (CVT) — 信号最新值缓存**

缓存每个信号的最新值，供Engine快速读取。CVT（Current Value Table）模式：只保留最新值，不保留历史。写入时覆盖旧值，读取时立即返回当前值。

**DataPool — 离散事件型Pub/Sub**

融入ICD而非独立层，MVP阶段仅支持离散事件型发布订阅：
- HAL层将原始数据发布到DataPool
- ICD内部订阅原始数据，经过协议解包后发布SignalValue
- UI监控面板订阅SignalValue通道，实时显示

#### 5.2.2 信号数据结构

**原始字节元数据**

```cpp
struct RawData {
    QString deviceId;       // 设备唯一ID
    QString channelId;      // 通道ID
    QByteArray data;        // 原始字节
    qint64 timestamp;       // 采集/收发时间(微秒)
    QString protocolId;     // 关联协议ID
};
```

**SignalValue — 信号工程值**

```cpp
struct SignalValue {
    QString signalId;       // 信号UUID
    QVariant value;         // 工程值
    qint64 timestamp;       // 微秒级时间戳
    bool isValid;           // 有效性
    int quality;            // 数据质量(0=正常, 1=可疑, 2=无效, 3=模拟)
    QString sourceInfo;     // 来源描述
};
```

#### 5.2.3 传输通道分层

```
ICD协议转换(pack/unpack)
       │
       ▼
传输通道选择
  ├── 串口 (RS232/422/485) ──▶ HAL串口插件 → 物理硬件
  ├── TCP                     ──▶ Qt网络模块 → LAN
  └── UDP                     ──▶ Qt网络模块 → LAN

应用层协议(MQTT/WebSocket/HTTP等) ──▶ ICD协议转换内处理 ──▶ TCP传输通道
```

分层原则：
- **串口保留在HAL层**：串口对应物理硬件板卡，由ISerialPlugin管理
- **TCP/UDP作为ICD的传输通道选项**：TCP/UDP不是硬件设备，是通信协议/传输方式，在ICD层与串口并列选择
- **应用层协议走ICD协议转换**：MQTT/WebSocket/HTTP等应用层协议在ICD的pack/unpack内处理，底层使用TCP传输通道

#### 5.2.4 Engine-ICD交互接口

Engine层通过IICDEngineInterface与ICD层交互，不直接操作硬件：

```cpp
class IICDEngineInterface {
public:
    virtual ~IICDEngineInterface() = default;

    // 设置信号值（工程值→协议封包→传输通道→硬件）
    virtual bool setSignal(const QString& signalId, const QVariant& value,
                           QString* err = nullptr) = 0;

    // 读取信号最新值（从CVT缓存快速读取）
    virtual SignalValue getSignal(const QString& signalId,
                                  bool* ok = nullptr) = 0;

    // 验证信号值（读取+比较，支持允差）
    virtual bool verifySignal(const QString& signalId, const QVariant& expected,
                              const Tolerance& tolerance, QString* err = nullptr) = 0;

    // 等待信号满足条件（轮询+超时）
    virtual bool waitForSignal(const QString& signalId, const QString& op,
                               const QVariant& value, int timeoutMs,
                               QString* err = nullptr) = 0;

    // 注入故障
    virtual bool injectFault(const QString& signalId, const FaultConfig& fault,
                             QString* err = nullptr) = 0;

    // 清除故障
    virtual void clearFault(const QString& signalId) = 0;
    virtual void clearAllFaults() = 0;
};
```

所有接口参数只使用基本类型和QVariant，不传递C++对象指针。

### 5.3 用例管理层

#### 5.3.1 JSON核心格式（v1.0）

测试用例以JSON作为核心中间格式，支持10种基本指令和3种控制流指令：

**10种基本指令**

| cmd | 必填字段 | 可选字段 | 说明 |
|-----|---------|---------|------|
| SET | target, value | | 设置信号值 |
| VERIFY | target, value | tolerance | 验证信号值 |
| WAIT | target, op, value | timeout(ms) | 等待信号满足条件 |
| DELAY | value | unit(ms/s) | 延时等待 |
| ACTION | | desc | 暂停等待用户确认 |
| PHOTO | | | 拍照 |
| RECORD | value(bool) | | 开始/停止录像 |
| INJECT_FAULT | target, fault_type | fault_value | 注入故障 |
| CLEAR_FAULT | target | | 清除故障(target="*"清除全部) |
| LOG | desc | | 输出日志 |

**3种控制流指令**

| cmd | 必填字段 | 可选字段 | 说明 |
|-----|---------|---------|------|
| LOOP | count, steps | | 固定次数循环 |
| WHILE | condition, timeout, steps | interval(ms) | 条件循环 |
| IF | condition, then_steps | else_steps | 条件分支 |

#### 5.3.2 条件表达式统一结构

IF、WHILE、WAIT共用同一套条件格式：

```json
{ "target": "温度", "op": ">=", "value": 30.0 }
```

`op` 支持：`==`、`!=`、`>`、`<`、`>=`、`<=`

`target` 字段引用信号的UUID，UI显示时翻译为信号显示名。

#### 5.3.3 控制流指令

**LOOP — 固定次数循环**

```json
{
  "cmd": "LOOP",
  "count": 5,
  "steps": [
    { "cmd": "SET", "target": "温度", "value": 37.5 },
    { "cmd": "DELAY", "value": 1000 }
  ]
}
```

转Lua：
```lua
for i = 1, 5 do
    SetDevice("温度", 37.5)
    Delay(1000)
end
```

**WHILE — 条件循环**

```json
{
  "cmd": "WHILE",
  "condition": { "target": "温度", "op": "<", "value": 30.0 },
  "interval": 1000,
  "timeout": 30000,
  "steps": [
    { "cmd": "SET", "target": "加热器", "value": 1 },
    { "cmd": "DELAY", "value": 500 }
  ]
}
```

转Lua：
```lua
local _start = os.clock()
while GetDevice("温度") < 30.0 do
    if (os.clock() - _start) * 1000 >= 30000 then
        error("WHILE超时: 温度 < 30.0")
    end
    SetDevice("加热器", 1)
    Delay(500)
    Delay(1000)
end
```

- `interval`：每次循环开始前等待的间隔(ms)，防止CPU空转，为硬件留响应时间
- `timeout`：**必填**，最大循环时长(ms)，超时报错中断，防止死循环

**IF — 条件分支**

```json
{
  "cmd": "IF",
  "condition": { "target": "温度", "op": ">=", "value": 30.0 },
  "then_steps": [
    { "cmd": "LOG", "desc": "温度达标" }
  ],
  "else_steps": [
    { "cmd": "SET", "target": "加热器", "value": 1 }
  ]
}
```

`else_steps`可选，省略则只有then没有else。

#### 5.3.4 嵌套约束

steps/then_steps/else_steps内部**不允许再出现LOOP/WHILE/IF**。需要嵌套控制流时，用户必须编写Lua脚本。原因：

- JSON格式保持扁平，可读性不失控
- 用例编辑器UI不需要递归嵌套的树形编辑
- JSON Schema校验简单

#### 5.3.5 格式转换

```
Excel ──▶ JSON ◀── YML
            │
            ▼
       JSON Schema 校验
            │
            ▼
       Lua 脚本代码 ──▶ Engine.execute()
```

- JSON是核心中间格式，所有格式的用例最终转换为JSON
- Lua脚本代码是**中间产物**，运行时转换后交给Engine
- Engine只接收脚本代码执行，不感知用例的原始格式

**Excel模板**

列映射：步骤序号 | cmd | target | value | unit | condition | tolerance_min | tolerance_max | timeout | fault_type | fault_value | desc

每个用例占一个Sheet，Sheet名=用例名称。用例集信息放在首行。

**YML格式**

与JSON结构完全对应，天然可互换。YML更适合人工编写，JSON更适合程序处理。

#### 5.3.6 JSON Schema校验

使用JSON Schema Draft-07对用例文件进行语法校验，校验内容包括：
- 必填字段完整性
- 字段类型正确性
- cmd枚举值合法性
- 嵌套约束检查（控制流指令内部不能再嵌套控制流）

### 5.4 测试引擎层

#### 5.4.1 Lua引擎集成

采用sol2作为C++/Lua绑定层，将Lua 5.4虚拟机嵌入到C++应用中：

- 每个测试用例在独立的Lua VM中执行，用例之间状态隔离
- C++端封装ICD操作供脚本调用，脚本端通过全局函数访问
- 使用Lua Debug Library的hook机制实现调试能力

#### 5.4.2 Lua API绑定

Engine层为Lua脚本提供以下10个API函数，对应JSON用例的10种基本指令：

```lua
SetDevice("温度", 37.5)                          -- SET
VerifyDevice("温度", 37.5, {min=-0.1, max=0.1})  -- VERIFY
WaitFor("温度", ">=", 30.0, 5000)                -- WAIT, 超时5s
Delay(1000)                                       -- DELAY
UserAction("请观察指示灯是否亮起")                  -- ACTION
TakePhoto()                                       -- PHOTO
SetRecord(true)                                   -- RECORD
InjectFault("温度", {type="stuck_at", value=999}) -- INJECT_FAULT
ClearFault("温度")                                -- CLEAR_FAULT
Log("当前温度正常")                                -- LOG
```

每个Lua API内部调用IICDEngineInterface的对应方法：

| Lua API | IICDEngineInterface方法 |
|---------|------------------------|
| SetDevice(signalId, value) | setSignal() |
| VerifyDevice(signalId, expected, tolerance) | verifySignal() |
| WaitFor(signalId, op, value, timeout) | waitForSignal() |
| Delay(ms) | QThread::msleep() |
| UserAction(desc) | 弹窗等待用户确认 |
| TakePhoto() | VISA插件摄像头控制 |
| SetRecord(enable) | VISA插件录像控制 |
| InjectFault(signalId, config) | injectFault() |
| ClearFault(signalId) | clearFault() |
| Log(msg) | 日志输出 |

#### 5.4.3 执行控制

| 特性 | 说明 | MVP优先级 |
|------|------|----------|
| 暂停/恢复 | 当前步骤执行完后挂起，用户恢复后继续 | 高 |
| 终止 | 立即停止，标记用例为"已终止" | 高 |
| 行断点 | 停在脚本某一行 | 高 |
| 条件断点 | 信号值满足条件时停 | 中 |
| 单步执行 | Step Over / Step Into / Step Out | 高 |
| 变量监视 | 实时查看脚本变量值和ICD信号值 | 高 |
| 调用栈 | 当前执行位置 | 中 |
| 并行执行 | 多用例同时执行 | 低（远期） |

调试能力基于Lua Debug Library实现：通过`lua_sethook()`注册行级hook，在每行执行前检查断点和暂停标志，实现断点/单步/变量监视。

#### 5.4.4 数据记录与报告生成

**数据记录**

测试执行过程中自动记录：
- 每个步骤的开始/结束时间
- SET指令的设置值
- VERIFY指令的期望值和实际值、判定结果
- 故障注入/清除记录
- 步骤通过/失败/跳过状态

**报告生成**

MVP阶段支持简单文本/HTML格式报告，包含：
- 用例基本信息（名称、描述、前置条件）
- 各步骤执行结果表格
- 总体通过率统计
- 失败步骤详情

### 5.5 应用层（UI）

#### 5.5.1 主窗口布局

主窗口基于Qt Advanced Docking System (QADS)实现停靠式界面布局，分为6个区域：

```
┌──────┬──────────────────────────────────┬──────────┐
│      │                                  │          │
│ 活动 │        中央编辑区                  │  属性    │
│ 栏   │  (帧编辑器/ICD编辑器/用例编辑器/    │  面板    │
│      │   拓扑编辑器/代码编辑器)           │          │
│      │                                  │          │
├──────┴──────────────────────────────────┴──────────┤
│              底部面板（输出/问题/终端）                │
├────────────────────────────────────────────────────┤
│                     状态栏                           │
└────────────────────────────────────────────────────┘
```

- **活动栏**：7个SVG图标按钮（资源管理器/搜索/Git/调试/扩展/硬件/设置）
- **侧边栏**：StackedPage容器，活动栏切换显示内容
- **中央编辑区**：多标签编辑器，支持QScintilla代码编辑和可视化编辑器
- **底部面板**：输出面板（日志）、问题面板、终端面板
- **属性面板**：当前选中对象的属性编辑
- **状态栏**：执行状态、连接状态等信息

#### 5.5.2 核心功能模块

| 模块 | 对应架构层 | 说明 |
|------|-----------|------|
| 设备管理器 | HAL | 设备树视图，展示厂家→分类→设备→通道，显示状态和自检结果 |
| 帧协议编辑器 | ICD | ProtocolRegistry的可视化编辑前端，定义帧字段结构 |
| ICD编辑器 | ICD | SignalMapper的表单/表格编辑方式，定义信号映射关系 |
| 拓扑编辑器 | ICD | SignalMapper的图形化连线编辑，基于GraphicsView |
| 用例编辑器 | 用例管理层 | 可视化编辑测试用例，支持JSON/Excel/YML格式 |
| 监控面板 | Engine/ICD | 实时信号值监控、步骤执行进度、变量监视 |
| 报告查看器 | Engine | 查看测试执行报告 |

#### 5.5.3 三编辑器共享数据模型

帧协议编辑器、ICD编辑器、拓扑编辑器操作**同一份数据**（SignalMapper + ProtocolRegistry），只是交互方式不同：

| 编辑器 | 操作的数据 | 交互方式 |
|--------|-----------|---------|
| 帧协议编辑器 | ProtocolRegistry | 表格编辑帧字段结构 |
| ICD编辑器 | SignalMapper | 表单/表格编辑信号映射 |
| 拓扑编辑器 | SignalMapper | 图形化拖拽连线 |

拓扑编辑器的可视化元数据（节点位置、连线样式、节点颜色等）与映射数据分开存储，修改节点位置不影响信号映射关系。

---

## 6 数据流设计

### 6.1 写路径（Engine → 硬件）

测试脚本设置信号值时的数据流：

```
Lua: SetDevice("温度", 37.5)
         │
         ▼
Engine ──ICD.setSignal(signalId, 37.5)──▶ ICD
                                              │
                                              ├── 查UUID映射 → deviceId + channelId + protocolId
                                              ├── 查故障注入 → FaultManager(无故障则放行)
                                              ├── 协议封包 → pack(工程值→原始字节)
                                              └── 选择传输通道 → 串口/TCP/UDP
                                                      │
                                                      ▼
                                                  HAL.Send(原始字节) → 硬件
```

步骤说明：
1. Lua脚本调用`SetDevice("温度", 37.5)`
2. Engine将调用转发给ICD的`setSignal(signalId, 37.5)`
3. ICD通过SignalMapper查找signalId对应的deviceId、channelId、protocolId
4. FaultManager检查该信号是否有活跃故障，有故障则按故障规则修改值
5. ProtocolRegistry根据protocolId执行`pack()`，将工程值37.5转换为原始字节
6. 根据传输通道选择，通过串口（HAL插件）或TCP/UDP（Qt网络模块）发送原始字节

### 6.2 读路径（硬件 → Engine）

硬件数据上报时的数据流：

```
HAL(收硬件数据) ──Pub──▶ ICD内部DataPool(原始字节 + 元数据)
                              │
                              ▼
                        ICD.Subscribe() → 协议解包unpack → SignalValue
                              │
                              ├──▶ SignalValueCache(CVT) ──▶ Engine.getSignal() 快速读取最新值
                              │
                              └──▶ SignalValue通道.Publish ──▶ UI监控订阅
```

步骤说明：
1. HAL层设备插件接收到硬件数据，发布到ICD内部的DataPool
2. ICD的订阅处理器取出原始数据，通过ProtocolRegistry执行`unpack()`，将原始字节转换为工程值
3. 转换后的SignalValue写入SignalValueCache（CVT），Engine调用`getSignal()`可立即读取最新值
4. 同时通过DataPool的Pub/Sub机制发布SignalValue，UI监控面板订阅后实时更新显示

### 6.3 故障注入对数据流的影响

```
Engine ──ICD.injectFault(signalId, FaultConfig)──▶ ICD内部FaultManager
                                                       │
                                                       ▼
                                               影响ICD转换过程:
                                               - 写路径: 信号值被篡改后封包
                                               - 读路径: 解包后的值被修改
                                               - 延迟/丢包: 影响DataPool数据传输
```

故障注入作用在ICD转换过程内部，Engine和HAL层无需感知故障的存在。

---

## 7 关键技术方案

### 7.1 UUID信号标识机制

**问题**：如果信号用名称标识，修改显示名会导致所有引用该信号的映射关系、测试用例、拓扑连线全部失效。

**方案**：信号内部使用UUID标识，显示名只是一个可修改的标签。

- 信号UUID格式：`sig-550e8400-e29b-41d4-a716-446655440000`
- 显示名（如"温度传感器"）可随意修改，不影响映射关系
- 测试用例中`target`字段引用的是UUID，UI显示时翻译为显示名
- 帧编辑器字段、ICD映射规则、拓扑编辑器连线全部基于UUID关联
- 改名不会导致引用失效

**实现**：使用QUuid::createUuid()生成，前缀"sig-"区分信号UUID与其他UUID。

### 7.2 Dry Run影子运行

**目的**：在没有真实硬件的环境下，仍能开发和调试测试用例。

**方案**：

```
Engine ──ICD API──▶ ICD ──▶ DataPool ──▶ HAL(Simulate模式: 虚拟设备生成数据)
                       │                      │
                       ◀──────────────────────┘
```

- Engine和ICD在Dry Run模式下**无感知**——它们调用的API完全相同
- HAL层每个插件提供`simulate()`方法，切换到虚拟数据源
- DataPool不变，数据来源从真实硬件换成虚拟设备
- UI监控面板无感知，正常订阅SignalValue通道

**触发方式**：

| 方式 | 说明 |
|------|------|
| 全局开关 | 整个系统进入Dry Run，所有设备走虚拟 |
| 单设备开关 | 部分设备真实、部分设备虚拟（混合模式） |
| 开关位置 | HAL层插件级别，不向上透传 |

### 7.3 故障注入

**目的**：验证被测设备在异常条件下的鲁棒性。

**方案**：通过ICD层的FaultManager实现，在信号转换过程中注入故障。

**7种故障类型**

| 类型 | 实现位置 | 说明 | 示例 |
|------|---------|------|------|
| 信号值死滞(Stuck-at) | ICD转换 | 信号值固定为某值 | 温度固定读999 |
| 信号值加偏置/噪声 | ICD转换 | 在原值上叠加偏移量 | AD值多加5% |
| 协议CRC错误 | ICD封包 | 封包时篡改CRC字段 | CAN帧CRC字段写反 |
| 校验位错误 | ICD封包 | 封包时篡改校验位 | 429 label校验位取反 |
| 通信延迟 | DataPool | 数据延迟N ms到达 | 延迟100ms |
| 通信丢包 | DataPool | 按概率丢弃原始字节 | 10%丢包率 |
| 采样率异常 | DataPool | 数据频率突然变化 | 采样率突变 |

**故障清除机制**

- `ICD.clearFault(signalId)` — 清除单信号故障
- `ICD.clearAllFaults()` — 清除所有故障
- 用例结束时自动清除所有活跃故障，防止故障泄漏到下一个用例

### 7.4 传输通道分层

**问题**：TCP/UDP应该放在HAL层作为设备插件，还是放在ICD层作为传输通道？

**决策**：TCP/UDP作为ICD的传输通道选项，不在HAL层。

**理由**：
- TCP/UDP不是物理硬件设备，不存在"打开设备""关闭设备""设备自检"的概念
- 串口对应物理板卡（有总线号/槽位号），TCP/UDP是逻辑通道
- 如果TCP/UDP放在HAL层，每个TCP连接都要成为一个"设备插件"，插件数量膨胀
- 在ICD层，一个信号可以选择通过串口发送或通过TCP发送——这是传输方式的选择，不是设备的选择

**分层原则**：
- 串口 → HAL层ISerialPlugin（物理硬件）
- TCP/UDP → ICD层传输通道选项（通信方式）
- MQTT/WebSocket/HTTP → ICD协议转换内处理，底层走TCP

### 7.5 插件进程隔离

**MVP策略**：同进程QPluginLoader

| 阶段 | 方案 | 优点 | 缺点 |
|------|------|------|------|
| MVP | QPluginLoader同进程 | 简单、低延迟、开发成本低 | 单插件崩溃可能影响整个进程 |
| 远期 | 评估独立进程隔离 | 插件崩溃不影响主进程 | IPC通信延迟、开发复杂度高 |

接口层面保持抽象（IDevicePlugin），不暴露IPC细节，后续切换到独立进程方案不影响上层代码。

---

## 8 可视化编辑器设计

### 8.1 帧协议编辑器

**对应数据**：ProtocolRegistry

**功能**：
- 以表格方式编辑帧字段结构：字段名、起始位、位长、字节序（大端/小端）、编码方式（无符号/有符号/浮点/BCD/枚举）
- 定义帧的校验规则：CRC类型、校验和算法
- 实时预览帧的二进制布局（位图可视化）
- 支持帧模板导入/导出（JSON/XML格式）

**交互方式**：
- 表格行 = 帧字段，可拖拽调整顺序
- 字段选中时高亮显示对应的位区域
- 删除字段时执行**引用检查**：如果该字段被ICD信号的SignalMapper引用，则禁止删除或标记映射失效（红色高亮提示）

**帧编辑器与ICD编辑器的边界**：
- 帧编辑器管"比特流长什么样" → 产出Protocol对象 → 存入ProtocolRegistry
- ICD编辑器管"工程值怎么映射到比特流哪个位置" → 产出SignalMapping → 存入SignalMapper
- 两者通过protocolId关联

### 8.2 ICD编辑器

**对应数据**：SignalMapper

**功能**：
- 以表单/表格方式编辑信号映射关系
- 定义信号的基本属性：UUID、显示名、单位、量程、有效性范围
- 定义映射关系：信号 → 设备通道 + 协议字段
- 定义转换规则：线性(y=kx+b)、多项式、枚举映射、Lua脚本
- 选择传输通道：串口/TCP/UDP
- 信号分组管理：按系统、按功能、按设备分组

**交互方式**：
- 主视图为信号列表表格，每行一个信号
- 点击信号展开详细配置面板
- 转换规则可视化编辑（线性：输入k和b参数；枚举：输入值-名称对照表）
- 支持批量导入/导出信号定义

### 8.3 拓扑编辑器

**对应数据**：SignalMapper（映射数据）+ 可视化元数据（分开存储）

**功能**：
- 基于Qt GraphicsView框架的图形化连线编辑
- 拖拽放置硬件设备节点和被测设备(UUT)节点
- 通过拖拽连线描述硬件通道与UUT接口的映射关系
- UUT节点是拓扑图上的**视觉装饰**，仅表示被测设备对外暴露的接口

**交互方式**：
- 左侧面板：硬件设备列表和UUT模板库，拖拽到画布
- 画布：节点可自由移动、连线可拖拽创建
- 右侧面板：选中连线的属性编辑（关联的信号UUID、转换规则）
- 连线创建时自动在SignalMapper中创建映射记录
- 连线删除时提示是否同时删除SignalMapper中的映射

**可视化元数据与映射数据分开存储**：
- 映射数据（哪个信号映射到哪个通道）存入SignalMapper
- 可视化元数据（节点坐标、连线样式、节点颜色）单独存储
- 修改节点位置不影响映射关系

### 8.4 用例编辑器

**对应数据**：测试用例JSON文件

**功能**：
- 以可视化方式编辑测试用例的步骤列表
- 每个步骤以卡片形式展示，显示cmd类型和关键参数
- 支持添加/删除/重排步骤
- 支持控制流指令（LOOP/WHILE/IF）的折叠/展开
- 步骤中的target字段下拉选择ICD中已定义的信号（通过UUID引用）
- 内置JSON Schema校验，实时提示语法错误

**交互方式**：
- 主视图为步骤卡片列表
- 点击步骤卡片展开参数编辑面板
- 底部工具栏提供"添加步骤"按钮（下拉选择指令类型）
- 支持切换到代码视图，直接编辑JSON源码
- 支持从Excel文件导入、导出为Excel/YML

### 8.5 监控面板

**对应数据**：Engine执行状态 + ICD信号值

**功能**：
- 实时信号值表格/仪表盘显示
- 步骤执行进度条和当前步骤高亮
- 变量监视窗口（脚本变量值 + ICD信号值）
- 执行日志实时滚动
- 执行控制按钮（开始/暂停/恢复/终止/单步）

---

## 9 远期展望

以下为MVP阶段之后的中长期方向，不做详细设计，仅记录方向性思考：

| 项目 | 当前策略 | 远期方向 | 触发条件 |
|------|---------|---------|---------|
| Web迁移 | 接口参数只用基本类型和QVariant | Engine-UI间加JSON/gRPC通信层 | 需要远程执行或云端化 |
| 脚本语言 | 仅Lua(sol2 + Lua Debug Library) | IScriptEngine抽象支持Python/JS | 用户需求驱动 |
| 并行执行 | 顺序执行 | 多用例并行/步骤并行 | 大规模用例集性能瓶颈 |
| DataPool | 融入ICD，离散事件型Pub/Sub | 信号量>10000时拆分为独立层，支持时序数据库 | 信号规模增长 |
| AD流式数据 | 不考虑连续流式 | 高速采集场景可能需要直推路径 | 高速AD需求 |
| 数据记录与回放 | MVP记录执行日志 | 离线回放、波形回放、HDF5/时序数据库 | 数据分析需求 |
| 报告生成 | MVP简单文本/HTML | PDF(libharu)/Excel(QXlsx)/图表嵌入 | 报告合规性要求 |
| 插件进程隔离 | MVP同进程QPluginLoader | 独立进程隔离(QLocalSocket/QProcess) | 第三方插件稳定性问题 |
| 车载总线扩展 | MVP仅CAN/CAN FD | LIN、FlexRay支持 | 车载测试场景需求 |

---

## 10 开发计划

### 10.1 阶段概览

| 阶段 | 名称 | 状态 | 核心交付物 | 预估工期 |
|------|------|------|-----------|---------|
| 1 | 基础框架搭建 | ✅ 已完成 | 开发环境、核心基础设施、主窗口框架 | 3周 |
| 2 | 设备管理（HAL层） | 待开发 | 设备插件接口定义、Mock插件、设备树UI、自检流程 | 3周 |
| 3 | ICD信号层 | 待开发 | SignalMapper、ProtocolRegistry、传输通道、故障注入 | 4周 |
| 4 | 用例管理层 | 待开发 | JSON用例格式、格式转换器、Schema校验、用例编辑器 | 2.5周 |
| 5 | 测试引擎层 | 待开发 | Lua引擎集成、Lua API、执行控制、调试器、报告生成 | 3周 |
| 6 | 测试与优化 | 待开发 | 全模块回归测试、集成测试、兼容性测试、用户体验优化 | 2周 |

**总预估工期**：约17.5周（含已完成的阶段1）

### 10.2 阶段1：基础框架搭建（已完成）

| 交付物 | 状态 |
|--------|------|
| CMake + Ninja构建环境，VS2019 x64编译 | ✅ |
| 核心基础设施（Logger/ConfigManager/CrashHandler/异常框架/工具类） | ✅ |
| 插件框架（IPlugin/IDevicePlugin/PluginManager） | ✅ |
| 项目管理系统（.etproj格式、创建/打开/关闭/最近项目） | ✅ |
| 主窗口6区布局（QADS停靠式界面） | ✅ |
| 文件浏览器、多标签编辑器（QScintilla）、全局搜索 | ✅ |
| 底部面板（输出/问题/终端） | ✅ |
| 设置页面、会话持久化、单实例检测 | ✅ |
| 单元测试框架（10+测试文件） | ✅ |

### 10.3 阶段2：设备管理（HAL层）

**核心任务**：

| 编号 | 任务 | 说明 |
|------|------|------|
| 2.1 | IDevicePlugin基类增强 | 添加selfTest()/simulate()/deviceStatus()/configMetaData()通用接口 |
| 2.2 | IADevicePlugin v3.0补充 | 已有头文件，需补充剩余Mock插件实现 |
| 2.3 | IDADevicePlugin | DA输出插件接口定义 + Mock实现 |
| 2.4 | IDioPlugin | 开关量IO插件接口定义 + Mock实现 |
| 2.5 | IPulsePlugin | 脉冲信号插件接口定义 + Mock实现 |
| 2.6 | ISerialPlugin | 串口族插件接口定义 + Mock实现（RS232/422/485） |
| 2.7 | ICanPlugin增强 | CAN/CAN FD插件接口增强 + Mock实现 |
| 2.8 | IArinc429Plugin增强 | A429插件接口增强 + Mock实现 |
| 2.9 | IMil1553Plugin | 1553B插件接口定义 + Mock实现（BC/RT/MT模式） |
| 2.10 | IVisaPlugin | VISA SCPI程控仪器插件接口定义 + Mock实现 |
| 2.11 | 设备管理器UI | 设备树侧边栏视图（厂家→分类→设备→通道） |
| 2.12 | 设备自检流程 | 启动时统一自检，VISA发*IDN?验证 |
| 2.13 | Dry Run支持 | 每个插件simulate()模式切换 |
| 2.14 | DeviceInfo增强 | bus_number/slot_number/card_serial字段 |

### 10.4 阶段3：ICD信号层

**核心任务**：

| 编号 | 任务 | 说明 |
|------|------|------|
| 3.1 | SignalMapper | UUID→deviceId+channelId+protocolId映射，转换规则 |
| 3.2 | ProtocolRegistry | 协议定义仓库，Protocol{字段列表+字节序+校验+pack/unpack} |
| 3.3 | 协议转换引擎 | pack(工程值→原始字节)/unpack(原始字节→工程值) |
| 3.4 | 传输通道层 | 串口/TCP/UDP通道抽象，ICD选择通道发送原始字节 |
| 3.5 | FaultManager | 故障注入管理（7种故障类型） |
| 3.6 | SignalValueCache (CVT) | 信号最新值缓存 |
| 3.7 | DataPool (Pub/Sub) | 离散事件型发布订阅 |
| 3.8 | 帧协议编辑器UI | ProtocolRegistry的可视化编辑前端 |
| 3.9 | ICD编辑器UI | SignalMapper的表单/表格编辑方式 |
| 3.10 | 拓扑编辑器UI | SignalMapper的图形化连线编辑（Qt GraphicsView） |
| 3.11 | 导入导出 | XML/JSON/YAML/Excel导入导出 |

### 10.5 阶段4：用例管理层

**核心任务**：

| 编号 | 任务 | 说明 |
|------|------|------|
| 4.1 | JSON用例格式v1.0 | 完整Schema定义，10基本指令+3控制流指令 |
| 4.2 | 条件表达式统一 | target+op+value结构，IF/WHILE/WAIT复用 |
| 4.3 | 控制流指令 | LOOP(固定次数)、WHILE(条件循环+interval+timeout)、IF(条件分支) |
| 4.4 | 嵌套约束 | steps/then_steps/else_steps内不允许嵌套控制流 |
| 4.5 | JSON→Lua转换器 | 所有指令类型到Lua脚本的转换 |
| 4.6 | Excel→JSON转换器 | 标准模板列映射，Sheet=用例 |
| 4.7 | YML→JSON转换器 | 结构完全对应，天然互换 |
| 4.8 | JSON Schema校验 | 语法校验+嵌套约束检查 |
| 4.9 | 用例CRUD | 创建/读取/更新/删除 |
| 4.10 | 版本管理 | 作者、修订记录 |
| 4.11 | 用例编辑器UI | 可视化编辑界面 |

### 10.6 阶段5：测试引擎层

**核心任务**：

| 编号 | 任务 | 说明 |
|------|------|------|
| 5.1 | Lua引擎集成 | sol2嵌入，隔离VM，C++端封装硬件控制供脚本调用 |
| 5.2 | IICDEngineInterface | setSignal/getSignal/verifySignal/waitForSignal/injectFault/clearFault |
| 5.3 | Lua API绑定 | 10个API函数对应10种基本指令 |
| 5.4 | 执行控制 | 暂停/恢复/终止 |
| 5.5 | 调试器 | 断点(行断点+条件断点)/单步(Over/Into/Out)/变量监视/调用栈 |
| 5.6 | 数据记录 | 步骤执行记录、断言结果、故障注入记录 |
| 5.7 | 监控面板UI | 实时日志/通道数据/变量值/执行进度 |
| 5.8 | 报告生成 | MVP简单文本/HTML |
| 5.9 | 故障自动清除 | 用例结束时自动清除所有活跃故障 |

### 10.7 阶段6：测试与优化

| 编号 | 任务 | 说明 |
|------|------|------|
| 6.1 | 全模块回归测试 | 各模块独立单元测试 |
| 6.2 | 全链路集成测试 | HAL→ICD→用例→引擎→报告闭环验证 |
| 6.3 | 兼容性测试 | Windows 10/11兼容性验证 |
| 6.4 | 压力/稳定性测试 | 长时间运行、大量信号、大量用例 |
| 6.5 | 用户体验优化 | 界面交互优化、性能优化 |
| 6.6 | 打包与文档 | 安装包、用户手册、开发文档 |

### 10.8 里程碑

| 里程碑 | 完成标志 | 对应阶段 |
|--------|---------|---------|
| M1：框架就绪 | 主窗口可运行，插件可加载，编辑器可使用 | 阶段1 ✅ |
| M2：设备就绪 | 所有插件接口定义完成，Mock插件可运行，设备树可展示，自检通过 | 阶段2 |
| M3：信号就绪 | ICD信号映射可用，协议转换正确，拓扑编辑器可连线 | 阶段3 |
| M4：用例就绪 | JSON用例可编辑、校验、转换为Lua脚本 | 阶段4 |
| M5：引擎就绪 | Lua脚本可执行、调试、生成报告 | 阶段5 |
| M6：产品发布 | 全链路闭环测试通过，安装包可交付 | 阶段6 |

---

## 11 验证方案

### 11.1 单元测试

- 覆盖范围：核心模块（PluginManager、SignalMapper、ProtocolRegistry、转换引擎、用例校验器、Lua API绑定）
- 覆盖率目标：核心模块≥80%
- 测试框架：Google Test 1.17.0
- 执行方式：CMake CTest集成，CI自动运行

### 11.2 集成测试

- **全链路闭环验证**：从硬件数据采集 → ICD协议解包 → 信号值缓存 → 脚本读取验证 → 报告生成的完整数据流通路
- **多格式用例验证**：同一测试逻辑分别用JSON、Excel、YML编写，验证转换结果一致
- **故障注入验证**：注入各类故障后验证信号值确实被篡改、清除故障后恢复正常
- **Dry Run验证**：不连接任何真实硬件，使用Mock插件跑通完整测试流程

### 11.3 性能测试

- 信号采集延迟：端到端延迟（硬件数据到达 → 信号值可读取）< 10ms
- 脚本执行开销：Lua API调用的C++/Lua切换延迟 < 1ms
- CVT缓存读取：单次getSignal()调用 < 0.1ms
- 报告生成：1000步用例的报告生成 < 5s

### 11.4 兼容性测试

- Windows 10 21H2+ / Windows 11
- MSVC2019编译环境
- Qt 5.12.12运行时

---

## 附录

### A 术语表

| 术语 | 全称 | 说明 |
|------|------|------|
| IATP | Integrated Automated Test Platform | 综合性自动化测试平台 |
| HAL | Hardware Abstraction Layer | 硬件抽象层 |
| ICD | Interface Control Document | 接口控制文档，此处指信号映射层 |
| CVT | Current Value Table | 当前值表，信号最新值缓存 |
| UUID | Universally Unique Identifier | 通用唯一标识符 |
| UUT | Unit Under Test | 被测设备 |
| VISA | Virtual Instrument Software Architecture | 虚拟仪器软件架构 |
| SCPI | Standard Commands for Programmable Instruments | 可编程仪器标准命令 |
| Pub/Sub | Publish/Subscribe | 发布/订阅模式 |
| MVP | Minimum Viable Product | 最小可行性产品 |
| QADS | Qt Advanced Docking System | Qt高级停靠系统 |

### B 参考文档

| 文档 | 说明 |
|------|------|
| 构思.md | 项目原始需求愿景 |
| 架构梳理.md | V1.0正式架构文档 |
| IATP_Comprehensive_Design_Spec.md | 早期IATP设计规格（五层架构版本） |
| IATP_Core_Technical_Points.md | 早期IATP核心技术白皮书 |
