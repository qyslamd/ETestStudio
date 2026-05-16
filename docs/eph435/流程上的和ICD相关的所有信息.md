# EphAutoTest (eph435) ICD相关工作流全信息

> 本文是为了压缩会话上下文而做的信息 dump，包含从 EphAutoTest 项目（C#/.NET WinForms）中分析出的所有与 ICD 相关的架构、流程、映射关系、关键代码逻辑。供 etest-demo 设计参考。

---

## 一、项目结构概览

```
D:\sb\eph435\
├── EphAutoTest.ICD\              ← ICD核心库
│   ├── IcdManager.cs             ← ICD管理器（单例）
│   ├── IcdFrame.cs               ← ICD帧（继承IcdNode，含dataCache）
│   ├── IcdNode.cs                ← ICD节点（树节点）
│   ├── IcdValue.cs               ← ICD值基类
│   ├── IcdIntValue.cs            ← 有符号整型值
│   ├── IcdUIntValue.cs           ← 无符号整型值
│   ├── IcdLongValue.cs / IcdULongValue.cs
│   ├── IcdFloatValue.cs / IcdDoubleValue.cs
│   ├── IcdByteValue.cs           ← 单字节值
│   ├── IcdBytesValue.cs          ← 字节数组值
│   ├── IcdStringValue.cs         ← 字符串值
│   ├── IcdSumValue.cs            ← 累加和校验值
│   ├── IcdCmdNode.cs             ← 命令节点（继承IcdNode，发送方向用）
│   ├── IcdFilterFrame.cs         ← 过滤帧
│   ├── BitSet64.cs               ← 核心位操作工具
│   ├── IcdInf.cs                 ← 基础枚举和常量
│   ├── IConvert.cs               ← 可扩展转换接口
│   ├── ConvertorFactory.cs       ← 转换器工厂（反射加载外部dll）
│   └── Schema\                   ← XSD + XML定义文件
│       ├── IcdConfig.xsd         ← IcdConfig.xml 的 Schema
│       ├── IcdSchema.xsd         ← ICD XML 文件的 Schema
│       ├── IcdSchema.cs          ← Schema生成的C#代码
│       ├── IcdConfig.cs          ← Config Schema生成的C#代码
│       ├── IcdConfig.xml         ← ICD文件目录清单（核心索引）
│       └── A429_*.xml            ← 各Label的ICD定义（452个文件）
│
├── EphAutoTest.IO.Inf\           ← IO接口层
│   ├── IDeviceIo.cs              ← 设备IO接口
│   ├── IoSession.cs              ← IO会话
│   ├── IoManager.cs              ← IO管理器（单例，设备工厂）
│   ├── IoOperation.cs            ← IO读写操作接口
│   └── IIoEvents.cs              ← IO事件接口
│
├── EphAutoTest.IO.Imp\           ← IO实现层
│   ├── IoManager.cs              ← IoManager实现（单例）
│   ├── IoBaseSession.cs          ← 会话基类
│   ├── IoCustomSession.cs        ← 自定义会话（含IcdManager引用）
│   └── (各设备驱动: Eph6272T, Eph5272, ComSession等)
│
├── EphAutoTest\                  ← UI主程序（WinForms + DevExpress + ANTLR）
│   ├── MainFm.IcdOperation.cs    ← ICD操作分体类（关键逻辑所在）
│   └── MainForm.My.cs            ← 早期版本的ICD操作（更简单）
│
├── ConfigFiles\
│   ├── 信号KeyValue工装发送.csv   ← 写方向：友好名→ICD节点名
│   └── 信号KeyValue工装接收.csv   ← 读方向：友好名→ICD节点名
│
└── Schema\                       ← ICD XML 定义文件存放目录（452个xml文件）
```

---

## 二、核心架构分层

### 2.1 四层配置体系

