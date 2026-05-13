# AD 硬件特点分析 — 基于恩菲特科技 EP-H 系列板卡 SDK

## 一、概述

本文件基于恩菲特科技 7 款 AD 采集板卡（EPH5022 / EPH5023A / EPH5024C / EPH5026 / EPH5035 / EPH5037 / EPH5039）的 PDF 文档、SDK 头文件和 API 参考手册，综合分析真实 AD 硬件的技术特点，为 `IADevicePlugin` 接口设计和插件封装提供参考依据。

---

## 二、板卡硬件规格总览

| 型号 | 通道数 | 分辨率 | 最高采样率 | 输入范围 | 耦合方式 | 特色功能 | 总线 |
|------|--------|--------|-----------|---------|---------|---------|------|
| EPH5022 | 8 SE/DIFF | 16bit | 128KSPS | ±50mV~±50V 10档 | AC/DC/ICP | ICP恒流源, DMA传输 | cPCI/PXI |
| EPH5023A | - | - | - | - | - | 与5022类似但更新 | PXI |
| EPH5024C | 4 | 14bit | 2.5MSPS | ±0.5V~±10V 6档 | AC/DC/ICP_AC/ICP_DC/GND_AC/GND_DC | FIFO模式, 抗混叠滤波器, PXI总线配置, 预触发 | cPCI/PXI |
| EPH5026 | - | 16bit | - | - | - | - | PXI |
| EPH5035 | - | - | - | - | - | - | PXI |
| EPH5037 | 16 SE / 8 DIFF | 16bit | 1MHz(扫描) | ±0.05V~±10V 4档(Gain 1/2/20/200) | DC(扫描采集) | 扫描表(4096深度), 数字IO(47ch), DA(2ch), FIFO, PXI总线 | cPCI/PXI |
| EPH5039 | 96 | 16bit | 1MHz | ±10V | DC | 扫描表(256), 校准数据, 中断FIFO回调, 设备枚举, PXI总线 | PXI |

> SE = 单端, DIFF = 差分。缺数据项因提取的 PDF 或 SDK 中未包含对应信息。

---

## 三、真实 AD 硬件的典型工作流程

### 3.1 强制采样（Continuous / Forced Sampling）

```
连接 → 复位 → 配置通道(量程/耦合/增益) → 设置采样频率 → 启动采样
→ 循环: 读结束地址 → 读数据 → 停止采样 → 关闭
```

典型代码（EPH5022）：
```c
Eph5022_AutoConnectToFirst(&cardnum);      // 连接
Eph5022_SetSampleFreq(cardnum, 6);          // 128KHz
Eph5022_SetChRange(cardnum, 0, 2);          // ±10V
Eph5022_SetChCouple(cardnum, 0, 1);         // DC耦合
Eph5022_StartStop(cardnum, 1);              // 启动采样
// 循环读取...
Eph5022_DMAReadVoltage(cardnum, addr, 0, 1024, voltage);
Eph5022_StartStop(cardnum, 0);              // 停止采样
Eph5022_Close(cardnum);                     // 关闭
```

### 3.2 触发采样（Triggered Sampling）

```
连接 → 复位 → 配置通道参数 → 设置采样频率 → 设置触发模式/长度
→ 触发允许 → 轮询采样状态 → 采样结束 → 读数据 → 禁止触发
```

典型代码（EPH5024C）：
```c
Eph5024C_SetTrgMode(cardnum, EP_TRG_MODE_SOFT);  // 软件触发
Eph5024C_SetTrgLen(cardnum, 0, 2048);              // 采样长度
Eph5024C_TrgEn(cardnum, 1);                         // 触发允许
// 轮询等待采样结束
while (status != EP_SAMP_STATUS_END)
    Eph5024C_GetSampleStatus(cardnum, &status);
Eph5024C_ReadEndAddr(cardnum, &endaddr);
Eph5024C_ReadChVoltageByEndAddr(cardnum, ch, endaddr, count, voltage);
Eph5024C_TrgEn(cardnum, 0);                         // 禁止触发
```

### 3.3 预触发采样（Pre-trigger Sampling）

EPH5024C 支持**预触发**（pre-trigger）模式：采集触发信号到来之前的数据。

```c
Eph5024C_SetTrgLen(cardnum, preLen, trgLen);  // preLen > 0 表示预采样
// 触发后实际采集长度 = preLen + trgLen
// Voltage[0..preLen-1]   = 预触发数据（触发前）
// Voltage[preLen..]      = 触发后数据
```

---

## 四、AD 硬件的核心特征

### 4.1 采样率

