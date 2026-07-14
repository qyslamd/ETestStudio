# MockUUTSimulator 设计方案

> **版本**: v5.1
> **日期**: 2026-07-10
> **状态**: 设计完成（四轮评审通过）

## 1. 背景与问题

### 1.1 真实使用场景

ETest 平台的用户是测试工程师，他们在产线或实验室中使用真实的测试设备组合来完成对被测对象（UUT）的自动化测试。

**UUT 是什么样的？**

一个 UUT 不是单一协议类型的设备，它可能是一个复合系统，比如"飞行控制器"：

```
┌─────────────────────────────────────────┐
│  UUT: 飞行控制器                          │
│                                          │
│  ├── CAN 端口（CAN1）   ← 接 PXI-CAN卡   │
│  ├── A429 端口（A429_IN） ← 接 PXI-A429卡 │
│  ├── AD 采集端口（AD_CH） ← 接 PXI-AD卡   │
│  ├── IO 端口（IO_PORT）  ← 接 PXI-IO卡    │
│  └── 1553B 端口（MIL1553）← 接 PXI-1553B卡│
└─────────────────────────────────────────┘
```

每个端口既有"接收"方向（测试设备发给 UUT 的指令），也有"发送"方向（UUT 回复给测试设备的数据），两侧协议帧不同：

```
UUT.CAN1 ←── CAN 总线 ──→ PXI-CAN卡.ch1
  │ 设备→UUT：发送控制指令.eprotox （设备按此协议编码发送）
  │ UUT→设备：回传状态.eprotox     （设备按此协议解码接收）

UUT.A429_IN ←── A429 总线 ──→ PXI-A429卡.ch1
  │ 设备→UUT：发送Label150.eprotox
  │ UUT→设备：接收Label155.eprotox
```

**用户流程：**

```
① 接线：把 PXI 机箱的各板卡通过总线连到 UUT 的接口
② ETestStudio 中配拓扑、配协议、写测试程序
③ 点击运行 → 引擎加载真实驱动 → 走真实硬件 → 物理闭环
```

### 1.2 Mock 的必要性

真实闭环需要"硬件插件 + 物理连线 + UUT"三者齐全。缺任何一个物理上都不通：

```
物理链路：
  PXI串口卡 ——— RS232 线 ——— 5V导光板电源

缺任何一个：
  串口卡 ——— 断开的线          ← 信号没出过端口
  断开的线 ——— 电源             ← 信号没进过 UUT
```

所以只有两种状态：

| 状态 | 硬件插件 | 物理连线 | UUT | 运行方式 |
|------|---------|---------|-----|---------|
| **真实模式** | ✅ 真实驱动 | ✅ 物理接线 | ✅ 真实 UUT | 引擎直接操作硬件 |
| **Mock 模式** | ✅ Mock 插件 | ❌ 不需要 | ❌ 软件模拟 | 引擎走 Mock 链路 |

当用户需要 Mock 时，必然是硬件和 UUT 全部模拟——不存在"半实物"的物理前提。

**设计约束：拓扑中所有设备的 `mock` 值必须一致。** 禁止同一项目中混合 mock 设备和真实设备。

---

## 2. 拓扑数据模型

### 2.1 端口-协议绑定

每个端口（UUT 端口和设备端口）都有收/发两个方向，绑定不同的协议帧：

```json
// UUT 端口
{
  "name": "COM1",
  "direction": "bidirectional",
  "sendFrames": ["回传电流值"],       // UUT→设备
  "receiveFrames": ["发送电流设定值"]  // 设备→UUT
}

// 设备端口
{
  "name": "ch1",
  "direction": "bidirectional",
  "sendFrames": ["发送电流设定值"],    // 设备→UUT
  "receiveFrames": ["回传电流值"]     // UUT→设备
}
```

约束：`device.sendFrames == uut.receiveFrames`，`device.receiveFrames == uut.sendFrames`

### 2.2 拓扑 JSON 完整示例

```json
{
  "version": "2.0",
  "products": [
    {
      "name": "5V导光板电源",
      "ports": [
        {
          "name": "COM1",
          "direction": "bidirectional",
          "sendFrames": ["回传电流值"],
          "receiveFrames": ["发送电流设定值"]
        },
        {
          "name": "AD_CH1",
          "direction": "input",
          "sendFrames": [],
          "receiveFrames": ["回传电压值"]
        }
      ]
    }
  ],
  "devices": [
    {
      "id": "pxi-serial-1",
      "name": "PXI串口卡",
      "pluginId": "etest.plugin.device.serial_pxi",
      "type": "serial",
      "mock": false,
      "properties": [
        {"key": "slot_number", "value": "3"},
        {"key": "baud_rate", "value": "115200"}
      ],
      "ports": [
        {
          "name": "ch1",
          "sendFrames": ["发送电流设定值"],
          "receiveFrames": ["回传电流值"]
        }
      ]
    },
    {
      "id": "pxi-ad-1",
      "name": "PXI-AD采集卡",
      "pluginId": "etest.plugin.device.ad_pxi",
      "type": "ad",
      "mock": false,
      "ports": [
        {
          "name": "ch0",
          "channel": 0,
          "sendFrames": [],
          "receiveFrames": ["回传电压值"]
        }
      ]
    }
  ],
  "connections": [
    {
      "productName": "5V导光板电源",
      "portName": "COM1",
      "deviceName": "pxi-serial-1",
      "devicePort": "ch1"
    }
  ]
}
```

