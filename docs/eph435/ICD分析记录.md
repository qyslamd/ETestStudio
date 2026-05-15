# ICD 分析记录

> 本文基于 EPH-435（C# .NET Framework）的源码分析，厘清 ICD 涉及的四层架构，讨论帧结构的由来，以及在 DA 类设备上的处理方式。

---

## 1. 信号路由系统的四层架构

EPH-435 中一条 `SET "迎角" 45` 从 DSL 执行到硬件板卡，涉及四个不同项目/模块的协作。先给出全局视图。

```
                    ┌──────────────────────────────────────┐
    路由胶水层       │  MainFm.IcdOperation.cs              │
    (MainFm)        │  CSV查表、设备路由、双机遍历、类型转换  │
                    └──────────┬───────────────────────────┘
                               │ WriteSignal(path, value)
                    ┌──────────▼───────────────────────────┐
    硬件实现层       │  EphAutoTest.IO.Imp                  │
    (IoCustomSession)│  DoWrite(IcdNode) 多态实现            │
                    │     ┌─────────────────────────┐       │
                    │     │  ICD 数据模型调用        │       │
                    │     │  node.Value.Value = v   │       │
                    │     └─────────────────────────┘       │
                    └──────────┬───────────────────────────┘
                               │ IIoOperation 接口
                    ┌──────────▼───────────────────────────┐
    硬件接口定义层   │  EphAutoTest.IO.Inf                  │
    (接口)          │  IIoSession / IIoOperation             │
                    └──────────────────────────────────────┘

                    ┌──────────────────────────────────────┐
    ICD 数据模型层   │  EphAutoTest.ICD                     │
    (纯数据模型)     │  IcdManager → IcdFrame → IcdNode    │
                    │  Frame/Node/Value + 位编解码 + 缩放   │
                    └──────────────────────────────────────┘
```

| 层 | 项目/模块 | 职责 |
|---|----------|------|
| **ICD 数据模型层** | `EphAutoTest.ICD` | Frame/Node/Value 结构、位编解码、物理↔原始值缩放变换 |
| **硬件接口定义层** | `EphAutoTest.IO.Inf` | `IIoSession`、`IIoOperation` 纯接口 |
| **硬件实现层** | `EphAutoTest.IO.Imp` | 各设备 DoWrite/DoRead 多态实现，内部调用 ICD 数据模型 |
| **路由胶水层** | `MainFm.IcdOperation.cs` | CSV 查表、设备路由、双机遍历、类型转换，不在任一库项目中 |

下面逐层展开。

---

## 2. EphAutoTest.ICD —— 数据模型层

### 2.1 职责边界

`EphAutoTest.ICD` 是一个纯粹的**数据模型项目**。它只做三件事：

1. 定义 Frame/Node/Value 的对象结构
2. 提供 IcdManager 按名查找节点（遍历内存树，O(n)）
3. 提供物理值↔原始值变换 + 位域编码/解码

它**不**持有任何路由映射表（CSV、_dicDevToBoard），**不**保存任何设备会话引用。

### 2.2 IcdManager.Find 的本质

```csharp
public IcdNode Find(string systemName, string signalName, IcdFrameType frameType)
{
    foreach (var frame in _frames)
        if (frame.FrameType == frameType)
            if (frame.Name.Contains(systemName))
            {
                node = frame.Find(signalName);
                if (node != null) return node;
            }
    return null;
}
```

这是一个**线性遍历**——三层嵌套 foreach（帧列表→帧内节点→递归子节点）。它不是哈希索引，不是数据库查询。

### 2.3 类图关系

```
IcdManager (单例)
  └─ IList<IcdFrame> _frames
       └─ IcdFrame : IcdNode, IBindingList
            ├─ byte[] _dataCache          ← 帧共享缓存
            ├─ IList<IcdNode> _childNodes ← 帧内信号节点
            │    └─ IcdNode
            │         ├─ IIcdValue _value
            │         │    ├─ Value { get; set } → 触发 UnConvert + UnExtract
            │         │    ├─ Extract(byte*)     ← raw → phys
            │         │    └─ UnExtract(byte*)   ← phys → raw (位域写入Cache)
            │         └─ ICDWord _schema         ← XML 定义
            │              ├─ ScaleA / ScaleB
            │              ├─ LinkTo
            │              └─ StartBit / BitWidth / Offset
            └─ (校验和节点) IcdSumNode / IcdXorNode
```

### 2.4 EphAutoTest.ICD 不提供什么

