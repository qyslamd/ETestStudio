# HAL 接口设计方案 — 讨论记录

**日期**: 2026-05-13
**背景**: 基于 `IATP_设计方案.md`（六层架构）和 `使用场景分析.md`（10 类恩菲特硬件的真实 API 场景），讨论 HAL 层插件接口如何承载全部使用场景。

---

## 关键结论

### 1. 抽象层级不动

三级抽象足够，不需要新增层级：

```
IPlugin (生命周期)
  └── IDevicePlugin (设备通用: open/close/info/status)
      ├── IADevicePlugin
      ├── IDADevicePlugin
      ├── ICANPlugin
      ├── IArinc429Plugin
      ├── ISerialDevicePlugin
      └── (待新建) IDioPlugin, IPulsePlugin, IVisaPlugin,
           IFlexRayPlugin, I1394BPlugin
```

### 2. IDevicePlugin 加一个能力查询

```cpp
class IDevicePlugin : public IPlugin {
  // ... 现有方法不变 ...
  virtual QStringList capabilities() const = 0;
};
```

插件返回能力标签列表，如 `["fifo", "waveform", "trigger", "scan_list"]`。
调用方先查能力再调用，避免不支持的方法返回 false 造成歧义。

### 3. 每个设备接口按使用场景补全

不再分 Basic/Advanced 两层接口，不做能力接口多重继承，不做 `executeCommand` 逃生口。

**原则**: `使用场景分析.md` 里某类设备的 6 个场景用了什么能力，接口就写什么方法。不支持的方法对应的能力不在 `capabilities()` 列表中即可，调用方自行判断。

### 4. 补齐缺失的 5 个接口

从 0 新建：

| 接口 | 对应设备章节 |
|---|---|
| `IDioPlugin` | IO (DIO) |
| `IPulsePlugin` | IO (DIO) — 脉冲测量/PWM 输出单独接口 |
| `IVisaPlugin` | (使用场景分析中暂未分析 VISA，待后续补充) |
| `IFlexRayPlugin` | FlexRay |
| `I1394BPlugin` | 1394B (AS5643) |

---

## 讨论过程中否决的方案

| 方案 | 否决原因 |
|---|---|
| **C: `executeCommand()` 万能逃生口** | 违反 ISP 和 DIP；编译期类型安全丢失，运行时字符串拼错风险高 |
| **B 变体: Basic/Advanced 两层设备接口** | 调用方需要 `qobject_cast` 两次，实用拧巴 |
| **能力接口多重继承（IFifoCapability 等 Mixin）** | Qt 多重虚继承的 `qobject_cast` 链路需要验证，增加复杂度 |
| **一刀切将所有能力写到一个接口，不支持返回 false** | 给调用方错误期望，不如 `capabilities()` 显式声明 |
| **B: 能力接口式** | 概念正确但 C++ 实现复杂度高，当前阶段不必追求纯 SOLID |

---

## 现有接口覆盖度快照

| 接口 | 版本 | 当前方法数 | 覆盖度 | 要做的事 |
|---|---|---|---|---|
| `IADevicePlugin` | v3.0 | ~20 | ~90% | 基本不用动 |
| `IDADevicePlugin` | v1.0 | 2 | ~25% | 按 DA 章节扩（波形/FIFO/电流/批量） |
| `ICANPlugin` | v1.0 | 4 | ~75% | 补位级滤波、多卡触发同步 |
| `ISerialDevicePlugin` | v1.0 | 6 | ~35% | 补多通道/FIFO/中断/定时发送/RS485方向 |
| `IArinc429Plugin` | v1.0 | 4 | ~30% | 补 Scheduled/Label过滤/FIFO/触发同步 |
| `IDioPlugin` | ❌ 无 | — | 0% | 新建（TTL/PWM测量/高电压分拆设计） |
| `IPulsePlugin` | ❌ 无 | — | 0% | 新建（频率/脉宽/计数/正交编码） |
| `IVisaPlugin` | ❌ 无 | — | 0% | 新建（SCPI命令/自检/仪器管理） |
| `IFlexRayPlugin` | ❌ 无 | — | 0% | 新建 |
| `I1394BPlugin` | ❌ 无 | — | 0% | 新建 |

---

## 下一步

1. 确定每个接口的完整方法清单（按 `使用场景分析.md` 逐类提取）
2. 按清单逐个补齐/新建接口文件
3. 实现具体插件（从恩菲特设备开始）