### 2.3 Mock 标记

`devices[]` 中的可选字段 `mock` 和 `type` 控制该设备的加载方式：

```json
{"id": "pxi-serial-1", "pluginId": "etest.plugin.device.serial_pxi",
 "type": "serial", "mock": true, "ports": [...]}
```

| 字段 | 说明 |
|------|------|
| `mock` | `false`（默认）→ 真实驱动；`true` → Mock 插件 |
| `type` | 设备类型标识（`"serial"` / `"can"` / `"a429"` / `"ad"` / `"da"`）。mock:true 时用于查找匹配的 Mock 插件 |

AD 设备端口需额外指定 `channel` 字段：

```json
{"name": "ch0", "channel": 0, "sendFrames": [], "receiveFrames": ["回传电压值"]}
```

---

## 3. 插件体系

### 3.1 插件元数据

```cpp
// PluginMetaData 新增字段
struct PluginMetaData {
  // ... 现有字段（device_type 已有） ...
  bool is_mock = false;  // 新增：标记此插件是 Mock 实现
};
```

### 3.2 Mock 插件声明

```cpp
// MockSerialPlugin.cpp
MockSerialPlugin::MockSerialPlugin() {
  meta_.device_type = "serial";
  meta_.is_mock = true;
}
MockCANPlugin::MockCANPlugin()       { meta_.is_mock = true; }
MockA429Plugin::MockA429Plugin()     { meta_.is_mock = true; }
MockADPlugin::MockADPlugin()         { meta_.is_mock = true; }
MockDAPlugin::MockDAPlugin()         { meta_.is_mock = true; }
```

### 3.3 PluginManager 新增方法

```cpp
// PluginManager.h
// 按 device_type 查询 Mock 插件或真实插件
QList<PluginMetaData> devicesByMockType(const QString& deviceType, bool mock) const;
```

### 3.4 引擎匹配逻辑

**安全约束：`mock` 值决定了插件搜索范围，严格互斥，永不 fallback。**

```
HardwareManager::instantiateDevice(deviceId, pluginId, properties, mock):

  type = properties.value("type").toString();  // 拓扑中的 type 字段

  if (mock) {
    // ── 只在 Mock 插件池中搜索 ──
    plugin = pm.plugin(pluginId);               // ① 直接匹配
    if (!plugin) {
      auto matches = pm.devicesByMockType(type, true);  // ② 按 type 查 Mock
      if (!matches.isEmpty())
        plugin = pm.plugin(matches.first().id);
    }
    if (!plugin) {
      emit deviceError(deviceId, "Mock 插件未找到，请检查 plugins/ 目录");
      return false;  // ← 硬错误
    }

  } else {
    // ── 只在真实插件池中搜索 ──
    plugin = pm.plugin(pluginId);               // ① 直接匹配
    if (!plugin) {
      auto matches = pm.devicesByMockType(type, false);  // ② 按 type 查真实
      if (!matches.isEmpty())
        plugin = pm.plugin(matches.first().id);
    }
    if (!plugin) {
      emit deviceError(deviceId, "真实设备插件未找到，请检查硬件驱动是否已安装");
      return false;  // ← 硬错误
    }
  }
```

| 场景 | mock=false | mock=true |
|------|-----------|----------|
| 真实插件未安装 | ❌ 报错，提示安装驱动 | ⛔ 不搜 |
| Mock 插件未安装 | ⛔ 不搜 | ❌ 报错，提示检查 plugins/ |
| 两者都安装了 | ✅ 加载真实插件 | ✅ 加载 Mock 插件 |
| 两者都没安装 | ❌ 报错 | ❌ 报错 |

---

## 4. MockUUT 建造者模式

### 4.1 核心思路

MockUUT 是复合体，不是简单的一个 JSON 查表器。Builder 根据**拓扑模型 + ICD 协议定义**，动态组装出一个完整的 MockUUT 实例——它不知道"我在模拟"，它只知道"我是这个 UUT"。

```
拓扑 JSON
  │
  │ products[].ports[].sendFrames/receiveFrames
  │ connections[] 连线关系
  ▼
MockUUTBuilder
  │
  │ ① 遍历 products[]，找到每个 UUT
  │ ② 遍历 UUT 的 ports[]，根据 direction 和绑定的协议帧
  │ ③ 为每个端口创建对应的 PortSimulator（注入 device_id）
  │ ④ 注入 JSON 配置的行为（回复内容、通道值）
  ▼
MockUUT 实例
  ├── SerialPortSimulator(COM1)     ← 处理串口协议收发
  │     device_id: "pxi-serial-1"
  │     ├── receiveFrames: [发送电流设定值.eprotox]
  │     └── sendFrames: [回传电流值.eprotox]
  │
  ├── ADChannelSimulator(AD_CH1)   ← 处理 AD 通道值
  │     device_id: "pxi-ad-1"
  │     └── 绑定帧：回传电压值.eprotox
  │
  └── ...（CAN、A429、1553B、IO 同理）
```