| 它不提供 | 那由谁提供 |
|----------|-----------|
| 信号名→ICD路径名的映射 | MainFm（CSV 查表） |
| ICD节点→硬件设备的映射 | MainFm（_dicDevToBoard） |
| 设备会话的创建和管理 | IoManager（IO.Imp） |
| 硬件读写的具体实现 | 各 Session 的 DoWrite/DoRead |

---

## 3. EphAutoTest.IO.Inf —— 硬件接口定义层

`EphAutoTest.IO.Inf` 是一个只有接口和枚举的项目，没有实现。

```csharp
public interface IIoSession
{
    bool Open(string resDescription);
    void Close();
    void Start();
    void Stop();
    bool IsSimulate { get; set; }
}

public interface IIoOperation
{
    object Read(string path);
    void Write<T>(string path, T value);
}
```

这个接口层的作用是让 MainFm 可以**不依赖具体板卡类型**来编程：

```csharp
var operationObj = IoManager.Instance.Sessions[device] as IIoOperation;
operationObj.Write(path, value);
```

---

## 4. EphAutoTest.IO.Imp —— 硬件实现层

### 4.1 IoManager：设备创建与会话缓存

URI 来自 `app.config` 的 `device.list` 段，`IoManager.Create(uri)` 是一个以 URI 字符串为 key 的工厂方法：

| URI 包含关键字 | 会话类型 |
|---|---|
| `5272` | `Eph5272ChannelSession` (ARINC 429) |
| `XT1394` | `Xt1394Session` (1394) |
| `5173A` | `Eph5173ASession` (DA 模拟量输出) |
| `COMPOWER_COM` | `ComPowerControlSession` (直流电源) |
| `ADTS300H` | `ComADTS300HSession` (压力给定) |
| `5276H_00_COM6` | `Eph5276HChannelToCOMSession` (串口) |
| `UDP` | `EphUDPSession` (调试用) |

### 4.2 IoCustomSession：ICD 值变换 + DoWrite 抽象模板

```csharp
void Write<T>(string path, T value)
{
    node = _icdManager.Find(path, IcdFrameType.iftCmd);
    if (node == null)
        node = _icdManager.Find(path, IcdFrameType.ifConfig);

    node.Value.Value = value;
    // ↑ 触发 ICD 编码链: DoSetValue → UnConvert(逆缩放) → UnExtract(位域→Cache)

    DoWrite(node, name);  // 多态调用子类的硬件写入
}

protected abstract void DoWrite(IcdNode node, string name);
```

### 4.3 各设备 DoWrite 的分化

```
DoWrite 多态实现
├── Eph5272ChannelSession:  Cache[0..3] → 32bit → Eph5272_Transmit_Scheduled()
├── Xt1394Session:          Cache → Struct → Mil1394_CC_MSG_ASYNC_Data_Set()
├── Eph5173ASession (DA):   node.Value.Value → Eph5173_DA_SetOutVoltage(ch, f)
│                            (忽略 Cache，只取物理值)
├── ComPowerControlSession:  "VOLT 28.0\r" → _port.Write(cmd)
│                            (忽略 Cache，转成SCPI指令)
└── ComADTS300HSession:      "PS2000\r" → _port.Write(cmd)
```

---

## 5. MainFm.IcdOperation —— 路由胶水层

这是关键点：**路由逻辑不在 ICD 数据模型项目里，在 MainFm 里**。

### 5.1 CSV 查表：信号名 → ICD 路径名

```csharp
Engine_WriteSignalRequired(TestEngine engine)
{
    var device_signal_sign = GetWriteControlDeviceStr("迎角");
    // ↑ 查 "信号KeyValue工装发送.csv"
    //   "迎角" → "A429_IN4(221) 迎角"
}
```

CSV 格式：`<ICD路径名>, <DSL中使用的信号名>`。文件编码为 GBK。

### 5.2 _dicDevToBoard：ICD 路径名 → 设备名

```xml
<!-- app.config SignalChannelName 段 -->
<add key="ISI-01#A429_IN4(221)" value="1394B"/>
<add key="ISI-01#DA0_CH5" value="DA0"/>
```

这是在 ICD 节点名和硬件物理设备之间建立路由。

### 5.3 WriteSignal：完整的路由流程