```
┌─────────────────────────────────────────────────────┐
│  第1层: CSV (友好名 ↔ ICD节点名)                      │
│  信号KeyValue工装发送.csv                              │
│  "纬度" → "A429_IN1(110) GNSS_Latitude"              │
├─────────────────────────────────────────────────────┤
│  第2层: appSettings (产品信号通道 ↔ 设备资源名)         │
│  app.config <appSettings>                             │
│  "ISI-01#A429_IN1" → "6272T_00"                      │
├─────────────────────────────────────────────────────┤
│  第3层: IcdConfig.xml (ICD文件目录清单)                │
│  帧名" A429_00_ISI_01_发送_Label110_6272T_00         │
│  → 指向 A429_00_ISI_01_发送_Label110_6272T_00.xml    │
├─────────────────────────────────────────────────────┤
│  第4层: device.list (设备资源名 ↔ 物理硬件地址)        │
│  "6272T_00" → "EPH6272T::PXI6::2" (PXI6槽2)          │
└─────────────────────────────────────────────────────┘
```

### 2.2 数据流向（顶→底）

```
测试脚本/测试引擎
    │ Set 纬度 90°
    ▼
CSV 友好名翻译层        ← 信号KeyValue工装发送.csv
    │ "纬度" → "A429_IN1(110) GNSS_Latitude" + 拼上产品前缀
    ▼
appSettings 设备查表层   ← app.config <appSettings>
    │ "ISI-01#A429_IN1" → "6272T_00"
    ▼
IcdConfig 帧索引         ← IcdConfig.xml
    │ 匹配到帧 A429_00_ISI_01_发送_Label110_6272T_00
    ▼
ICD XML 信号定义         ← A429_00_ISI_01_发送_Label110_6272T_00.xml
    │ 找到节点 GNSS_Latitude: Offset=1, BitWidth=21, ScaleA=0.00017166
    │ 物理→原始编码: 90.0 / 0.00017166 = 524288
    │ BitSet64 写入 dataCache 指定位
    ▼
device.list 硬件地址     ← app.config <device.list>
    │ "6272T_00" → "6272T_00::EPH6272T::PXI6::2"
    ▼
IoManager 会话分发       ← IoManager.Instance.Sessions[]
    │ 取到 Eph6272TChannelSession 实例
    ▼
PXI 寄存器写入 → FPGA自动发送A429字到通道00
```

### 2.3 运行时四种映射关系

| 映射 | 数据源 | 键→值 | 构建时机 |
|------|--------|-------|---------|
| `_writeControlMap` | CSV文件 | `"纬度" → "A429_IN1(110) GNSS_Latitude"` | `initControlMap()` |
| `_dicDevToBoard` | appSettings | `"ISI-01#A429_IN1" → "6272T_00"` | `InitUnderTestDevice()` |
| `_wtFramesList` | IcdConfig.xml → ICD XML | IcdFrame 列表(Type=2) | `InitIcdManager()` |
| IoManager.Sessions | device.list | `"6272T_00" → Eph6272TChannelSession` | `ConnectDevices()` |

---

## 三、ICD 核心代码逻辑

### 3.1 IcdManager（单例）

```csharp
public class IcdManager {
    public static readonly IcdManager Instance = new IcdManager();

    // 帧容器
    private readonly IList<IcdFrame> _frames = new List<IcdFrame>();
    private readonly IDictionary<int, IcdFrame> _framesWithId = new Dictionary<int, IcdFrame>();

    // 构造函数：默认加载 {BaseDir}/IcdConfig.xml
    // 额外构造函数：支持 filter 过滤特定设备相关的帧

    public void Init(string fileName) {
        // 1) 反序列化 IcdConfig.xml → ICDConfig 对象
        ICDConfig icdConfig = (ICDConfig)SerializationHelper.LoadFromXml(fileName, typeof(ICDConfig));

        // 2) 遍历 <Files>/<FileInfo>
        foreach (var item in icdConfig.Files) {
            // 3) 反序列化单个 ICD XML → ICDData 对象
            ICDData curData = (ICDData)SerializationHelper.LoadFromXml(icdFileName, typeof(ICDData));

            // 4) 构造 IcdFrame
            curFrame = new IcdFrame(
                name:        item.Name,          // 来自 IcdConfig <Name>
                byteOrder:   item.ByteOrder,     // 0=小端, 1=大端
                frameType:   (IcdFrameType)item.Type,  // 1=Data(接收), 2=Cmd(发送), 4=Config
                schema:      curData
            );
            _frames.Add(curFrame);
        }
    }

    // 按 path（格式：帧名/信号名）查节点
    public IcdNode Find(string url, IcdFrameType frameType) {
        // 按 '/' 拆分 → systemUrl 和 signalName
        // 遍历 _frames，帧名做字符串匹配过滤
        // 在匹配的帧中递归查找 signalName
    }
}
```

