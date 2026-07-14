# AD插件接口设计

## 一、背景

为了设计一个通用且可扩展的 `IADevicePlugin` 接口，我们分析了三张来自 ENPHT 公司的真实 AD 采集卡 SDK，以确保接口设计既能覆盖现有硬件的共性，又能为未来的不同硬件留出扩展空间。

## 二、三张AD采集卡对比分析

### 2.1 硬件规格对比

| 特性 | EPH5022 | EPH5035 | EPH5039 |
|------|---------|---------|---------|
| **设备ID** | 0x5022 | 0x5035 | 0x5039 |
| **通道数** | 8通道 | 64通道 | 96通道(单端) / 48通道(差分) |
| **最大采样率** | 128KSPS | 100KHz | 1MHz |
| **分辨率** | 16位 | 未明确(16位推断) | 未明确 |
| **触发模式** | 6种 (不含星形触发) | 8种 (含星形触发) | 7种 + FIFO_INFO |
| **总线类型** | PXI (PCI 9054) | PXI (PCI 9054) | PXI (PCI 9054/8311) |
| **SDK版本** | V1.38 (2024.10) | V1.06 (2024.06) | V1.02 (2024.12) |
| **SDK文件** | eph5022_32.dll (77KB) | EPH5035.dll (86KB) | Eph5039.dll (138KB) |

### 2.2 SDK API对比

| 功能分类 | EPH5022 | EPH5035 | EPH5039 |
|----------|---------|---------|---------|
| **连接** | AutoConnectToFirst, AutoConnectToBusSlot, AutoConnectToAll | AutoConnectToFirst, AutoConnectToBusSlot | AutoConnectToFirst, AutoConnectToBusSlot, GetDevices |
| **关闭** | Close | Close | Close |
| **复位** | Reset | Reset | Reset |
| **设备信息** | GetManuID, GetDevID | GetManuId, GetDeviceId, GetVersion, GetModuleInf | GetManuID, GetDevID, GetRevision, GetCardID, GetDevCfgPara, GetIDN |
| **采样率设置** | SetSampleFreq (索引值 6+n) | SetSampleFreq (值6~31对应频率表) | SetSampFreq (离散档位0~4) |
| **采样长度** | SetSampleLen (1K~4096K) | SetTrgLen (2~1048576点) | 通过存储深度配置 |
| **通道配置** | **SetChRange / SetChCouple / SetChICP_CCP** | **SetChPara / SetChInputMode / SetChGain** | **SetChConfig (inputMode/gain/bias)** |
| **触发模式** | TrgMode (6种) | SetTrgMode (8种) | SetTrgMode (7种+FIFO) |
| **触发边沿** | TrgEdge | SetChTrgEdge | 无独立函数 |
| **触发电平** | TrgLevel | SetChTrgLevel | 无独立函数 |
| **启停采集** | StartStop | StartStop | StartStop |
| **CPU触发** | CPUTrg | CPUTrg | CPUTrg |
| **采样状态** | GetSampleStatus | GetSampleStatus | GetSampStatus |
| **单通道电压读取** | SynchroReadVoltage / DMAReadVoltage | ReadChData / ReadChDataByEndAddr | ReadVoltage / ReadVoltageByEndaddr |
| **所有通道电压读取** | SynchroReadAllChVoltage / DMAReadAllChVoltage | ReadAllChData / ReadAllChDataByEndAddr | ReadAllVoltage |
| **平均值读取** | ReadCVT (8通道平均) | ReadCVT (64通道平均) | 无独立函数 |
| **FIFO模式** | 无 | FIFO_ReadChData | FIFO_ReadChVoltage |
| **扫描列表** | 无 | 无 | SetScanList (最多256项) |
| **数据传输模式** | 无 | 无 | SetDataTransMode (普通/MAP/中断DMA/块DMA) |

### 2.3 共性分析 (所有三张卡都具备的API)

1. **连接管理**: `AutoConnectToFirst` / `Close`
2. **设备复位**: `Reset`
3. **设备信息**: `GetManuID` / `GetDevID`
4. **采样率设置**: `SetSampleFreq` (具体取值不同，但概念一致)
5. **触发模式**: `SetTrgMode` (具体模式数量不同)
6. **启停采集**: `StartStop`
7. **CPU触发**: `CPUTrg`
8. **采样状态**: `GetSampleStatus`
9. **数据读取**: 单通道电压读取 + 所有通道电压读取
10. **通道配置**: 每张卡都有每通道的参数设置 (但参数内容完全不同)

### 2.4 关键差异 (影响接口设计的核心点)

#### 差异一：每通道参数完全不同

| 卡片 | 每通道参数 |
|------|-----------|
| EPH5022 | 量程(range)、耦合方式(couple)、ICP供电 |
| EPH5035 | 输入模式(单端/差分)、信号类型(电压/电流)、增益、触发沿、触发电平 |
| EPH5039 | 输入模式(单端/差分)、增益、偏置 |