```csharp
WriteSignal(string nodeName, double value)
{
    // 1. 查设备名
    string device = "";
    foreach (var temp in _dicDevToBoard)
        if (nodeName.Contains(temp.Key))
            { device = temp.Value; break; }

    // 2. 取会话
    var operationObj = IoManager.Instance.Sessions[device] as IIoOperation;

    // 3. 从发送帧列表中按名匹配 ICD 节点
    foreach (var frame in _wtFramesList)
    foreach (var node in frame.AllChildNodes)
    {
        if (node.Name == nodeName)
        {
            var path = frame.Name + "/" + nodeName;

            // 4. 类型转换（DSL给double，ICD节点可能是byte/uint等）
            object objValue = ConvertToNodeType(node, value);

            // 5. 调用 IO 层写入
            operationObj.Write(path, objValue);
            break;
        }
    }
}
```

支持双机遍历：`_products = {"ISI-01", "ISI-02"}`，对每台设备分别调用一次 WriteSignal。

### 5.4 双重 IcdManager.Find 的冗余

`WriteSignal` 在调用 `Write(path, value)` 前已经做了一次遍历匹配，而 `IoCustomSession.Write<T>()` 内部又用 `_icdManager.Find(path)` 查了一次。这是设计冗余。

---

## 6. ICD 为什么设计成帧结构

### 6.1 硬件 API 粒度

通讯协议（ARINC 429、1394、CAN 等）的硬件 API 以"帧"为单位收发：

```c
int Eph5272_Transmit_Scheduled(int card, ushort ch, uint flag, ref uint sendData);
int Mil1394_CC_MSG_ASYNC_Data_Set(void* handle, ...);
```

ICD 帧的 `_dataCache` 字节数组正好对应硬件 API 的"数据"入参。

### 6.2 数据一致性

同一条帧里的信号共享同一个 Cache，编码后自动打包在连续字节中：

```csharp
_dataCache = _owner._dataCache;  // IcdNode 指向所属帧的缓存
```

- 同帧信号自动打包，不需要额外组装
- 校验和/计数自动计算：`IcdFrame.Encode()` 遍历节点更新 Sum/Count
- 单次硬件调用完成整帧发送

### 6.3 ICD 文档本身就是按帧组织的

航空电子的 ICD 规格说明书按"总线→消息→信号"组织。一份 ICD XML 文件对应一条消息定义：

```
Schema/
  A429_00_ISI_01_发送_Label050_5272_00.xml    ← 一条消息 = 一个帧
  大华电源_发送电压_ComPower_COM1.xml            ← 电源指令 = 一个帧
  压力给定_发送静压总压_ComADTS300H_COM2.xml      ← 压力指令 = 一个帧
```

**结论：帧结构是协议粒度、数据一致性、ICD 文档组织方式三者共同决定的自然结果。**

---

## 7. 帧结构在 DA 类设备的处理

### 7.1 通讯协议 vs DA vs 串口仪器的对比

| 环节 | A429/1394 | DA（5173A） | 串口仪器（电源/压力） |
|------|-----------|-------------|----------------------|
| 路由路径 | 信号→CSV→ICD路径→_dicDevToBoard→设备→Write | 同上 | 同上 |
| IcdManager.Find | 查 iftCmd 帧 | 查 iftCmd 帧 | 查 iftCmd 帧 |
| node.Value.Value=v | 编码到 Cache | Cache被编码但不使用 | Cache被编码但不使用 |
| DoWrite | Cache → 整帧二进制 → 硬件 | `node.Value` → 物理值 → DLL | `node.Value` → 协议字符串 → 串口 |
| 路由胶水层感知差异 | 不感知 | 不感知 | 不感知 |

### 7.2 DA 上的帧结构被"虚用"

DA 设备的 ICD 帧实质只用到了三个功能：**name 查找、value 物理值 + 缩放、LinkTo 通道映射**。没用到 Cache、位域编码、校验和。

等效地说，DA 的 ICD 帧是一个**逻辑帧**——它存在只是为了共享 ICD 数据模型的查找和值变换基础设施。

### 7.3 冗余的工程权衡

开销：
- DoWrite 前 UnExtract → BitSet64 白跑一趟（CPU 可忽略）
- ICD XML 中多定义了一个无意义的位宽和偏移

收益：
- MainFm.WriteSignal 不感知设备类型（统一走 IIoOperation.Write）
- 路由映射表（CSV + _dicDevToBoard）格式完全一致
- 新增设备类型不需要改动 ICD 数据模型或路由逻辑

**结论：开销极小，收益明显，统一是划算的。**

---

## 8. ScaleA / ScaleB 的作用

```csharp
// 值变换公式: physical = ScaleA × raw + ScaleB
```

### 写路径（SET）：物理值 → 原始值写入帧