### 3.2 IcdFrame（继承 IcdNode）

```csharp
public class IcdFrame : IcdNode, IBindingList {
    private byte[] _dataCache;              // 数据缓冲区
    private IcdNode _headNode;              // Tag=Head 的节点
    private IcdNode _sumNode;               // Tag=Sum/XOR 的节点
    private IcdNode _countNode;             // Tag=Count 的节点
    private readonly BindingList<IcdNode> _allChildNodes = new BindingList<IcdNode>();

    public IcdFrame(string name, IcdByteOrder byteOrder, IcdFrameType frameType, ICDData schema) {
        this.Name = name;
        this.FrameType = frameType;

        // 计算帧长度
        _length = CalLength(schema.Data.Item);
        _dataCache = new byte[_length];

        // 遍历 ICD XML 的 <Item> 创建节点树
        foreach (var item in schema.Data.Item) {
            node = CreatNode(this, frameType, baseOffset, item, byteOrder);
            _childNodes.Add(node);
            MapToList(node);

            // 按 Tag 标记特殊字段
            switch ((IcdTag)item.Tag) {
                case IcdTag.Head:   _headNode = node; break;
                case IcdTag.Sum:    _sumNode = node; break;
                case IcdTag.Count:  _countNode = node; break;
            }
        }
    }

    // 解码：外部数据 → dataCache → 子节点逐一 Decode
    public void Decode(byte[] data, int offset, int count) {
        Buffer.BlockCopy(data, offset, _dataCache, 0, count);
        // 为每个子节点调用 item.Value.Decode(pData)
    }

    // 编码：子节点数据 → dataCache → 校验重算
    public void Encode() {
        if (_countNode != null) _countNode.Value.Value = ...; // 计数自增
        if (_sumNode != null)   _sumNode.Value.Encode();       // 校验重算
    }

    // 展平节点列表（查找时不用递归）
    public IList<IcdNode> AllChildNodes => _allChildNodes;
}
```

### 3.3 IcdNode（树节点）

```csharp
public class IcdNode {
    public string Name { get; set; }       // 节点名（来自XML <Name>）
    public int Offset { get; set; }        // 字节偏移
    public int BitWidth { get; set; }      // 位宽
    public IIcdValue Value { get; set; }   // 值对象
    public IcdTag Tag { get; set; }        // 标签
    public ICDWord Schema { get; set; }    // XML原始schema引用

    // 读写值时经过缩放的逻辑
    public object DoGetDataObject() {
        if (IsScaled)  // 物理值 = ScaleA * 原始值 + ScaleB
            return _rawValue * ScaleA + ScaleB;
        else
            return _rawValue;
    }
}
```

### 3.4 BitSet64（核心位操作）

```csharp
public struct BitSet64 {
    private int _startBit;   // 起始位
    private int _bitWidth;   // 位宽（1-64）

    // 从 dataCache 中提取指定位范围的值
    public ulong Extract(byte* pData);

    // 将值写入 dataCache 的指定位范围
    public void UnExtract(byte* pData) {
        uint* pTempData32 = (uint*)pData;
        UInt32 mark = (UInt32)(0x1 << _bitWidth) - 1;
        *pTempData32 = (*pTempData32 & ~(mark << _startBit))
                     | ((UInt32)value << _startBit);
    }
}
```

### 3.5 IcdValue 值对象体系