### 4.2 为什么用 Builder

| 对比 | 传统查表 | Builder 组装的 MockUUT |
|------|---------|----------------------|
| 配置对象 | JSON 配 "frameId→response" | **UUT 实例**，有端口、有协议、有行为 |
| 复合 UUT 支持 | 需要手动配所有帧 | Builder 从拓扑自动发现所有端口 |
| 新增协议类型 | 改配置格式 + 改代码 | 拓扑加端口 → Builder 自动创建对应模拟器 |
| 跨信号联动 | 无法表达 | 端口模拟器间通过 Builder 注入依赖 |
| 与拓扑一致性 | 人工维护（容易不一致） | **拓扑即定义**，Mock 行为自然跟随 |

### 4.3 MockTypes.h

```cpp
// src/engine/MockTypes.h
#pragma once

#include <QByteArray>

namespace etest::engine {

// Mock 端口模拟器的回复描述（onFrameReceived 的返回值）
struct MockResponse {
    int targetFrameId;      // CAN ID / A429 Label / Serial 忽略
    QByteArray data;        // 回复的原始字节
};

} // namespace etest::engine
```

### 4.4 Builder 接口

```cpp
namespace etest::engine {

// ── 帧型端口模拟器基类（Serial、CAN、A429） ──
class FramePortSimulator {
public:
    virtual ~FramePortSimulator() = default;
    // 收到帧 → 返回模拟回复。无配置时返回 std::nullopt
    virtual std::optional<MockResponse> onFrameReceived(
        int frameId, const QByteArray& frameData) = 0;

    bool hasReceiveFrame(int frameId) const;
    const QString& deviceId() const { return device_id_; }

protected:
    QString device_id_;                     // 绑定的设备 ID（Builder 注入）
    QVector<int> receive_frame_ids_;
    QVector<int> send_frame_ids_;
    QHash<int, QByteArray> response_config_;
};

// ── 串口端口模拟器 ──
class SerialPortSimulator : public FramePortSimulator {
public:
    SerialPortSimulator(const QString& portName,
                         const QString& deviceId,
                         const QVector<int>& receiveFrameIds,
                         const QVector<int>& sendFrameIds);
    std::optional<MockResponse> onFrameReceived(
        int frameId, const QByteArray& frameData) override;
    void setResponseConfig(int frameId, const QByteArray& responseHex);
private:
    QString port_name_;
};

// ── CAN 端口模拟器 ──
class CANPortSimulator : public FramePortSimulator {
public:
    CANPortSimulator(const QString& portName,
                      const QString& deviceId,
                      const QVector<int>& receiveFrameIds,
                      const QVector<int>& sendFrameIds);
    std::optional<MockResponse> onFrameReceived(
        int frameId, const QByteArray& frameData) override;
    void setResponseConfig(int frameId, const QByteArray& responseHex);
private:
    QString port_name_;
};

// ── A429 端口模拟器 ──
class A429PortSimulator : public FramePortSimulator {
public:
    A429PortSimulator(const QString& portName,
                       const QString& deviceId,
                       const QVector<int>& receiveFrameIds,
                       const QVector<int>& sendFrameIds);
    std::optional<MockResponse> onFrameReceived(
        int frameId, const QByteArray& frameData) override;
    void setResponseConfig(int frameId, const QByteArray& responseHex);
private:
    QString port_name_;
};

// ── 通道型端口模拟器基类（AD、DA） ──
class ChannelPortSimulator {
public:
    virtual ~ChannelPortSimulator() = default;
    virtual double readChannelValue(int channel) = 0;
    int frameId() const { return frame_id_; }
    const QString& deviceId() const { return device_id_; }

protected:
    QString device_id_;
    int frame_id_ = 0;
};

// ── AD 通道模拟器 ──
class ADChannelSimulator : public ChannelPortSimulator {
public:
    ADChannelSimulator(const QString& deviceId, int frameId, int channel);
    double readChannelValue(int channel) override;
    void setFixedValue(double value);
private:
    int channel_;
    double fixed_value_ = 0.0;
};

// ── MockUUT — 对应一个 UUT（product） ──
class MockUUT {
public:
    const QString& name() const { return name_; }

    void addFrameSimulator(std::unique_ptr<FramePortSimulator> sim);
    void addChannelSimulator(std::unique_ptr<ChannelPortSimulator> sim);

    // 在自身模拟器中查找，同时匹配 deviceId 和 frameId
    FramePortSimulator* findFrameSimulator(
        const QString& deviceId, int frameId) const;
    ChannelPortSimulator* findChannelSimulator(int frameId) const;

    // 收到设备发来的帧 → 返回模拟回复。无配置时返回 std::nullopt
    std::optional<MockResponse> onFrameWritten(
        const QString& deviceId, int frameId, const QByteArray& frameData);

private:
    QString name_;
    QVector<std::unique_ptr<FramePortSimulator>> frame_sims_;
    QVector<std::unique_ptr<ChannelPortSimulator>> channel_sims_;
};

// ── Builder — 从拓扑 + ICD 组装 MockUUT ──
class MockUUTBuilder {
public:
    MockUUTBuilder(icd::Repository* icdRepo, const QJsonObject& topologyDoc);

    // 返回 false 表示构建失败（帧名解析失败等硬错误），此时 out 为空
    // 调用 lastError() 获取错误详情
    bool buildAll(QVector<std::unique_ptr<MockUUT>>& out);
    QString lastError() const;

    // 加载响应配置。filePath 是 mock/MockResponses.json 的完整路径
    // 必须在 buildAll() 之前调用
    void loadResponseConfigFile(const QString& filePath);

private:
    bool buildSingleUUT(const QJsonObject& product,
                        std::unique_ptr<MockUUT>& out);
    std::unique_ptr<FramePortSimulator> buildFrameSimulator(
        const QJsonObject& port,
        const QJsonObject& connection,
        const QJsonObject& device);
    std::unique_ptr<ChannelPortSimulator> buildChannelSimulator(
        const QJsonObject& port,
        const QJsonObject& connection,
        const QJsonObject& device);
    // 将帧名解析为帧 ID，解析失败返回 false
    bool resolveFrameNamesToIds(const QStringList& frameNames,
                                QVector<int>& outIds);

    icd::Repository* icd_repo_;
    QJsonObject topology_doc_;
    QJsonArray frame_responses_;
    QString last_error_;
};

} // namespace etest::engine
```

