# AD 监听器连续波形数据链路打通方案

## 问题陈述

加载 mock 项目后，切到运行页，勾选 AD 监听器通道，点运行，可视化区看不到连续正弦波。

## 现状分析

### 数据通路已完整

经过代码核实，`StepRunner` 的三个硬件操作方法都已经发射 `hardwareOperationFinished` 信号：

| 方法 | 行号 | 发射信号 |
|------|------|---------|
| `execSet()` | 第 273 行 | ✅ `emit hardwareOperationFinished(deviceId, portName, rawFrame, rawValue, engValue)` |
| `execCheck()` | 第 312 行 | ✅ 同上 |
| `execVerify()` | 第 374 行 | ✅ 同上 |

所以监视器数据通路已经完整：

```
测试步骤 Step(AD 通道)
  → StepRunner::execCheck/execVerify
  → HardwareManager::read() → MockADPlugin::readChannel() → 返回当前值
  → emit hardwareOperationFinished(deviceId, "ch{N}", ...)
  → MonitorManager::onHardwareOpFinished()
  → lookup_table_[(deviceId, "ch{N}")] → 匹配 tap(monitorIndex, channelIndex)
  → subscribers_[(monitorIndex, channelIndex)] → 回调
  → visualizer::onSampleCaptured() → 波形更新
```

**当前看不到波形的原因**：another_mock 项目的测试程序没有对 AD 通道做循环 VERIFY，或者测试步骤根本没触发 AD 读取。

### 关键数据格式对照

| 概念 | 拓扑 / ICD | StepRunner 发射时 |
|------|-----------|-----------------|
| 设备 ID | `tap.deviceId` | `signal.deviceId` |
| 端口名 | `tap.devicePort` (如 "ch0") | `signal.portName`（由 ICD 解析得到） |
| 工程值 | 期望值 | `step.value`（SET）/ `actual`（CHECK/VERIFY） |

**只要 `signal.portName` 和拓扑 tap 中 `devicePort` 一致，MonitorManager 就能匹配上。** 这是 tap 匹配的核心前提。

## 方案

**不需要修改 StepRunner / MonitorManager / MockADPlugin / VisualizationArea。** 只需要写一个测试程序，对 AD 通道做 LOOP + VERIFY。

### 测试程序结构

```
测试套件: "AD 波形演示"
  └── 用例: "AD 通道 0 循环采集"
        └── 前置步骤: 启动AD采集
              └── 步骤: SET AD启动采集 (target=启动寄存器)
        └── 循环 512 次:
              └── VERIFY AD通道0 (target=ch0信号ID, 期望值=任意, 容差=极大)
              └── (或 CHECK AD通道0, 同上)
```

每次 VERIFY 产生一个数据点 → MonitorManager → visualizer。512 次拼接出连续正弦波。

### 前置条件：AD 设备采集必须运行

MockADPlugin 的数据来自 `onAcquisitionTick()` 定时器，该定时器在 `startAcquisition()` 调用后才启动。因此测试程序需要先有一步让 AD 设备开始采集：

```
步骤1: SET (target=AD启动采集的控制信号, value=1)
  → resolver 解析为 startAcquisition 调用
  → MockADPlugin::startAcquisition() → 定时器开始 → 正弦波写入 ring buffer
```

或者引擎加载设备后自动调用 `startAcquisition()`（需要确认现有加载流程是否支持）。

### 需要确认的单点

1. **AD 启动采集**：引擎加载设备后是否自动 `startAcquisition()`？还是需要测试程序显式触发？如果自动启动则无需额外步骤。
2. **signal.portName 与 tap.devicePort 匹配**：需要确认 ICD 解析出来的 `portName` 和拓扑 tap 中配置的 `devicePort` 一致（比如都是 "ch0"）。不一致则 MonitorManager 匹配不上。
3. **mock 项目文件完整性**：另一个 mock 项目的 JSON 文件存在编码问题，可能需要修复后才能正常加载。

### 验证方式

1. 编写一个简单的演示测试程序（LOOP 512 × VERIFY AD ch0）
2. 用 mock 项目加载，配置好监听器 tap
3. 切到运行页 → 勾选 AD 通道 → 点运行
4. 可视化区看到正弦波

### 如果在用 mock 项目中无法验证

可能需要直接新建一个最小 mock 项目，包含：
- 一个 AD 设备（`mock: true`）
- 一个监听器，tap 挂在该 AD 设备的 ch0
- 一个测试程序，LOOP + VERIFY AD 通道
- 对应 ICD 信号定义

## 审查发现的问题

### 阻塞: MockUUT 路由拦截了 AD 数据路径

当项目使用 `mock: true` 设备时, `HardwareManager::read()` 优先走 MockUUT 路径:

```
HardwareManager::read() -> ADChannelSimulator::readChannelValue() -> fixed_value_ (默认 0.0)
```

而非 `MockADPlugin::readChannel()`. 所以 LOOP VERIFY 读到的永远是 0.0.

### 决策: ADChannelSimulator 产生正弦波

**方案(已采纳并已实现)**: 让 `ADChannelSimulator::readChannelValue()` 在未设置固定值时生成正弦波.

改动:
- `MockUUTBuilder.h` - 新增 `sample_counter_` 成员 + `kFrequency` / `kAmplitude` 常量
- `MockUUTBuilder.cpp` - `readChannelValue()` 改为: 有固定值则返回固定值(来自 MockResponses.json), 否则递增计数器产生 `kAmplitude * sin(2pi * kFrequency * t)` 正弦波
- 模拟 1kHz 采样率, 50Hz 正弦波, 幅值 +/-5V
- 每调用一次 `readChannelValue()`, 采样计数器 +1, 相位自然推进

**优点**:
- 不改变数据通路管线, MockUUT 路径不变
- `setFixedValue()` 仍可用(来自 MockResponses.json 配置时优先)
- 测试程序不需要特殊步骤, LOOP VERIFY 直接读到动态正弦波