```
IIcdValue (接口)
├── IcdBitValue (基类，位级类型: byte/int/long 等)
│   └── 包含 IcdIntValue, IcdUIntValue, IcdLongValue, IcdULongValue, IcdByteValue
├── IcdValueBase (基类，完整字节类型: float/double/bytes/string)
│   └── 包含 IcdFloatValue, IcdDoubleValue, IcdBytesValue, IcdStringValue
└── IcdSumValue (特殊：累加和校验)
    └── IcdXorValue (异或校验)
```

**收发两条线的值创建分支**：
- 接收(Data/Type=1): `new IcdNode(parent, baseOffset, schema, byteOrder)` — IcdNode.Value 只读，只做 Decode
- 发送(Cmd/Type=2): `new IcdCmdNode(parent, baseOffset, schema, byteOrder)` — IcdCmdNode.Value 可写，做 Encode

---

## 四、关键函数调用链（完整路径）

```
测试脚本: Set 纬度 90°
    │
    ▼
Engine_WriteSignalRequired(engine)                    ← MainFm.IcdOperation.cs:391
    │ cmd.Signal = "纬度", cmd.Value = 90.0
    │
    ├─ GetWriteControlDeviceStr("纬度")               ← line 368
    │   → _writeControlMap["纬度"]
    │   → "A429_IN1(110) GNSS_Latitude"               ← CSV列A
    │
    ├─ nodeName = $"ISI-01#{device_signal_sign}"       ← line 403
    │   = "ISI-01#A429_IN1(110) GNSS_Latitude"
    │
    └─ WriteSignal(nodeName, 90.0)                    ← line 122
        │
        ├─ _dicDevToBoard 遍历（子串匹配）              ← line 125
        │   "ISI-01#A429_IN1(110)...".Contains("ISI-01#A429_IN1") == true
        │   → device = "6272T_00"
        │
        ├─ IoManager.Instance.Sessions["6272T_00"]    ← line 136
        │   → Eph6272TChannelSession (as IIoOperation)
        │
        ├─ _wtFramesList 遍历（node.Name 精确匹配）     ← line 140-145
        │   node.Name == "ISI-01#A429_IN1(110) GNSS_Latitude"
        │   → path = frame.Name + "/" + nodeName
        │   path = "A429_00_ISI_01_发送_Label110_6272T_00/ISI-01#A429_IN1(110) GNSS_Latitude"
        │
        ├─ 类型转换（IsScaled=0 时 double→目标类型）    ← line 149-176
        │   本例 IsScaled=1，直接传 90.0
        │
        └─ operationObj.Write(path, 90.0)             ← line 183
            │
            └─ IoCustomSession.Write<T>(path, value)  ← IoCustomSession.cs
                │
                ├─ _icdManager.Find(path, iftCmd)     ← IcdManager.cs:167
                │   │
                │   ├─ Extract(path) 按 '/' 拆分       ← line 324
                │   │   systemUrl  = "A429_00_ISI_01_发送_Label110_6272T_00"
                │   │   signalName = "ISI-01#A429_IN1(110) GNSS_Latitude"
                │   │
                │   └─ Find(systemUrl, signalName, frameType)  ← line 221
                │       │
                │       ├─ 遍历 _frames: 帧名匹配
                │       │   item.Name = "A429_00_ISI_01_发送_Label110_6272T_00"
                │       │   systemName.EndsWith(item.Name) == true ✅
                │       │
                │       └─ item.Find(signalName)      ← IcdFrame.cs:108
                │           → 递归查找子节点
                │           node.Name == "ISI-01#A429_IN1(110) GNSS_Latitude" ✅
                │           → 返回 IcdNode
                │
                ├─ node.Value.Value = 90.0            ← 触发ICD编码
                │   │
                │   ├─ IcdIntValue.UnConvert()        ← IcdIntValue.cs
                │   │   raw = (90.0 - 0) / 0.000171661376953125 = 524288
                │   │
                │   └─ BitSet64.UnExtract(pData)      ← BitSet64.cs:88
                │       写入 dataCache[1..3] 的 bit0..20
                │       DWORD bits 8..28 = 524288
                │
                └─ DoWrite(node, name)                ← Eph6272TChannelSession
                    │
                    ├─ frame.Encode(txData)           ← IcdFrame.cs:260
                    │   重算PARITY/SUM校验
                    │
                    └─ PXI 寄存器写入
                        → BAR0 + CH_OFFSET(ch0) + DATA_REG
                        → 6272T FPGA 自动发送 A429 32-bit word
```