- **非连续可调**：所有板卡的采样率为固定档位（如 128K/64K/32K/16K/…Hz），通过寄存器索引设置
- **扫描时钟 vs 通道采样率**（EPH5037/5039）：扫描时钟频率 ÷ 扫描表深度 = 单通道采样率
- **高速差异**：EPH5024C 最高 2.5MSPS（14bit），EPH5022 最高 128KSPS（16bit）

### 4.2 通道配置

| 参数 | 描述 | 典型取值范围 |
|------|------|-------------|
| 量程 (Range) | 每通道独立设置 | ±50mV~±50V（EPH5022 10档），±10V（EPH5037 4档） |
| 耦合 (Coupling) | AC/DC 可选 | DC/AC/ICP_AC/ICP_DC/GND_AC/GND_DC |
| ICP 电流 | 传感器恒流源 | 2~20mA，每通道可设 |
| 输入模式 | 单端/差分 | 每通道可独立配置（EPH5037）|
| 增益 (Gain) | 程控增益放大器 | 1/2/20/200（EPH5037）|
| 滤波 | 抗混叠滤波器 | 4档带通（EPH5024C）|

### 4.3 触发模式

各板卡支持的触发模式：

| 触发模式 | EPH5022 | EPH5024C | EPH5037 | EPH5039 |
|----------|---------|----------|---------|---------|
| 软件触发 (Soft/CPU) | ✅ | ✅ | ✅ | ✅ |
| 外触发正沿 (ExtP) | ✅ | ✅ | ✅ | ✅ |
| 外触发负沿 (ExtN) | ✅ | ✅ | ✅ | ✅ |
| 系统触发正沿 (SysP) | ✅ | ✅ | ❌ | ❌ |
| 系统触发负沿 (SysN) | ✅ | ✅ | ❌ | ❌ |
| 星型触发正沿 (StarP) | ❌ | ✅ | ✅ | ✅ |
| 星型触发负沿 (StarN) | ❌ | ✅ | ✅ | ✅ |
| 内触发 (Inst) | ✅ | ✅ | ❌ | ❌ |

### 4.4 数据读取模式

| 模式 | 说明 | 适用板卡 |
|------|------|---------|
| 同步读取 | CPU 通过地址直接读存储器 | 所有 |
| DMA 读取 | 硬件 DMA 传输 | EPH5022, EPH5024C |
| MAP 读取 | 内存映射方式 | EPH5024C, EPH5037, EPH5039 |
| FIFO 读取 | 软件模拟 FIFO，自动维护地址指针 | EPH5024C, EPH5037, EPH5039 |
| 原始值读取 | 返回 ViInt16 未转换的 AD 码 | 所有 |

数据输出格式：
- **电压值** (double)：`电压 = 原始值 / 32768 × 量程`
- **原始值** (ViInt16)：AD 转换器的直接输出码，范围 -32768 ~ 32767

### 4.5 存储管理

- **环形缓冲**：每通道独立环形存储器（如 4Msa/ch for EPH5024C）
- **通道存储模式**：数据按通道号固定地址存放
- **扫描表存储模式**：数据按扫描表定义的通道顺序连续存放（EPH5037/5039）
- **结束地址机制**：`ReadEndAddr()` 返回最新写入地址，据此计算未读数据量

### 4.6 扫描表（Scan List）— EPH5037/5039 独有

```
扫描表是一个通道序列数组（最大 4096 项），定义了 ADC 轮询采集的顺序。
例: [CH0, CH1, CH2, CH3, GND, GND, CH2, GND]

扫描频率 200KHz, 扫描表深度 8
→ CH2 出现 2 次, 采样率 = 200KHz × 2/8 = 50KHz
→ CH0 出现 1 次, 采样率 = 200KHz × 1/8 = 25KHz
→ GND 插入可提高通道间隔高度
```

### 4.7 中断与回调

EPH5039 支持**中断 FIFO 回调**机制：
```c
// 注册中断回调函数，当 FIFO 数据就绪时自动调用
Eph5039_RegisterFunction(cardnum, &fifo_struct, 1);  // 1=注册
Eph5039_RegisterFunction(cardnum, &fifo_struct, 0);  // 0=注销
```

### 4.8 校准

EPH5037/5039 支持每通道校准数据读写：
```c
// 读取校准数据：单端 zero/gain, 差分 zero/gain
Eph5039_ReadCalibration(cardnum, SEChZero[4][96], SEChGain[4][96],
                         DIFFChZero[4][48], DIFFChGain[4][48]);
Eph5039_WriteCalibration(cardnum, flgWriteEEP, Gain,
                          SEChZero, SEChGain, DIFFChZero, DIFFChGain);
```