### 4.5 运行时的数据流

```
StepRunner::execSet
  → codec_->encodeToFrame(1.5) → 8字节帧
  → hw_->writeFrame(signal, frame)

     ┌─── HardwareManager ──────────────────────────────┐
     │  device_pool_ 查到 deviceId → 是 mock 设备       │
     │  ① mock_serial->writeData(frame)                 │
     │     ← 写进 rx_buffer_（环回保障基本读写）          │
     │                                                   │
     │  ② if mock_uut_:                                 │
     │     resp = mock_uut_->onFrameWritten(id, frame)  │
     │     if resp:                                     │
     │      ③ writeResponseViaPlugin(resp, signalType) │
     │         → serial->writeData(resp.data)            │
     │         或 can->sendMessage(resp.targetFrameId)   │
     │         或 a429->sendLabel(resp.targetFrameId)    │
     └───────────────────────────────────────────────────┘
```

### 4.6 配置接口

```json
{
  "version": "1.0",
  "portBehaviors": [
    {
      "productName": "5V导光板电源",
      "deviceId": "pxi-serial-1",
      "port": "COM1",
      "direction": "receive",
      "responses": [
        {
          "frameId": 0,
          "description": "收到电流设定帧后回复",
          "responseHex": "01 06 40 22 00 96 AA BB"
        }
      ]
    },
    {
      "productName": "5V导光板电源",
      "deviceId": "pxi-ad-1",
      "port": "AD_CH1",
      "fixedValue": 5.0,
      "description": "模拟 AD 通道采集到 5V"
    }
  ]
}
```

| 字段 | 说明 |
|------|------|
| `productName` | 限定所属 UUT（多 UUT 场景消除歧义） |
| `deviceId` | 限定所属设备（通过 deviceId 反查端口连接） |
| `port` | 端口名 |
| `direction` | 可选，帧型端口区分收/发方向 |

---

## 5. Mock 模式完整数据流

### 场景定义

以下流转以 A429 复合 UUT 为例：

**拓扑定义：**

```
UUT.A429_IN1   ←── A429 总线 ──→   PXI-A429卡.ch1（mock: true）
  发送帧：接收Label155                 发送帧：发送Label150
  接收帧：发送Label150                 接收帧：接收Label155
```

**测试程序步骤：**

```
SET    uuid:7a3f...  值=1.5    // 设备按"发送Label150"协议编码，frameId=150
CHECK  uuid:9b2d...  值=1.5    // 设备按"接收Label155"协议解码，frameId=155
```

**Mock 配置：**

```json
{
  "portBehaviors": [
    {
      "productName": "飞行控制器",
      "deviceId": "mock-a429-1",
      "port": "A429_IN1",
      "responses": [
        {
          "frameId": 150,
          "description": "收 Label150 后回复 Label155 帧",
          "responseHex": "00 00 00 88"
        }
      ]
    }
  ]
}
```

### 5.1 启动加载