**结论**: 无法设计一个通用的结构体来容纳所有可能的硬件参数，硬件配置必须交由具体插件自行管理。

#### 差异二：采样率设置方式不同

| 卡片 | 设置方式 | 取值范围 |
|------|---------|---------|
| EPH5022 | 索引值 n | 6 ~ 31 (n=6→128K, n+1→减半) |
| EPH5035 | 频率档位值 | 6 ~ 31 (每个值对应固定频率) |
| EPH5039 | 离散模式值 | 0=1MHz, 1=500KHz, 2=250KHz, ... |

**结论**: 接口层以 `double` (Hz) 传入，插件内部自行映射到硬件的取值。

#### 差异三：数据读取模式不同

| 卡片 | 读取方式 |
|------|---------|
| EPH5022 | 同步读 / DMA读 |
| EPH5035 | 普通读 / 按结束地址读 / FIFO读 |
| EPH5039 | 普通读 / FIFO读 / Scan List读 |

**结论**: 接口层只定义最基础的 `readChannel()` 和 `readAllChannels()`，具体的读取策略（同步/DMA/FIFO）由插件内部实现和优化。

## 三、接口设计决策

### 3.1 设计原则

1. **最小通用接口**: `IADevicePlugin` 只定义所有AD采集卡都具备的行为
2. **硬件参数下沉**: 每通道的硬件配置参数由具体插件自行定义和管理
3. **读取方式封装**: 底层读取策略（同步/DMA/FIFO）对上层透明
4. **保持向后兼容**: 模拟设备插件不受影响
5. **遵循现有框架**: 继承 `IDevicePlugin`，与 `PluginManager` / `HardwareTreeWidget` 无缝集成

### 3.2 现有接口分析 (IADevicePlugin.h)

当前 `IADevicePlugin.h` 中的 `ADChannelConfig`:

```cpp
struct ADChannelConfig {
  WaveformType waveform = WaveformType::Sine;  // 信号仿真参数
  double frequency = 1.0;       // 仅仿真用
  double amplitude = 1.0;       // 仅仿真用
  double offset = 0.0;          // 仅仿真用
  double noise_level = 0.05;    // 仅仿真用
};
```

上述结构体实际描述的是**信号仿真/信号源**参数，而非真实 AD 采集的硬件参数。三张真实AD卡的对比验证了：**硬件参数放入通用接口不可行**（因为每张卡的硬件参数完全不同）。

### 3.3 最终接口设计

#### ADChannelConfig（通道配置）

```cpp
// 信号波形（仿真/模拟设备用）
enum class WaveformType {
  Sine,
  Square,
  Triangle,
  DC
};

// 仿真参数子结构
struct ADSimulationConfig {
  WaveformType waveform = WaveformType::Sine;
  double frequency = 1.0;       // Hz
  double amplitude = 1.0;       // 归一化幅值 [0, 1]
  double offset = 0.0;          // 归一化偏移 [-1, 1]
  double noise_level = 0.05;    // 噪声幅度 [0, 1]
};

// 通道配置（仅包含通用字段 + 仿真参数）
struct ADChannelConfig {
  QString label;                // 通道标签
  ADSimulationConfig simulation; // 仿真参数（模拟设备用）
  // 硬件参数: 由具体插件自行定义和提供API管理
};
```

#### IADevicePlugin（AD设备插件接口）

```cpp
class IADevicePlugin : public IDevicePlugin {
 public:
  ~IADevicePlugin() override = default;

  // === 数据读取 ===
  // 读取指定通道的最新电压值
  virtual double readChannel(int channel) = 0;
  // 读取所有通道的最新电压值，数组长度等于通道数
  virtual QVector<double> readAllChannels() = 0;

  // === 采集控制 ===
  virtual bool startAcquisition() = 0;
  virtual void stopAcquisition() = 0;
  virtual bool isAcquiring() const = 0;

  // === 采样率（Hz，插件内部映射到硬件取值） ===
  virtual bool setSampleRate(double rate) = 0;
  virtual double sampleRate() const = 0;

  // === 通道仿真参数配置（仅影响模拟设备） ===
  virtual bool setChannelConfig(int channel, const ADChannelConfig& config) = 0;
  virtual ADChannelConfig channelConfig(int channel) const = 0;
};
```

#### IDevicePlugin（设备基类接口，已有）

```cpp
class IDevicePlugin : public IPlugin {
 public:
  ~IDevicePlugin() override = default;

  virtual bool openDevice() = 0;        // 打开设备（连接硬件）
  virtual void closeDevice() = 0;       // 关闭设备（断开连接）
  virtual DeviceInfo deviceInfo() const = 0;  // 设备信息
  virtual DeviceStatus deviceStatus() const = 0;  // 设备状态
};
```