---

## 五、IADevicePlugin 接口设计要点

### 5.1 必须覆盖的核心操作

| 类别 | 操作 | 对应 IADevicePlugin | 备注 |
|------|------|---------------------|------|
| 连接 | AutoConnectToFirst / BusSlot / All | 由 PluginManager 管理 | 插件内封装 `openDevice()` |
| 复位 | Reset | startAcquisition() 前隐含 | |
| 采样率 | SetSampleFreq(index) | `setSampleRate(double)` | 需 Hz→index 映射 |
| 存储深度 | SetSampleLen(length_in_K) | `setSampleLength(int)` | 需 sample→K 转换 |
| 量程 | SetChRange(range_index) | `ADChannelConfig.range` | 需 index→double 映射 |
| 耦合 | SetChCouple(couple_index) | `ADCoupling` 枚举 | 需扩展为 6 种模式 |
| 触发模式 | SetTrgMode(mode) | `ADTriggerMode` 枚举 | 需补充 StarP/StarN |
| 触发电平 | SetChTrgLevel(voltage) | `ADChannelConfig.trigger_level` | ✅ |
| 触发沿 | SetChTrgEdge(edge) | `ADTriggerEdge` 枚举 | ✅ |
| 触发使能 | TrgEn(enable) | startAcquisition() 隐含 | |
| 软件触发 | CPUTrg() | `softwareTrigger()` | ✅ |
| 采样状态 | GetSampleStatus() | `sampleStatus()` | 状态枚举一致 |
| 数据读取 | ReadChVoltage / DMAReadVoltage | `readChannelData()` | ✅ |
| 全通道读取 | ReadAllChVoltage | `readAllChannelsData()` | ✅ |
| 停止 | StartStop(0) | `stopAcquisition()` | ✅ |
| 关闭 | Close | `closeDevice()` | ✅ |

### 5.2 必须补充的缺失功能

| 缺失功能 | 优先级 | 说明 |
|----------|--------|------|
| **原始数据读取** (ViInt16) | **高** | SDK 同时提供 double 电压值和 ViInt16 原始 AD 码。部分场景需要原始码计算。新增 `readChannelRaw(ch, count) → QVector<qint16>` |
| **预触发深度** | **高** | EPH5024C 设置预采样长度，用于捕获触发前的数据。`ADTriggerConfig` 追加 `pretriggerLength` |
| **FIFO 读取模式** | **高** | EPH5024C/5037 支持 FIFO 读取（自动维护地址指针）。新增 `setReadMode(enum: Direct/DMA/MAP/FIFO)` |
| **扫描列表模式** | **高** | EPH5037/5039 独有特性，非所有板卡支持。新增 `setScanList(QVector<int>)` / `setMemoryMode(Channel/Scan)` |
| **多板卡支持** | **高** | SDK 支持 `AutoConnectToAll` 同时连接多张板卡。`DeviceInfo` 需追加 bus/slot 信息 |
| **耦合模式扩展** | **中** | 从 DC/AC 扩展为 DC/AC/ICP_AC/ICP_DC/GND_AC/GND_DC 6 种 |
| **通道输入模式** | **中** | 单端/差分配置。`ADChannelConfig` 追加 `inputMode` |
| **通道增益** | **中** | EPH5037 四档可编程增益。`ADChannelConfig` 追加 `gain` |
| **抗混叠滤波器** | **中** | EPH5024C 4 档滤波。`ADChannelConfig` 追加 `filter` |
| **PXI 背板同步** | **低** | 时钟输出/触发输出/星型触发使能。由主程序统一管理 |
| **校准数据** | **低** | EPH5037/5039 支持。由插件内部管理 |
| **错误码转字符串** | **低** | SDK 提供 `StatusGetString()`。插件可统一封装 |

### 5.3 枚举扩展建议

```cpp
// 耦合模式——从 2 种扩展为 6 种
enum class ADCoupling {
  DC,         // 直流耦合
  AC,         // 交流耦合
  ICP_DC,     // ICP 直流耦合
  ICP_AC,     // ICP 交流耦合
  GND_DC,     // GND 直流耦合(自检用)
  GND_AC      // GND 交流耦合(自检用)
};

// 触发模式——补充星型触发
enum class ADTriggerMode {
  Software,
  ExternalPos,     // 外触发正沿
  ExternalNeg,     // 外触发负沿
  SystemPos,       // 系统触发正沿
  SystemNeg,       // 系统触发负沿
  StarPos,         // 星型触发正沿 (新增)
  StarNeg,         // 星型触发负沿 (新增)
  Internal         // 内触发
};

// 读取模式——用于不同传输方式
enum class ADReadMode {
  Direct,  // 普通寄存器读取
  DMA,     // DMA 传输
  MAP,     // 内存映射
  FIFO     // FIFO 方式
};
```