```
TestExecutionEngine::loadTopology("test.etopo")
  │
  ├── ① 解析 JSON → root（一次解析，分发给以下步骤）
  │
  ├── ② 校验 mock 一致性
  │     → 遍历 devices[]，检查所有 mock 值是否一致
  │     → 不一致 → 报错并返回 false
  │
  ├── ③ hw_manager_->loadFromTopology(root)
  │     → 遍历 devices[] → 找到 mock:true, type:"a429" 的设备
  │     → PluginManager.devicesByMockType("a429", true) → MockA429Plugin
  │     → mock_a429->openDevice()  ✅
  │     → 存入 device_pool_["mock-a429-1"]
  │
  ├── ④ 检测到有 mock 设备 → 创建 MockUUTBuilder
  │     → if !builder.buildAll(mockUUTs):
  │         → hw_manager_->closeAllDevices()   // 回滚已打开的设备
  │         → return false
  │
  │     → Builder.buildAll() 内部
  │        → 遍历 products[]
  │        → 找到 UUT.A429_IN1 端口、连接、设备 A429卡
  │        → resolveFrameNamesToIds(["发送Label150"]) → [150]
  │        → resolveFrameNamesToIds(["接收Label155"]) → [155]
  │          ├── 任一解析失败 → buildAll 返回 false
  │        → 创建 A429PortSimulator("A429_IN1", "mock-a429-1", [150], [155])
  │        → 创建 ADChannelSimulator("pxi-ad-1", frameId, channel) ...
  │
  ├── ⑤ builder.loadResponseConfigFile("mock/MockResponses.json")
  │     → 找到 productName + deviceId + frameId=150
  │     → responseHex → 注入 A429PortSimulator
  │
  └── ⑥ hw_manager_->setMockUUT(std::move(mockUUTs))
        → mock_uuts_ 持有所有 MockUUT 实例
```

### 5.2 执行 SET

```
StepRunner::execSet(value=1.5, signal)
  │
  ├── signal 结构：
  │     deviceId  = "mock-a429-1"
  │     frameId   = 150
  │     signalType = A429
  │
  ├── codec_->encodeToFrame(1.5, signal)
  │     ↑ 用 ICD 中"发送Label150"协议编码
  │     → 4 字节帧 [0x00, 0x00, 0x01, 0x00]
  │
  └── hw_->writeFrame(signal, frameData)

    ┌─── HardwareManager::writeFrame ─────────────────────────────┐
    │ ① deviceId="mock-a429-1" → device_pool_ → MockA429Plugin   │
    │    → a429->sendLabel(150, [0x00,0x00,0x01,0x00])           │
    │      ↳ MockA429Plugin::sendLabel(150, data)                │
    │        → label_data_[150] = data  ← 存起来                  │
    │                                                             │
    │ ② Mock 回复                                                │
    │    resp = mock_uut_->onFrameWritten(                        │
    │              "mock-a429-1", 150, frameData)                 │
    │                                                             │
    │    ┌─── MockUUT::onFrameWritten ───────────────────────┐    │
    │    │ → findFrameSimulator("mock-a429-1", 150)          │    │
    │    │ → A429PortSimulator                               │    │
    │    │   device_id == "mock-a429-1" ✓                    │    │
    │    │   150 ∈ receiveFrameIds ✓                         │    │
    │    │ → sim->onFrameReceived(150, frameData)            │    │
    │    │   → 查 response_config → [0x00,0x00,0x00,0x88]   │    │
    │    │   → MockResponse{targetFrameId=155, data=...}     │    │
    │    │   → return to HardwareManager                     │    │
    │    └────────────────────────────────────────────────────┘    │
    │                                                             │
    │ ③ if resp:                                                 │
    │    → a429->sendLabel(155, [0x00,0x00,0x00,0x88])          │
    │      ↳ MockA429Plugin::sendLabel(155, data)                │
    │        → label_data_[155] = data  ← UUT 回复存好           │
    │                                                             │
    │ ④ return true                                               │
    └─────────────────────────────────────────────────────────────┘

    → SET 返回 PASS ✅
```

**关键理解：MockUUT 通过返回值将数据回流给 HardwareManager。**

```
          设备（MockA429Plugin）              UUT（MockUUT）
          ┌──────────────────┐            ┌─────────────────────┐
          │  sendLabel(150)  │─────帧────→│ onFrameReceived     │
          │                  │            │  (frameId=150, data)│
          │                  │            │                     │
          │  sendFrames:[150]│            │  receiveFrames:[150]│
          │                  │            │                     │
          │  receiveFrames:  │            │  sendFrames:[155]   │
          │    [155]         │            │                     │
          │                  │            │  查配置 → 返回      │
          │  HW 写回:        │←─返回──────│  MockResponse{      │
          │ sendLabel(155,   │            │    targetFrameId=155│
          │  responseBytes)  │            │    data=response    │
          │                  │            │  }                  │
          │ label_data_[155] │            └─────────────────────┘
          │   = responseBytes│
          └──────────────────┘
```

**关键理解：**

```
HardwareManager 收到 MockUUT::onFrameWritten 的返回值后，
根据 signal.signalType 写回对应的插件接口：
  SERIAL → serial->writeData(resp.data)
  CAN    → can->sendMessage(resp.targetFrameId, resp.data)
  A429   → a429->sendLabel(resp.targetFrameId, resp.data)
```

### 5.3 执行 CHECK