---

## 五、ICD XML 文件结构（按 XSD Schema）

### IcdConfig.xsd — 目录清单

```xml
<ICDConfig>
  <Files>
    <FileInfo>
      <Name>A429_00_ISI_01_发送_Label110_6272T_00</Name>   ← 帧名（运行时标识）
      <ID>90</ID>                                            ← 数字ID
      <Type>2</Type>                                         ← 1=Data(接收),2=Cmd(发送),4=Config
      <Description>A429_00_ISI_01_发送_Label110_6272T_00</Description>
      <Enable>1</Enable>
      <WordType>0</WordType>
      <ByteOrder>0</ByteOrder>                               ← 0=小端, 1=大端
      <Path>A429_00_ISI_01_发送_Label110_6272T_00.xml</Path>  ← 实际ICD文件路径
    </FileInfo>
    ...
  </Files>
</ICDConfig>
```

### IcdSchema.xsd — 信号定义

```xml
<ICDData>
  <Name/>              <!-- 此处Name通常为空，帧名来自 IcdConfig.FileInfo.Name -->
  <Data>
    <Item>
      <Offset>0</Offset>        ← 字节偏移（相对帧起始）
      <StartBit>0</StartBit>    ← 起始位（在Offset字节内）
      <BitWidth>32</BitWidth>   ← 位宽
      <Type>dword</Type>       ← 数据类型：byte/int/uint/long/float/double/string/bytes/dword
      <Name>ISI-01#A429_IN1(110) 发送数据</Name>  ← 节点名
      <Description>...</Description>
      <GroupName>ISI-01#A429_IN1(110)</GroupName>
      <IsScaled>0</IsScaled>    ← 是否启用缩放
      <ScaleA>1</ScaleA>        ← 缩放系数A：物理值 = A × 原始值 + B
      <ScaleB>0</ScaleB>        ← 缩放系数B
      <Unit>°</Unit>            ← 单位
      <Tag>0</Tag>              ← 标签：0=无, 40=Head, 48=Length, 49=Count, 50=Sum, ...
      <SystemName>ARINC429总线通讯模块</SystemName>
      <Min>0</Min>              ← 最小值
      <Max>0</Max>              ← 最大值
      <ValueTextList>72</ValueTextList>  ← 预设值列表（LABEL用：十进制72=八进制110）
      <LinkTo>\6272T_00\ch0</LinkTo>     ← 关联的硬件通道
      <Childs>                  ← 子节点（嵌套结构）
        <Item>...</Item>
      </Childs>
    </Item>
  </Data>
</ICDData>
```

### IcdSchema.cs 关键结构

由 xsd.exe 从 IcdSchema.xsd 生成：

```csharp
public class ICDData {
    public string Name { get; set; }
    public ICDDataData Data { get; set; }
}

public class ICDDataData {
    public ICDWord[] Item { get; set; }
}

public class ICDWord {
    public int Offset;
    public int StartBit;
    public int BitWidth;
    public string Type;           // "byte", "int", "uint", "float", "dword"...
    public string Name;
    public string Description;
    public int IsScaled;          // 0 or 1
    public double ScaleA;
    public double ScaleB;
    public int Tag;
    public string Unit;
    public ICDWord[] Childs;      // 嵌套子节点
    public string ValueTextList;  // LABEL预设值
    public string LinkTo;         // 硬件通道引用
}
```

### 帧名编码规则