### 5.4 通道配置结构体扩展建议

```cpp
struct ADChannelConfig {
  double range = 10.0;                    // 量程 (V)
  ADCoupling coupling = ADCoupling::DC;   // 耦合方式
  double icp_current = 0.004;             // ICP 电流 (A)

  // === 新增字段 ===
  bool differential = false;              // true=差分, false=单端
  int gain = 1;                           // 可编程增益 (1/2/20/200)
  int filter = 0;                         // 抗混叠滤波器档位

  // 触发参数
  ADTriggerEdge trigger_edge = ADTriggerEdge::Rising;
  double trigger_level = 0.0;             // 触发电平 (V)
};
```

### 5.5 补充触发配置

```cpp
struct ADTriggerConfig {
  ADTriggerMode mode = ADTriggerMode::Software;
  bool enabled = true;                    // 触发使能

  // === 新增 ===
  int pretrigger_length = 0;              // 预触发采样点数（EPH5024C）
  int trigger_length = 1024;              // 触发后采样点数
};
```

### 5.6 新增接口建议

```cpp
// 在 IADevicePlugin 或插件配置层追加:

// ——— 读取模式 ———
virtual bool setReadMode(ADReadMode mode);
virtual ADReadMode readMode() const;

// ——— 扫描表模式（仅 5037/5039） ———
virtual bool setMemoryMode(ADMemoryMode mode);   // Channel / Scan
virtual ADMemoryMode memoryMode() const;
virtual bool setScanList(const QVector<int>& scanList);
virtual QVector<int> scanList() const;
virtual int maxScanDepth() const;                // 查询最大扫描表深度

// ——— 原始值读取 ———
virtual QVector<qint16> readChannelRaw(int channel, int count);
virtual QVector<qint16> readAllChannelsRaw(int count);

// ——— 设备标识增强 ———
// DeviceInfo 补充:
//   int bus_number;      // PXI 总线号
//   int slot_number;     // 设备号
//   int card_serial;     // 板卡序列号

// ——— 采样率映射 ———
// 插件内部需维护 Hz 值 ←→ 寄存器索引 的映射表
// setSampleRate(100000) 需转换为 setSampleFreq(6) (EPH5022)
```

### 5.7 数据流总结

```
用户层
  │
  │ setSampleRate(100000) / setChannelConfig(...) / startAcquisition()
  ▼
IADevicePlugin（抽象接口）
  │
  │ 内部：Hz→index 映射,  double→寄存器值 映射,  状态管理
  ▼
SDK API（C 动态库 .dll）
  │
  │ Eph5022_SetSampleFreq() / Eph5022_SetChRange() / Eph5022_StartStop()
  ▼
硬件寄存器（CPCI/PXI 总线）
  │
  │ FPGA 逻辑 → ADC 转换 → 存储器
  ▼
采样数据
  │
  │ Eph5022_ReadEndAddr() → Eph5022_ReadChVoltage() / ReadChHexData()
  ▼
double[] / ViInt16[] → 用户数据处理
```

---

## 六、与现有 IADevicePlugin 的差距总结

| 对比维度 | 当前 IADevicePlugin | 真实 SDK 要求 |
|----------|-------------------|--------------|
| 耦合模式 | DC/AC 2 种 | 6 种 (含 ICP/GND) |
| 触发模式 | 6 种 | 8 种 (含 StarP/StarN) |
| 采样率 | Hz 值 | 寄存器索引 (需映射) |
| 数据格式 | double 电压 | double 电压 + ViInt16 原始码 |
| 读取模式 | Direct 仅一种 | Direct/DMA/MAP/FIFO 四种 |
| 预触发 | 不支持 | 支持 (pretrigger_len) |
| 扫描表 | 不支持 | 支持 (最多 4096 项) |
| 存储模式 | 通道存储 | 通道存储 + 扫描表存储 |
| 通道增益 | 固定 | 可编程 1/2/20/200 |
| 输入模式 | 单端 | 单端 + 差分 (每通道可配) |
| 抗混叠滤波 | 不支持 | 4 档可选 |
| 校准 | 不支持 | 每通道 zero/gain 校准 |
| 中断回调 | 不支持 | 支持 (EPH5039) |
| 多板卡 | 不涉及 | 支持枚举和指定连接 |
| 错误码 | 无统一处理 | StatusGetString() 转换 |