```
StepRunner::execCheck(value=1.5, signal)
  │
  ├── signal 结构：
  │     deviceId  = "mock-a429-1"
  │     frameId   = 155
  │     signalType = A429
  │
  └── hw_->read(signal)

    ┌─── HardwareManager::read ──────────────────────────────────┐
    │ ① deviceId="mock-a429-1" → device_pool_ → MockA429Plugin  │
    │                                                             │
    │ ② a429->receiveLabel(155)                                  │
    │    ↳ MockA429Plugin::receiveLabel(155)                      │
    │      → label_data_.value(155) → [0x00,0x00,0x00,0x88]     │
    │      → 返回 QByteArray([0x00,0x00,0x00,0x88])              │
    │                                                             │
    │ ③ return QVariant(QByteArray([0x00,0x00,0x00,0x88]))       │
    └─────────────────────────────────────────────────────────────┘

    → codec_->decodeFromFrame(data, signal)
      ↑ 用 ICD 中"接收Label155"协议解码
      → 解出工程值 1.5A

    → |1.5 - 1.5| ≤ 0.01 → PASS ✅
```

### 5.4 无 Mock 配置时的行为（环回模式）

当 `MockResponses.json` 不存在或没有匹配的配置时，Mock 流程退化为**环回模式**：

```
SET 1.5A:
  → codec_->encodeToFrame(1.5) → 帧 [..., 0x0096, ...]
  → hw_->writeFrame(signal, frame)
     → mock_serial->writeData(frame)          ← rx_buffer_ 有数据了
     → mock_uut_->onFrameWritten("mock-a429-1", 150)
       → findFrameSimulator("mock-a429-1", 150)
       → 没有配置 → 什么也不做

CHECK:
  → hw_->read(signal)
     → 如果 CHECK 的 frameId 跟 SET 一样（150）：
       → mock_serial->readData() → 返回环回的帧 → 解码 → PASS ✅
     → 如果 CHECK 的 frameId 不同（155）：
       → mock_a429->receiveLabel(155) → label_data_ 空
       → 返回 QByteArray() → 解码出 0 → 可能 FAIL
```

**结论：环回模式只保证同一 frameId 的收发验证。跨 frameId 的收发（如 A429 Label150→Label155）必须配置 MockResponses.json。**

### 5.5 AD 通道读取流程

AD 与帧型设备不同——没有"写→读"的对应关系，是纯读取的。但 AD 通道在拓扑中绑定了协议帧（`receiveFrames`），该帧在 ICD 中有 frameId——所以 AD 通道也通过 frameId 统一查找。

```
StepRunner::execCheck(value=5.0, signal)
  │
  ├── signal 结构：
  │     deviceId  = "pxi-ad-1"
  │     frameId   = 1             // 来自绑定的"回传电压值"帧
  │     channel   = 0
  │     signalType = AD
  │
  └── hw_->read(signal)

    ┌─── HardwareManager::read ──────────────────────────────────┐
    │ ① 有 mock_uuts_ → 遍历 MockUUT                             │
    │    → uut->findChannelSimulator(signal.frameId)              │
    │    → ADChannelSimulator(deviceId="pxi-ad-1", frameId=1)     │
    │                                                             │
    │ ② sim->readChannelValue(signal.channel)                   │
    │    → 返回 5.0（不经过 MockADPlugin）                        │
    │                                                             │
    │ ③ return QVariant(5.0)                                     │
    └─────────────────────────────────────────────────────────────┘

    → codec_->decode(5.0, signal) → 5.0V
    → |5.0 - 5.0| ≤ 0.01 → PASS ✅
```

**没有配置时**：ADChannelSimulator 返回 0.0（默认值），CHECK 会失败——用户必须为 AD 通道配 fixedValue。

### 5.6 DA 通道写入流程

DA 是帧型设备的反向——只写不回，回读其实是读最后写的值。MockDA 插件内部已自洽：

```
StepRunner::execSet(value=10.0, signal)
  → signal: deviceId=mock-da-1, channel=0, type=DA
  → codec_->encode(10.0, signal) → raw=10.0
  → hw_->write(signal, 10.0)
     → MockDAPlugin::writeChannel(0, 10.0)
       → channel_values_[0] = 10.0
     → return true → PASS ✅

（不需要 MockUUT 介入，DA 自身维护 channel_values_）

StepRunner::execCheck(value=10.0, signal)
  → hw_->read(signal)
     → if mock_uuts_ present:
       → uut->findChannelSimulator(frameId)
       → ADChannelSimulator 在 DA 模式下回读 channel_values_
       → 返回 10.0
  → |10.0 - 10.0| ≤ 0.01 → PASS ✅
```

---

## 6. HardwareManager 集成

### 6.1 新增成员

```cpp
class HardwareManager {
    void setMockUUT(QVector<std::unique_ptr<MockUUT>> uuts);

    struct DeviceEntry {
        IDevicePlugin* plugin = nullptr;
        DeviceStatus status = DeviceStatus::Offline;
        bool is_mock = false;
    };

private:
    MockUUT* findMockUUTForFrame(const QString& deviceId, int frameId) const;
    QVector<MockUUT*> mock_uuts_;  // Builder 产出
};
```

### 6.2 instantiateDevice