### 3.4 PluginMetaData 配置示例

通过 `PluginMetaData` 的已有字段来区分不同AD卡：

```json
{
  "id": "etest.plugin.device.ad.enpht_ep5022",
  "name": "ENPHT EP-H5022 AD采集卡",
  "category": "device",
  "device_type": "ad",
  "device_channels": 8
}
```

`HardwareTreeWidget` 通过 `device_type == "ad"` 筛选出所有AD插件，按厂家分组展示。

### 3.5 接口继承层次

```
IPlugin
  └── IDevicePlugin (openDevice/closeDevice/deviceInfo/deviceStatus)
        └── IADevicePlugin (readChannel/readAllChannels/startAcquisition/...)
              ├── 模拟AD设备插件
              ├── EnphtEP5022Plugin (8通道)
              ├── EnphtEP5035Plugin (64通道)
              └── EnphtEP5039Plugin (96通道)
```

## 四、EP-H5022 AD插件设计

### 4.1 设计要点

| 方面 | 方式 |
|------|------|
| **继承接口** | `IADevicePlugin` |
| **DLL加载** | `QLibrary` 动态加载 `eph5022_32.dll`，不编译时链接 |
| **硬件参数API** | 插件自定义公共方法，不走通用接口 |
| **采集线程** | 后台线程循环读取 + 信号通知UI线程 |
| **最新数据缓存** | `readChannel()` 和 `readAllChannels()` 从缓存返回最新值 |

### 4.2 EP-H5022硬件参数管理

EPH5022特有的硬件参数通过插件自有API暴露，不加入 `IADevicePlugin`：

```cpp
class EnphtEP5022Plugin : public QObject, public IADevicePlugin {
  // ... IADevicePlugin 接口实现 ...

  // ========== EPH5022 专用硬件配置 API ==========

  // 量程设置（每通道独立）
  bool setChRange(int channel, double range);  // → Eph5022_SetChRange
  double chRange(int channel) const;            // → Eph5022_GetChRange

  // 耦合方式设置（每通道独立）
  bool setChCouple(int channel, bool dcCouple); // → Eph5022_SetChCouple
  bool chCouple(int channel) const;              // → Eph5022_GetChCouple

  // ICP供电设置（每通道独立）
  bool setChICP(int channel, double current);   // → Eph5022_SetChICP_CCP
  double chICP(int channel) const;

  // 采样长度（设备级）
  bool setSampleLength(int points);              // → Eph5022_SetSampleLen
  int sampleLength() const;

  // 触发模式（设备级）
  bool setTriggerMode(int mode);                 // → Eph5022_TrgMode
  int triggerMode() const;

  // 触发边沿（内触发时，每通道）
  bool setTriggerEdge(int channel, int edge);    // → Eph5022_TrgEdge
  int triggerEdge(int channel) const;

  // 触发电平（内触发时，每通道）
  bool setTriggerLevel(int channel, double level); // → Eph5022_TrgLevel
  double triggerLevel(int channel) const;
};
```

### 4.3 设备生命周期与采集流程

```
initialize()
  └── 扫描系统，获取EPH5022板卡信息
  └── PluginEventBus::instance().subscribe(...)

start()
  └── 启动后台状态轮询线程

openDevice()
  └── Eph5022_AutoConnectToFirst(&cardnum)
  └── Eph5022_Reset()
  └── 填充 DeviceInfo

startAcquisition()
  └── 配置采样参数（频率/长度/触发等）
  └── Eph5022_TrgEn(1)
  └── Eph5022_StartStop(1)
  └── 启动采集线程

采集线程循环:
  └── Eph5022_GetSampleStatus() → 等待采样结束
  └── Eph5022_ReadEndAddr(&endAddr)
  └── Eph5022_SynchroReadAllChVoltage(endAddr, count, data)
  └── 写入最新数据缓存 latest_data_
  └── emit dataReady(latest_data_)  // 信号通知UI
  └── Eph5022_CPUTrg()  // 触发下一次采集

stopAcquisition()
  └── Eph5022_StartStop(0)
  └── 停止采集线程

closeDevice()
  └── stopAcquisition()
  └── Eph5022_Close()

stop()
  └── closeDevice()
  └── 停止状态轮询

uninitialize()
  └── 释放资源
  └── PluginEventBus::instance().unsubscribe(...)
```

`readChannel()` 和 `readAllChannels()` 直接返回缓存数据，不直接操作硬件：

```cpp
double readChannel(int channel) override {
  if (channel < 0 || channel >= device_channels_) return 0;
  return latest_data_.value(channel, 0.0);
}

QVector<double> readAllChannels() override {
  return latest_data_;
}
```