```
A429_00_ISI_01_发送_Label110_6272T_00
│      │   │    │     │       │
│      │   │    │     │       └─ 硬件资源名
│      │   │    │     └─ Label号（八进制）
│      │   │    └─ 方向（发送/接收/配置）
│      │   └─ 产品名
│      └─ 通道号
└─ 总线类型（A429 / 串口 / 电源 / 压力...）

其他示例:
大华电源_发送电压_ComPower_COM4
串口自检_发送_6276K8_00_COM7
压力给定_发送静压总压_ComADTS300H_COM2
摄像头_控制指令_LanTLIPC342_0
```

---

## 六、IcdConfig.xml 中注册的设备类型（实际部署）

| 设备 | 型号 | 连接 | 通道数 | 用途 |
|------|------|------|--------|------|
| A429板卡(机柜) | EPH6272T | PXI6 | 16 (00~15) | ARINC 429 激励/采集 |
| A429板卡(便携) | EPH5272 | PXI4 | 8 (00~07) | ARINC 429 便携版 |
| 离散量板卡 | EPH5121A | PXI6 | 1 | 离散量IO |
| 串口板卡 | 6276K8 | COM7~14 | 8 | 串口通信 |
| MIL-1553 | EPM6200 | PXI3 | 2 | 1553收发 |
| 大华电源 | POWERCTRL | COM3, COM4 | 2 | 直流电源控制 |
| 5V导光板电源 | POWERCTRL | COM1 | 1 | 导光板电源 |
| 压力给定设备 | ADTS300H | COM2 | 1 | 模拟静压总压 |
| 摄像头 | TL-IPC342 | 网络 | 1 | 视频采集 |
| 示波器 | DP07504 | USB | 1 | 信号测量 |

---

## 七、CSV 映射文件详解

### CSV 结构

```csv
XML文件中Node的name(value), 友好名1, 友好名2, 友好名3, ...
A429_IN1(110) GNSS_Latitude, GNSS_Latitude, 纬度, ,
A429_IN1(110) SSM,          GNSS_Latitude_SSM, 纬度_SSM, ,
```

### 读取逻辑

```csharp
// 列A = value（ICD XML 节点名）
// 列B~E = keys（友好显示名/别名）
// 字典: {友好名 → ICD节点名}
_writeControlMap["GNSS_Latitude"] = "A429_IN1(110) GNSS_Latitude"
_writeControlMap["纬度"]          = "A429_IN1(110) GNSS_Latitude"
```

### 发送/接收分离

- `信号KeyValue工装发送.csv` → `_writeControlMap`：
  包含 `A429_IN1` / `A429_IN2` / `A429_IN3` / `A429_IN4` + 压力给定 + 电源 + 摄像头 + 离散量 + 示波器的**发送/设置**信号
- `信号KeyValue工装接收.csv` → `_readControlMap`：
  包含 `A429_OUT` + 压力给定 + 电源 + 摄像头 + 示波器的**读取/接收**信号

---

## 八、app.config 结构

```xml
<appSettings>
  <add key="ProductList" value="ISI-01::ISI-02" />
  <add key="SignalChannelName" value="A429_IN1::A429_IN2::...::离散量" />
  <!-- {产品}#{通道} → 设备资源名 -->
  <add key="ISI-01#A429_IN1" value="6272T_00" />
  <!-- 信号CSV文件路径 -->
  <add key="WtSignalKeyValuePath" value="\ConfigFiles\信号KeyValue工装发送.csv" />
  <add key="RdSignalKeyValuePath" value="\ConfigFiles\信号KeyValue工装接收.csv" />
</appSettings>

<!-- 自定义配置节：设备资源 → 物理地址 -->
<device.list>
  <!-- PXI板卡: 资源名::型号::总线::槽号 -->
  <add key="6272T_00" value="6272T_00::EPH6272T::PXI6::2" />
  <!-- 串口: 资源名::名称::波特率::数据位::停止位::校验 -->
  <add key="ComADTS300H_COM2" value="ComADTS300H_COM2::ComADTS300H_COM2::115200::8::1::0" />
  <!-- 网络: 资源名::名称::IP::端口::用户::密码 -->
  <add key="LanTLIPC342_0" value="LanTLIPC342_0::LanTLIPC342_0::192.168.1.60::80::admin::12345678a" />
  <!-- USB(VISA): 资源名::Board::USB0::VID::PID::序列号::0::INSTR -->
  <add key="UsbDP07504_0" value="UsbDP07504_0::Board::USB0::0x0483::0x0001::CN2513084000620::0::INSTR" />
</device.list>
```