```cpp
bool HardwareManager::instantiateDevice(const QString& deviceId,
                                        const QString& pluginId,
                                        const QVariantMap& properties,
                                        bool mock) {
    PluginManager& pm = PluginManager::instance();
    IPlugin* plugin = nullptr;
    QString type = properties.value("type").toString();

    if (mock) {
        plugin = pm.plugin(pluginId);
        if (!plugin) {
            auto matches = pm.devicesByMockType(type, true);
            if (!matches.isEmpty())
                plugin = pm.plugin(matches.first().id);
        }
        if (!plugin) return false;  // 硬错误
    } else {
        plugin = pm.plugin(pluginId);
        if (!plugin) {
            auto matches = pm.devicesByMockType(type, false);
            if (!matches.isEmpty())
                plugin = pm.plugin(matches.first().id);
        }
        if (!plugin) return false;  // 硬错误
    }

    auto* devPlugin = dynamic_cast<IDevicePlugin*>(plugin);
    if (!devPlugin->openDevice()) return false;

    device_pool_.insert(deviceId,
                        {devPlugin, DeviceStatus::Online, mock});
    return true;
}
```

> 注：`loadFromTopology` 需将 `devices[].type` 字段注入到 properties map 中，供 `instantiateDevice` 取用。

### 6.3 writeFrame——MockUUT 返回回复值

```cpp
bool HardwareManager::writeFrame(const ResolvedSignal& signal,
                                  const QByteArray& frameData) {
    IDevicePlugin* dev = pluginForDevice(signal.deviceId);
    if (!dev) return false;

    // ── ① 先写设备（环回保障基本读写） ──
    switch (signal.signalType) {
    case SignalType::SERIAL:
        dynamic_cast<ISerialDevicePlugin*>(dev)->writeData(frameData);
        break;
    case SignalType::CAN:
        dynamic_cast<ICANPlugin*>(dev)->sendMessage(signal.frameId, frameData);
        break;
    case SignalType::A429:
        dynamic_cast<IArinc429Plugin*>(dev)->sendLabel(
            static_cast<int>(signal.frameId), frameData);
        break;
    default:
        return false;
    }

    // ── ② Mock 模式：查 MockUUT 获取模拟回复 ──
    if (auto* mockUUT = findMockUUTForFrame(signal.deviceId,
                                              signal.frameId)) {
        auto resp = mockUUT->onFrameWritten(
            signal.deviceId, signal.frameId, frameData);
        if (resp) {
            // ③ 把回复帧写入设备插件（Mock 插件退化为内存操作）
            switch (signal.signalType) {
            case SignalType::SERIAL:
                dynamic_cast<ISerialDevicePlugin*>(dev)->writeData(resp->data);
                break;
            case SignalType::CAN:
                dynamic_cast<ICANPlugin*>(dev)->sendMessage(
                    static_cast<quint32>(resp->targetFrameId), resp->data);
                break;
            case SignalType::A429:
                dynamic_cast<IArinc429Plugin*>(dev)->sendLabel(
                    resp->targetFrameId, resp->data);
                break;
            default:
                break;
            }
        }
    }

    return true;
}

MockUUT* HardwareManager::findMockUUTForFrame(const QString& deviceId,
                                                int frameId) const {
    for (auto* uut : mock_uuts_) {
        if (uut->findFrameSimulator(deviceId, frameId))
            return uut;
    }
    return nullptr;
}
```

`findMockUUTForFrame` 使用 `deviceId` 参数精确定位 UUT，
防止多 UUT 场景下 frameId 冲突导致的跨 UUT 误匹配。

### 6.4 read——AD/DA 通道走 MockUUT

```cpp
QVariant HardwareManager::read(const ResolvedSignal& signal) {
    // AD/DA 通道，查 MockUUT 的通道模拟器（按 frameId 查找）
    if (!mock_uuts_.isEmpty() &&
        (signal.signalType == SignalType::AD ||
         signal.signalType == SignalType::DA)) {
        for (auto* uut : mock_uuts_) {
            auto* sim = uut->findChannelSimulator(signal.frameId);
            if (sim) {
                double val = sim->readChannelValue(signal.channel);
                return QVariant(val);
            }
        }
    }

    // 读设备
    IDevicePlugin* dev = pluginForDevice(signal.deviceId);
    // ... 现有 switch 分发 ...
}
```

> 条件 `!mock_uuts_.isEmpty()` 隐含前提：mock 和真实设备在拓扑中互斥。
> 有 MockUUT 时所有 AD/DA 走模拟器；无 MockUUT 时走真实设备。

---

## 7. TestExecutionEngine 集成