```csharp
// IcdIntValue.UnConvert()
tempDouble = (45.0 - ScaleB) / ScaleA;
int intValue = (int)Math.Round(tempDouble);
_rawValue.Value = intValue & edgeMask;  // 截断到位宽 → Cache
```

### 读路径（CHECKAUTO）：帧原始值 → 物理值

```csharp
// IcdIntValue.Convert()
_value = ScaleA * tempValue + ScaleB;
```

### Schema 中实际例子

| 信号 | IsScaled | ScaleA | ScaleB | 含义 |
|------|----------|--------|--------|------|
| Baro_Corr_hPa (11bit) | 1 | 0.1 | 0 | 总线传 745~1100 整数，应用看 74.5~110.0kPa |
| Baro_Corr_Hg (12bit) | 1 | 0.01 | 0 | 总线传 2200~3250 整数，应用看 22.00~32.50mmHg |
| 5V导光板电源 电压 (16bit) | 1 | 0.01 | 0 | 总线传 500，应用看 5.00V |
| Label/SDI/SSM/自检帧 | 0 | - | - | 原始值 = 物理值，无缩放 |

`IsScaled=0` 意味着没有缩放关系，在 3800+ 个 XML 节点中占绝大多数。`ScaleA=0` 时有除零保护（`if (_schema.ScaleA != 0F)`）。

**缩放的目的**：协议总线上传有限位宽的整数，应用层读写带单位的物理量。缩放把协议层的数据编码细节从测试脚本中抹掉了。

---

## 9. C# ICD vs C++ icd_utility 对比

| 维度 | C# EphAutoTest.ICD | C++ icd_utility |
|------|-------------------|-----------------|
| 语言 | C# .NET 4.7.2 | C++17 |
| 依赖 | 无 | pugixml + nlohmann/json + tl/expected |
| 配置格式 | 仅 XML | XML + JSON |
| 值类型 | `object` 运行时类型 | `std::variant<>` 编译期类型安全 |
| 帧缓存 | 同帧节点**共享 `_dataCache`** | Frame 独立 `decode_buffer_` |
| set_value 副作用 | **自动触发** UnConvert(缩放) + UnExtract(位域) | **不做缩放变换**，只写位域 |
| 校验和/计数 | `Encode()` 自动更新 Sum/XOR/Count | Tag 枚举已定义但**无实现** |
| 错误处理 | `Debug.Assert` 静默断言 | `tl::expected` 带错误信息传播 |
| 查找 | 线性 O(n) 扫描 | O(1) frame id/name 索引 + O(n) 节点扫描 |

### 关键差异

**C# 完整可运行但错误处理粗糙，C++ 基础架构更干净但缩放和校验和两个核心功能未实现。**

```
C# EphAutoTest.ICD          C++ icd_utility
缩放 ScaleA/ScaleB  ───────  NodeAttrs 存了但不用
校验和 Sum/XOR/Count ──────  Tag 枚举已定义但无运行时行为
帧缓存共享                 Frame 独立 buffer（更清晰）
Debug.Assert              tl::expected（更严谨）
```

---

## 10. IATP 六层架构中 icd_utility 的定位

```
IATP 六层架构              icd_utility 覆盖范围
┌─────────────────────┐
│ 应用层 (UI)          │  ← 不相干
├─────────────────────┤
│ 用例管理层 (TestCase) │  ← 不相干
├─────────────────────┤
│ 测试引擎层 (Engine)   │  ← 不相干
├─────────────────────┤
│ ICD信号层            │
│  ├─ SignalMapper    │  ← 不覆盖（需新建）
│  ├─ ProtocolRegistry│  ← icd_utility = 它的 pack/unpack 引擎
│  ├─ FaultManager    │  ← 不覆盖
│  ├─ SignalValueCache│  ← 不覆盖
│  └─ DataPool        │  ← 不覆盖
├─────────────────────┤
│ 硬件抽象层 (HAL)      │  ← 不相干（已有完整插件接口定义）
└─────────────────────┘
```

**icd_utility 只做一件事**：给 ProtocolRegistry 提供"工程值 ↔ 原始字节帧"的双向转换能力。

### 需要补的

| 需要补的 | 工时 | 优先级 |
|---------|------|--------|
| 缩放变换集成到 set_value/decode | 1天 | 高（否则协议转换算出的值不对） |
| Frame::encode()（校验和/计数） | 1天 | 高（通讯协议必须） |
| NodeValue 放宽类型检查 | 0.5天 | 中（影响部分位域场景） |