### 4.4 DLL封装层（EnphtEP5022Device）

负责动态加载 `eph5022_32.dll`，定义所有函数指针类型：

```cpp
class EnphtEP5022Device {
 public:
  bool load();   // QLibrary::load("eph5022_32.dll")
  void unload();

  // 函数指针: 对应每个 eph5022_32.dll 导出函数
  using Func_AutoConnect = ViStatus (*)(ViUInt32*);
  using Func_Close = ViStatus (*)(ViUInt32);
  using Func_Reset = ViStatus (*)(ViUInt32);
  using Func_SetChRange = ViStatus (*)(ViUInt32, ViUInt16, ViUInt16);
  using Func_SynchroReadAllChVoltage = ViStatus (*)(ViUInt32, ViUInt32, ViUInt32, double[]);
  // ... 其余函数指针

 private:
  QLibrary dll_;
  QMap<QString, void*> functions_;  // 缓存已加载的函数指针
};
```

## 五、文件清单

### 5.1 修改的文件

| 文件 | 变更 |
|------|------|
| `src/core/plugin_sdk/IADevicePlugin.h` | `ADChannelConfig` 移除 `WaveformType` 等字段，改为嵌套的 `ADSimulationConfig`；接口声明微调 |

### 5.2 新增的文件

| 文件 | 职责 |
|------|------|
| `src/plugins/ad/enpht_ep5022/EnphtEP5022Plugin.h` | 插件主类，实现 `IADevicePlugin` |
| `src/plugins/ad/enpht_ep5022/EnphtEP5022Plugin.cpp` | 插件主类实现，包含采集线程 |
| `src/plugins/ad/enpht_ep5022/EnphtEP5022Device.h` | `eph5022_32.dll` 的 `QLibrary` 封装 |
| `src/plugins/ad/enpht_ep5022/EnphtEP5022Device.cpp` | DLL函数指针加载和调用 |
| `src/plugins/ad/enpht_ep5022/CMakeLists.txt` | 插件构建配置 |
| `src/plugins/ad/enpht_ep5022/enpht_ep5022.json` | 插件元数据（`Q_PLUGIN_METADATA`） |
| `src/plugins/ad/enpht_ep5022/sdk/eph5022_32.h` | SDK头文件（从发布包拷贝） |
| `src/plugins/ad/enpht_ep5022/sdk/visatype.h` | VISA类型定义（从发布包拷贝） |

### 5.3 运行时依赖

| 文件 | 来源 | 部署位置 |
|------|------|---------|
| `eph5022_32.dll` | ENPHT发布包 | `plugins/` 目录或系统PATH |

## 六、构建集成

插件编译为 `MODULE` 动态库，输出到 `build/ninja-debug/plugins/`：

```cmake
# src/plugins/ad/enpht_ep5022/CMakeLists.txt
add_library(enpht_ep5022 MODULE
    EnphtEP5022Plugin.h EnphtEP5022Plugin.cpp
    EnphtEP5022Device.h EnphtEP5022Device.cpp
    enpht_ep5022.json
)

target_include_directories(enpht_ep5022 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk
)

target_link_libraries(enpht_ep5022 PRIVATE
    etest_core
    Qt5::Core
)

set_target_properties(enpht_ep5022 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
```

`PluginManager` 默认搜索 `${appDir}/plugins/`，插件编译到此目录后会被自动发现和加载，**无需修改 PluginManager 代码**。

## 七、插件注册与UI集成

### 7.1 Qt插件元数据

```cpp
// EnphtEP5022Plugin.h
class EnphtEP5022Plugin : public QObject, public IADevicePlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "etest.core.plugin.IPlugin/1.0" FILE "enpht_ep5022.json")
  Q_INTERFACES(etest::core::plugin::IPlugin)
  // ...
};
```

### 7.2 HardwareTreeWidget 中的展示

插件被加载后，`HardwareTreeWidget::refreshTree()` 自动发现并展示：

```
北京中科恩普特
  └── AD采集
        └── ENPHT EP-H5022 AD采集卡 [离线]  ← 双击切换为 [在线]
```

用户双击设备节点 → `IDevicePlugin::openDevice()` → 连接硬件。
右键菜单提供`打开设备/关闭设备/刷新`操作。
状态定时器每2秒刷新 `DeviceStatus`。

## 八、未纳入本次设计的内容

以下内容属于后续阶段，本文档不做设计：

1. **PluginEventBus**: 插件事件总线（已在 `docs/03-开发/1.3-通用插件框架开发内容.md` 中规划）
2. **PluginConfig**: 插件配置持久化（已在 `docs/03-开发/1.3-通用插件框架开发内容.md` 中规划）
3. **AD采集数据存储与回放**: 后续阶段实现
4. **EPH5035 / EPH5039 插件**: 后续按需实现