```cpp
#include "MockUUTBuilder.h"

bool TestExecutionEngine::loadTopology(const QString& etopoPath) {
    // 一次解析 JSON，分发给 loadFromTopology 和 Builder
    QFile file(etopoPath);
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();

    // ── 校验 mock 一致性 ──
    if (!checkMockConsistency(root)) {
        emit error("拓扑中所有设备的 mock 值必须一致，不能混合 mock 和真实设备");
        return false;
    }

    // 加载所有设备
    bool ok = hw_manager_->loadFromTopology(root);

    // 有 Mock 设备 → 创建 MockUUT
    if (hasMockDevices(root)) {
        MockUUTBuilder builder(icd_repository_, root);
        QFileInfo fi(etopoPath);

        // 加载响应配置
        builder.loadResponseConfigFile(
            fi.absolutePath() + QStringLiteral("/mock/MockResponses.json"));

        // 构建 MockUUT（帧名解析失败等硬错误时回滚）
        QVector<std::unique_ptr<MockUUT>> mockUUTs;
        if (!builder.buildAll(mockUUTs)) {
            emit error(QStringLiteral("MockUUT 构建失败: %1")
                           .arg(builder.lastError()));
            hw_manager_->closeAllDevices();  // 回滚已打开的设备
            return false;
        }

        hw_manager_->setMockUUT(std::move(mockUUTs));
    }

    return ok;
}

static bool hasMockDevices(const QJsonObject& root) {
    for (const auto& d : root["devices"].toArray()) {
        if (d.toObject()["mock"].toBool())
            return true;
    }
    return false;
}

static bool checkMockConsistency(const QJsonObject& root) {
    auto devices = root["devices"].toArray();
    if (devices.isEmpty()) return true;

    bool firstMock = devices[0].toObject()["mock"].toBool();
    for (const auto& d : devices) {
        if (d.toObject()["mock"].toBool() != firstMock)
            return false;
    }
    return true;
}
```

---

## 8. 真实模式 vs Mock 模式的数据流

### 真实模式

```
拓扑加载 → HardwareManager 打开真实 PXI 板卡

SET 1.5A:
  → codec_->encodeToFrame(1.5) → 8字节帧
  → hw_->writeFrame(signal, frame)
     → serial_pxi->writeData(frame)       // 物理 RS232 发出去
     → UUT 物理回复

CHECK 1.5A:
  → hw_->read(signal)
     → serial_pxi->readData()             // 物理读取 UUT 回复
  → codec_->decodeFromFrame(data) → 1.5A → PASS ✅
```

### Mock 模式

```
拓扑加载 → HardwareManager 打开 Mock 插件
         → Builder 组装 MockUUT（含 SerialPortSimulator + ADChannelSimulator...）

无 MockResponses.json（环回模式，验证编解码链路）：
  SET 1.5A:
    → codec_->encodeToFrame(1.5) → 8字节帧
    → hw_->writeFrame(signal, frame)
       → mock_serial->writeData(frame)           // rx_buffer_
  CHECK 1.5A:
    → hw_->read(signal)
       → mock_serial->readData()                 // 取刚写的环回
    → codec_->decodeFromFrame(data) → 1.5A → PASS ✅

有 MockResponses.json（模拟 UUT 回复）：
  SET 1.5A:
    → codec_->encodeToFrame(1.5) → 8字节帧
    → hw_->writeFrame(signal, frame)
       → mock_serial->writeData(frame)           // rx_buffer_
       → MockUUT.onFrameWritten(frameId, frame)
          → SerialPortSimulator 查配置 → 找到回复
          → mock_serial->writeData(response)     // 回复帧也塞 rx_buffer_
  CHECK 1.5A:
    → hw_->read(signal)
       → mock_serial->readData()                 // 取回复帧
    → codec_->decodeFromFrame(data) → 回复值 → PASS ✅
```

---

## 9. 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/engine/MockTypes.h` | **新建** | MockResponse 公共类型 |
| `src/engine/MockUUTBuilder.h` | **新建** | Builder + MockUUT + PortSimulator 子类 |
| `src/engine/MockUUTBuilder.cpp` | **新建** | Builder 实现 + PortSimulator 实现 |
| `src/engine/HardwareManager.h` | 修改 | 加 mock_uuts_ + findMockUUTForFrame(deviceId, frameId) |
| `src/engine/HardwareManager.cpp` | 修改 | instantiateDevice 加 mock+type，writeFrame 加 MockResponse 处理 |
| `src/engine/TestExecutionEngine.h` | 修改 | 无改动 |
| `src/engine/TestExecutionEngine.cpp` | 修改 | loadTopology 一次解析 + Builder + mock 一致性校验 |
| `src/engine/CMakeLists.txt` | 修改 | 加新文件 |
| `src/core/plugin_sdk/PluginMetaData.h` | 修改 | 加 is_mock 字段 |
| `src/core/plugin_sdk/PluginManager.h/.cpp` | 修改 | 加 devicesByMockType(type, mock) |
| `examples/plugins/mock_*/` | 修改 | 各 Mock 插件加 is_mock = true |
| `mock/MockResponses.json` | **新建** | 示例配置文件（含 productName + deviceId） |

---

## 10. 未纳入 Phase 1 的内容

| 特性 | 说明 |
|------|------|
| 跨信号联动 | 串口收到 SET → AD 回采值联动变化（需要 PortSimulator 间注入依赖） |
| 动态回复 | 提取写入帧的字段值填充到回复帧而非固定 hex |
| 波形数据 | AD 返回预设波形而非固定值 |
| 数字 IO | 需要先定义 `IDigitalIOPlugin` 接口 |
| 1553B | 需要先定义 `IMil1553BusPlugin` 接口 |