---

## 九、IO 层关键设计

### IoManager（单例）

```csharp
public class IoManager : IIoManager {
    public static readonly IoManager Instance = new IoManager();
    public IDictionary<string, IIoSession> Sessions { get; } = new Dictionary<string, IIoSession>();

    // 打开设备（查device.list → Create → Open）
    public IIoSession Open(string resDescription) {
        string uri = GetUri(resDescription);   // 查 device.list
        IIoSession session = Create(uri);      // URI匹配→具体设备
        session.Open(resDescription);
        Sessions[resDescription] = session;
        return session;
    }

    private string GetUri(string key) {
        // 从 ConfigurationManager.GetSection("device.list") 查找
        return deviceMap[key];  // key="6272T_00" → value="6272T_00::EPH6272T::PXI6::2"
    }

    private IIoSession Create(string uri) {
        // URI 字符串匹配设备类型
        if (uri.Contains("6272T")) return new Eph6272TChannelSession();
        if (uri.Contains("5272"))  return new Eph5272ChannelSession();
        if (uri.Contains("COM"))   return new ComSerialSession();
        if (uri.Contains("USB"))   return new UsbVisaSession();
        // ...
    }
}
```

### IoCustomSession（ICD 桥接层）

```csharp
public class IoCustomSession : IoBaseSession, IIoOperation {
    protected IcdManager _icdManager;  // 持有 IcdManager 引用

    // 写入：ICD节点名 → 编码 → 硬件写入
    public void Write<T>(string path, T value) {
        // 1. Find ICD 节点
        node = _icdManager.Find(path, IcdFrameType.iftCmd);

        // 2. 设置值（触发 ICD 编码：UnConvert + BitSet64.UnExtract）
        node.Value.Value = value;

        // 3. 硬件写入
        DoWrite(node, name);
    }

    // 读取：硬件读取 → ICD 解码 → 物理值
    public object Read(string path) {
        // 1. 硬件读取原始数据
        byte[] rawData = ReadRaw(name);

        // 2. 帧解码（BitSet64.Extract + IcdValue.Convert）
        frame.Decode(rawData, 0, rawData.Length);

        // 3. 返回物理值
        return node.Value.Value;
    }
}
```

---

## 十、初始化流程（启动顺序）

```
程序启动
    │
    ├─ InitIcdManager()            ← 加载 IcdConfig.xml → 构建 ICD 帧/节点树
    │   ├─ new IcdManager(filter)
    │   │   └─ Init(fileName)
    │   │       ├─ 反序列化 IcdConfig.xml → ICDConfig
    │   │       └─ 遍历 FileInfo，逐个加载 ICD XML → IcdFrame
    │   └─ _wtFramesList / _rdFramesList 分类
    │
    ├─ InitUnderTestDevice()        ← 加载 app.config 映射
    │   ├─ 读取 ProductList / SignalChannelName
    │   └─ 构建 _dicDevToBoard 字典
    │
    ├─ initControlMap()             ← 加载 CSV 映射
    │   ├─ 读取 WtSignalKeyValuePath → _writeControlMap
    │   └─ 读取 RdSignalKeyValuePath → _readControlMap
    │
    └─ ConnectDevices()            ← 打开硬件设备
        ├─ 遍历 device.list
        └─ IoManager.Instance.Open(deviceKey)
            ├─ GetUri(key) → 查 device.list
            ├─ Create(uri) → URI 匹配 → 具体设备驱动
            └─ session.Open() → PXI/COM/USB 连接
```

---

## 十一、可复用设计模式总结

### 1. 编码管道（Encode Pipeline）

```
物理值 (90.0°)
    │
    ├─ UnConvert
    │   逆缩放: raw = (physical - ScaleB) / ScaleA
    │   例: (90.0 - 0) / 0.000171661376953125 = 524288
    │
    └─ BitSet64.UnExtract
        位域写入: dataCache[Offset + StartBit/8] 的 StartBit%8 位置写入 BitWidth 位
```

### 2. 解码管道（Decode Pipeline）

```
原始数据 (byte[])
    │
    ├─ BitSet64.Extract
    │   从 dataCache 指定 Offset/StartBit/BitWidth 提取原始码
    │
    └─ IcdValue.Convert
        正向缩放: physical = ScaleA * raw + ScaleB
```

### 3. Tag 标记系统

| Tag值 | 枚举 | 用途 | 处理逻辑 |
|-------|------|------|---------|
| 40 | IcdTag.Head | 帧头 | Encode时填充固定值 |
| 48 | IcdTag.Length | 帧长度 | Encode时更新 |
| 49 | IcdTag.Count | 帧计数 | Encode时自增 |
| 50 | IcdTag.Sum | 累加和 | Encode时重算所有数据累加 |
| 51 | IcdTag.Sum2 | Sum变体 | 同上 |
| 52 | IcdTag.Sum3 | 跳过帧头的Sum | Encode时从指定偏移开始算 |
| 53 | IcdTag.XOR | 异或校验 | Encode时重算所有数据异或 |
| 54 | IcdTag.XOR1 | 跳过帧头的XOR | Encode时从 _offset_follow_head 开始算 |
| 55 | IcdTag.XOR2 | XOR变体 | 同上 |
| 60 | IcdTag.SignalInValue | 信号内嵌值 | 用 ValueTextList 做匹配 |

### 4. 帧类型

| Type值 | 枚举 | 方向 | 节点类型 | 读写性 |
|--------|------|------|---------|--------|
| 1 | iftData | 接收(UUT→激励) | IcdNode | Value只读(Decode) |
| 2 | iftCmd | 发送(激励→UUT) | IcdCmdNode | Value可写(Encode) |
| 4 | iftConfig | 配置 | IcdCmdNode | 设备配置参数 |
| 3 | iftDataCmd | 双向 | - | 混合模式 |

### 5. 可扩展转换器（IConvert + ConvertorFactory）

```csharp
// 通过插件机制加载自定义转换器
// 约定: 编译为 EphAutoTest.ICD.{name}.dll
// 运行时: Assembly.LoadFrom(dll) → 反射创建 → IConvert.Init(baseDir)
public interface IConvert {
    void Init(string baseDir);
    bool Convert(string name, int rawValue, out double physicalValue);
}
```

---

## 十二、工程注意事项

1. **ICD节点名完整包含产品前缀**：`ISI-01#A429_IN1(110) GNSS_Latitude`，CSV中存储省去产品前缀的版本 `A429_IN1(110) GNSS_Latitude`，运行时拼装
2. **设备匹配用子串匹配**：`nodeName.Contains("ISI-01#A429_IN1")`，而不是精确匹配，允许带Label/信号名的完整节点名也能命中
3. **帧名匹配用 EndsWith/StartsWith/Contains**：`IcdManager.Find` 中四种字符串匹配组合，容错性强
4. **每个Label一个XML文件**：ARINC 429 的每个 Label 独立定义一个 ICD XML 文件，帧长度固定为 4 字节（32bit A429 word）
5. **调试开关**：`_debug_sytax` 变量控制是否实际连接硬件，为 true 时只打印日志不操作设备
6. **多产品支持**：`Engine_WriteSignalRequired` 中对 `_products` 列表中每个产品分别发送信号
7. **多版本IcdConfig共存**：目录中有 `IcdConfig.xml`（机柜）、`IcdConfig - 便携系统.xml`、`IcdConfig-机柜系统自环回.xml` 等变体，通过不同的文件名切换部署方案
8. **A429通道与信号关系**：A429_IN1~IN4 对应 6272T_00~03（激励→UUT），A429_OUT 对应 6272T_00（UUT→激励，同通道但不同方向）
