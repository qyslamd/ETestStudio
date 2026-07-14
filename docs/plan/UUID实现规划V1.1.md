# UUID 信号映射实现规划 V1.1

> **来源**：`docs/thinking/IATP_设计方案_精简版.md` 第 4 章（ICD 信号层）、第 7 章（数据流）
> **目标**：在 test_program / ICD / topology 三个独立子系统之间建立桥梁，实现"信号 UUID → (device, port, frame, node)"四元组映射。
> **版本**：V1.1
> **状态**：待评审
>
> **V1.1 变更要点**（相对 V1.0，含评审修订）：
> - UUID 的 device 维度改用**持久 id**（`TopologyDevice::id`），不用 `name`，解决多台同型号设备 UUID 碰撞 + 设备改名失效
> - 修正 SD1 自相矛盾：诚实区分"哪些改名不影响 UUID / 哪些会失效"
> - 明确 `nodePath` 定义、格式、生成算法（新增 3.7 节）
> - 明确 `QString ↔ std::string` 转换边界，`icd_utility` 不引入 Qt（新增 3.8 节）
> - 补 Layer 3 前置阶段 M0：先让 `StepTableWidget.target` 列编辑走 `ISignalSelection`（当前未被任何代码调用）
> - 明确 `linkTo` 废弃，UUID 是其升级（新增 3.9 节）
> - 修正 `TopologyJsonSerializer` 序列化行号、删除文档末尾混入的"我超威😁"
> - **V1.1 评审修订**：
>   - SignalRegistry 补全索引填充路径（`registerDevice` + `synchronizeFrames`），修复 `resolve()` / `findByNode` 不可用缺口
>   - 修正分层错误：ICD 感知的信号选择实现放 etest_app，抽名为 `IcdSignalSelection`
>   - 补全 `TopologyDocument` 缺少 `devicePortAdded/Removed` 信号的问题
>   - 补充 TopologyConnection 设备改名连带影响说明
>   - 补充 `documentCleared` 处理、`findDeviceById` 方法、老文件迁移持久化时机
>   - 补充与帧协议编辑器设计文档的 linkTo 协调说明
>   - 修正 SD5 碰撞概率表述精度

---

## 1 背景与现状

### 1.1 设计目标（来自 IATP 方案）

**写路径**：
```
Lua: SetDevice("温度", 37.5)
  → Engine.setSignal("sig-uuid", 37.5)
    → ICD查UUID映射(device+channel+protocol)   ← 当前缺失
    → FaultManager 检查 → ProtocolRegistry.pack
    → 传输通道 → 硬件
```

**读路径**：
```
HAL 收到原始字节 → Pub 到 DataPool
  → ICD Subscribe → unpack
    → SignalValueCache (CVT) → Engine.getSignal()
    → Pub 到 UI 监控面板
```

**关键设计决策 AD5**：信号用 UUID 标识，改名不导致引用失效。

### 1.2 当前代码现状（三孤岛）

```
┌─ TEST PROGRAM (.etprog) ─────────────────────────────┐
│ TestStepData.target = QString                         │  自由文本，无校验
│ ISignalSelection (抽象接口)                            │  已定义但未被调用
│                                                       │  DefaultSignalSelection =
│                                                       │  QInputDialog::getText
└──────────────────────────────────────────────────────┘
         ↓  无关联
┌─ ICD/PROTOCOL (icd_utility) ─────────────────────────┐
│ Repository → Frame → Node (signal)                    │  Node 无 UUID
│ 识别方式: frame_name + node_path                       │  名字可能重名
│ NodeAttrs.link_to (非必填, 形如 "\DA0\ch0")           │  硬件路径可选，放错层
│ Node 无 path() 方法                                    │  需遍历 parent 链
└──────────────────────────────────────────────────────┘
         ↓  无关联
┌─ TOPOLOGY (.etopo) ───────────────────────────────────┐
│ TopologyDevice (name + deviceType + pluginId + ports) │  有 pluginId 但缺实例 id
│ TopologyConnection (device:port ↔ product:port)       │  无 Frame 绑定
└──────────────────────────────────────────────────────┘
```

### 1.3 关键问题清单

| # | 问题 | 现状 |
|---|------|------|
| Q1 | 测试步骤如何引用 ICD 信号？ | `target` 是自由字符串，无注册表/校验 |
| Q2 | 信号唯一标识是什么？ | `frame_name + node_path`，无 UUID，重名会冲突 |
| Q3 | 设备端口对应哪个 ICD 帧？ | 拓扑未保存此绑定，UI 需手动配置 |
| Q4 | 运行时如何根据 UUID 路由到硬件？ | 无 SignalMapper |
| Q5 | 信号选择 UI 如何让用户可视化挑选？ | `ISignalSelection` 已定义但未接入，`StepTableWidget` 编辑 target 未走此接口 |
| Q6 | 多台同型号设备如何区分？ | `name`=型号不唯一，`pluginId`=插件标识同型号相同，均无法区分实例 |

---

## 2 总体设计

### 2.1 信号身份四元组

UUID 是 **4 元组的紧凑指纹**：

```
Signal UUID = f(deviceId, portName, frameName, nodePath)
            = SHA-1(...).hex (前 32 字符, 128 bit)
```

| 字段 | 来源模块 | 身份策略 | 改名影响 |
|------|----------|---------|---------|
| `deviceId` | topology | **持久 id**，创建时生成 UUID v4，永不变 | 设备改名（`name`）不影响 UUID ✅ |
| `portName` | topology | name（标准通道号，同设备内唯一，基本不改） | 端口改名失效（低概率，重算工具兜底） |
| `frameName` | icd_utility | name | 帧改名失效（重算工具兜底） |
| `nodePath` | icd_utility | 路径（从 root 到 node 的 name 拼接，见 3.7） | 节点改名/重构失效（重算工具兜底） |

**设计取舍**：
- `device` 用持久 id：多台同型号设备 `name` 不唯一（Q6），必须用持久 id 区分实例；同时让设备改名（`name`）不影响 UUID。**注意**：设备改名会导致 `TopologyConnection` 按 `deviceName` 引用的连接断裂（既有问题，不在本方案范围，见风险表）
- `port` 用 name：端口名是标准通道号（ch0、ch1），同设备内唯一，用户基本不改；给每个端口也加 id 代价不值得，低概率改名靠重算工具兜底
- `frame`/`node` 用 name/path：ICD 编辑器重命名帧/重构节点是核心功能，无法禁止；改名失效靠重算工具兜底

> 确定性 UUID 的好处：纯函数计算，无需为 UUID 本身持久化（`deviceId` 要持久化，但那是拓扑实例身份，不是 UUID）；同名配置重算结果一致。

### 2.2 分层架构

```
┌────────────────────────────────────────────────────────┐
│ 应用层 (UI)                                            │
│  IcdSignalSelection(etest_app) → SignalSelectionDialog │
└──────────────┬─────────────────────────────────────────┘
               │ 注册/查询
┌──────────────▼─────────────────────────────────────────┐
│ ICD 信号层 (新增)                                      │
│  SignalRegistry (etest_core):  UUID ↔ 4 元组           │
│  registerDevice / bindPortToFrames / registerSignals   │
│  SynchronizeRegistry (etest_app):  桥接 icd::Repository│
└──────────────┬─────────────────────────────────────────┘
               │ 使用
┌──────────────▼─────────────────────────────────────────┐
│ TestProgram (.etprog)  |  Topology (.etopo)             │
│  target = UUID          |  device.id + ports[].boundFrames[]
│  ISignalSelection (接口) |  devicePortAdded/Removed 信号 │
│  DefaultSignalSelection |  findDeviceById()            │
└────────────────────────────────────────────────────────┘
               │
┌──────────────▼─────────────────────────────────────────┐
│ 测试引擎层 (Phase 5)                                    │
│  Engine.setSignal(uuid, value) → SignalRegistry 解析  │
│                                  → HAL 通道 + ICD pack │
└────────────────────────────────────────────────────────┘
```

### 2.3 关键决策

| # | 决策 | 原因 |
|---|------|------|
| SD1 | UUID 确定性派生自 `(deviceId, portName, frameName, nodePath)` | `deviceId` 持久 → 设备改名不影响 UUID；`portName/frameName/nodePath` 改名会失效，靠重算工具兜底。**不再宣称"支持改名不失效"**，诚实区分哪些稳定、哪些会失效。**设备改名导致 `TopologyConnection` 断裂（既有问题，不在 UUID 方案范围）** |
| SD2 | 设备-端口-帧绑定存储在 `.etopo` 文件中 | 拓扑的物理连接是绑定的天然位置 |
| SD3 | SignalRegistry 放 `etest_core`（`src/core/SignalRegistry.h/.cpp`） | 跨模块共享，不依赖 Qt-UI；与 `icd_utility` 的 `std::string` 接口之间在调用层做转换（见 3.8）；不新增模块，避免架构膨胀 |
| SD4 | `ISignalSelection` 抽象保留，纯文本降级版 `DefaultSignalSelection` 也保留在 test_program | ICD 感知实现 `IcdSignalSelection` 放 etest_app，避免 test_program 反向依赖 icd_utility + etest_app |
| SD5 | UUID 计算统一使用 `QCryptographicHash::Sha1`，取前 32 hex 字符（128 bit） | Qt 内置跨平台；128 bit 生日碰撞概率 ~2^-64（10⁶ 个信号 < 10^-26），对本场景完全足够 |
| SD6 | 旧 `.etprog` target（非 UUID 文本）向后兼容 | `target` 是 QString，旧文件文本无需处理即能加载保存；新文件统一写入 UUID hex。无需格式检测或迁移工具 |
| SD7 | `TopologyDevice` 加持久 `id` 字段 | 解决多台同型号设备 UUID 碰撞 + 设备改名失效；创建时生成 UUID v4，序列化到 `.etopo`，老文件迁移 |
| SD8 | `NodeAttrs::link_to` 废弃 | UUID 是其升级：绑定从 ICD 层移到拓扑层、用持久 id 抗改名、SignalRegistry 强校验、端口绑帧粒度（见 3.9） |

---

## 3 详细设计

### 3.1 Layer 0 — SignalRegistry（UUID 注册中心）

**位置**：`src/core/SignalRegistry.h/.cpp`（属 `etest_core` 模块）

**数据模型**：

```cpp
namespace etest::core {

struct ResolvedSignal {
    QString uuid;          // 32 字符 SHA-1 hex
    QString deviceId;      // 拓扑设备持久 id
    QString deviceName;    // 设备显示名（便于 UI 展示，通过 registerDevice 注册）
    QString portName;      // 设备端口名
    QString frameName;     // ICD 帧名
    QString nodePath;      // 信号节点路径（frame 内的 / 分隔路径，见 3.7）
};

// 批量注册用：4 元组的 key-value 形式，由上层遍历 icd::Repository 后构建
struct SignalEntry {
    QString deviceId;
    QString portName;
    QString frameName;
    QString nodePath;
};

class SignalRegistry : public QObject {
    Q_OBJECT
public:
    explicit SignalRegistry(QObject* parent = nullptr);

    // ── 设备注册（纯 QString 映射，不依赖 icd 类型） ──

    // 注册设备实例：记录 id → 显示名映射
    void registerDevice(const QString& deviceId, const QString& deviceName);
    QStringList registeredDeviceIds() const;
    QString deviceName(const QString& deviceId) const;

    // ── 端口绑定（纯 QString 映射，不依赖 icd 类型） ──

    // 注册设备端口与帧的绑定
    void bindPortToFrames(const QString& deviceId,
                          const QString& portName,
                          const QStringList& frameNames);
    // 解绑设备端口的所有帧
    void unbindPort(const QString& deviceId, const QString& portName);

    // ── 信号索引填充（纯 QString 批量入口，不涉及 icd 类型） ──

    // 批量注册信号，建立 uuid_index_ + node_to_uuids_ 索引
    // 由上层（etest_app）遍历 icd::Repository 构建 entries 后调用
    void registerSignals(const QVector<SignalEntry>& entries);

    // ── 暴露端口绑定给上层（用于 synchronize 遍历） ──

    // 只读回调遍历：回调接收 (deviceId, portName, frameNames)
    // 由 etest_app 的 synchronizeRegistry 工具使用
    using PortBindingCallback =
        std::function<void(const QString& /*deviceId*/,
                           const QString& /*portName*/,
                           const QStringList& /*frameNames*/)>;
    void forEachPortBinding(PortBindingCallback cb) const;

    // ── UUID 计算（确定性纯函数，不依赖索引） ──

    static QString computeUuid(const QString& deviceId,
                               const QString& portName,
                               const QString& frameName,
                               const QString& nodePath);

    // ── 查询 ──

    // UUID → 4 元组（依赖 uuid_index_）
    std::optional<ResolvedSignal> resolve(const QString& uuid) const;
    // 4 元组 → UUID（纯计算，等价于 computeUuid，走索引缓存提升批量性能）
    QString resolveByTuple(const QString& deviceId,
                           const QString& portName,
                           const QString& frameName,
                           const QString& nodePath) const;
    // 反向：已知 frame+node 找所有 UUID
    QVector<ResolvedSignal> findByNode(const QString& frameName,
                                       const QString& nodePath) const;
    // 反向：已知 device+port 找所有信号
    QVector<ResolvedSignal> findByPort(const QString& deviceId,
                                       const QString& portName) const;

    // ── 清理 ──

    // 仅清空信号索引 uuid_index_ + node_to_uuids_（保留设备和端口绑定）
    void clearSignals();
    // 清空所有（设备 + 端口绑定 + 信号索引）
    void clear();

signals:
    void bindingsChanged();

private:
    QHash<QString, ResolvedSignal> uuid_index_;        // UUID → 4 元组
    QHash<QPair<QString, QString>, QStringList> port_to_frames_; // (deviceId,port) → frameNames
    QHash<QString, QString> device_names_;              // deviceId → 显示名
    QHash<QPair<QString, QString>, QStringList> node_to_uuids_; // (frame,nodePath) → UUIDs
};

}  // namespace etest::core
```

**UUID 计算（确定性）**：

```cpp
QString SignalRegistry::computeUuid(const QString& deviceId,
                                    const QString& portName,
                                    const QString& frameName,
                                    const QString& nodePath) {
    // 4 元组用 \x1f 分隔（US, 不可见字符, 避免与 nodePath 的 / 冲突）
    QByteArray raw;
    raw.append(deviceId.toUtf8()).append('\x1f');
    raw.append(portName.toUtf8()).append('\x1f');
    raw.append(frameName.toUtf8()).append('\x1f');
    raw.append(nodePath.toUtf8());
    return QString::fromLatin1(
        QCryptographicHash::hash(raw, QCryptographicHash::Sha1).toHex()
    ).left(32);  // 取前 32 字符 = 128 bit
}
```

**上层同步工具（etest_app，不属 SignalRegistry）**：

`synchronizeFrames` 的职责放在 etest_app 层，作为自由函数或工具类。SignalRegistry 只提供纯 QString 的 `registerSignals` + `forEachPortBinding` 入口：

```cpp
// src/app/utils/SignalSyncHelper.h — 新增，属 etest_app 模块
// 职责：桥接 icd::Repository 和 SignalRegistry

void synchronizeRegistry(etest::core::SignalRegistry& registry,
                         const icd::Repository* repo) {
    QVector<etest::core::SignalEntry> entries;

    // 从 SignalRegistry 遍历端口绑定（不暴露内部索引）
    registry.forEachPortBinding(
        [&](const QString& deviceId, const QString& portName,
            const QStringList& frameNames) {
        for (const QString& frameName : frameNames) {
            const auto* frame = repo->find(frameName.toStdString());
            if (!frame) continue;
            // Frame::nodes() 返回 flat 列表，覆盖所有节点（含深层子节点）
            for (const auto* node : frame->nodes()) {
                QString nodePath = buildNodePath(node);  // 3.7.2
                entries.push_back({deviceId, portName, frameName, nodePath});
            }
        }
    });

    registry.registerSignals(entries);
}
```

**调用时序**（见 3.10）在项目打开、ICD 文件保存后、拓扑绑定变更后执行。

**测试用例**：

```cpp
// tests/etest_core/test_signal_registry.cpp
// SignalRegistry 的单元测试只覆盖纯 QString 接口，不涉及 icd::Repository

TEST(SignalRegistry, ComputeUuidDeterministic) {
    auto u1 = SignalRegistry::computeUuid("dev-id-001", "ch0",
                                           "A429_发送", "业务数据/燃油阀门1");
    auto u2 = SignalRegistry::computeUuid("dev-id-001", "ch0",
                                           "A429_发送", "业务数据/燃油阀门1");
    EXPECT_EQ(u1, u2);
    EXPECT_EQ(u1.size(), 32);
}

TEST(SignalRegistry, DifferentDevicesDifferentUuid) {
    auto u1 = SignalRegistry::computeUuid("dev-id-001", "ch0",
                                           "A429_发送", "业务数据/燃油阀门1");
    auto u2 = SignalRegistry::computeUuid("dev-id-002", "ch0",
                                           "A429_发送", "业务数据/燃油阀门1");
    EXPECT_NE(u1, u2);
}

TEST(SignalRegistry, RegisterAndResolve) {
    SignalRegistry reg;
    reg.registerDevice("dev-id-001", "EPH5272-1");
    reg.bindPortToFrames("dev-id-001", "ch0", {"A429_发送"});

    // 模拟上层 build 的 SignalEntry
    reg.registerSignals({
        {"dev-id-001", "ch0", "A429_发送", "业务数据/燃油阀门1"},
        {"dev-id-001", "ch0", "A429_发送", "业务数据/燃油阀门2"},
    });

    auto uuid = SignalRegistry::computeUuid("dev-id-001", "ch0",
                                             "A429_发送", "业务数据/燃油阀门1");
    auto r = reg.resolve(uuid);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->deviceName, "EPH5272-1");
    EXPECT_EQ(r->nodePath, "业务数据/燃油阀门1");
}

TEST(SignalRegistry, FindByPort) {
    SignalRegistry reg;
    reg.registerDevice("dev-id-001", "EPH5272-1");
    reg.bindPortToFrames("dev-id-001", "ch0", {"A429_发送", "A429_接收"});
    reg.registerSignals({
        {"dev-id-001", "ch0", "A429_发送", "root/temp"},
        {"dev-id-001", "ch0", "A429_接收", "root/pressure"},
    });
    auto signals = reg.findByPort("dev-id-001", "ch0");
    EXPECT_EQ(signals.size(), 2);
}

### 3.2 Layer 1 — 设备端口 ↔ ICD 帧绑定

#### 3.2.1 拓扑数据模型扩展

**文件**：`src/topology/TopologyDocument.h`

`TopologyDevice` 新增持久 `id` 字段：

```cpp
struct TopologyDevice {
    QString id;             // ── 新增：设备实例持久 id（UUID v4），创建时生成，永不变
    QString name;           // 显示名（型号/友好名），可改，不参与 UUID 计算
    QString deviceType;
    QString pluginId;       // 设备插件唯一标识（同型号相同）
    QPointF position{0, 0};
    QVector<QPair<QString, QString>> properties;
    QVector<TopologyDevicePort> ports;
    QSizeF size{0, 0};
};
```

`TopologyDevicePort` 新增 `boundFrameNames`：

```cpp
struct TopologyDevicePort {
    QString name;
    TopologyPort::Direction direction = TopologyPort::Direction::Output;
    FunctionType functionType = FunctionType::CUSTOM;
    int positionHint = -1;
    int portStyle = 0;

    // ── 新增 ──
    QStringList boundFrameNames;   // 该端口绑定的 ICD 帧名
};
```

**访问器**（在 `TopologyDocument` 类内）：

```cpp
// 设备端口 ↔ ICD 帧绑定
void setDevicePortFrames(int deviceIndex, int portIndex,
                         const QStringList& frames);
QStringList devicePortFrames(int deviceIndex, int portIndex) const;

// ── 新增：按 id 查找 ──
int findDeviceIndexById(const QString& id) const;
```

**信号**：

```cpp
signals:
    void devicePortFramesChanged(int deviceIndex, int portIndex);
    // ── M2 新增 ──
    void devicePortAdded(int deviceIndex, int portIndex);
    void devicePortRemoved(int deviceIndex, int portIndex);
    void deviceIdChanged(int deviceIndex, const QString& oldId,
                         const QString& newId);
```

**id 生成**：`TopologyDocument::addDevice` 时，若 `device.id` 为空则生成 UUID v4（`QUuid::createUuid().toString(QUuid::WithoutBraces)`）。
`addDevicePort` / `removeDevicePort` 发射新增的 `devicePortAdded` / `devicePortRemoved` 信号。

#### 3.2.2 JSON 序列化扩展

**文件**：`src/topology/TopologyJsonSerializer.cpp`

**serialize**（device 段，约 line 63-96 附近）：

```cpp
QJsonObject devObj;
devObj["id"] = dev.id;                    // ── 新增
devObj["name"] = dev.name;
devObj["deviceType"] = dev.deviceType;
devObj["pluginId"] = dev.pluginId;
// ... position / properties ...
// device ports
QJsonArray portsArr;
for (const auto& dp : dev.ports) {
    QJsonObject dpObj;
    dpObj["name"] = dp.name;
    dpObj["direction"] = directionToString(dp.direction);
    dpObj["functionType"] = functionTypeToString(dp.functionType);
    dpObj["positionHint"] = dp.positionHint;
    dpObj["portStyle"] = dp.portStyle;
    // ── 新增 ──
    QJsonArray framesArr;
    for (const auto& f : dp.boundFrameNames)
        framesArr.append(f);
    dpObj["boundFrames"] = framesArr;
    portsArr.append(dpObj);
}
```

**deserialize**（device 段，约 line 216-253 附近）：

```cpp
// id：读取，缺省时生成（老文件迁移）；立即标记 document modified 以提示用户保存
dev.id = dObj["id"].toString();
if (dev.id.isEmpty()) {
    dev.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    // 标记 document modified，确保 id 会被持久化
    doc.setModified(true);
}
// ... name / deviceType / pluginId / position / size / properties ...

// device ports
QJsonObject portObj = portVal.toObject();
TopologyDevicePort dp;
dp.name = portObj["name"].toString();
// ... direction / functionType / positionHint / portStyle ...
// ── 新增 ──
QJsonArray framesArr = portObj["boundFrames"].toArray();
for (const auto& f : framesArr)
    dp.boundFrameNames.append(f.toString());
```

**向后兼容**（增补说明）：
- 缺省 `id` 字段：反序列化时生成新 UUID，并**标记 document modified**，确保 id 会被保存。如果用户不编辑直接关闭，`document modified` 提示用户保存。
- 缺省 `boundFrames` 字段：视为空数组
- `version` 字段：保持 `1` 不变（新增字段都是可选的，不破坏前向兼容）。反序列化只校验 `version` 是 double，不拒绝具体值

> **行号说明**：V1.0 文档写的"88-92 / 240-249"为近似值，实际以代码为准。落地时在 `TopologyJsonSerializer.cpp` 搜 `dpObj["name"]` 定位 device port 序列化点。
> 序列化时 `id` 插在 `name` 之前（约 line 66），反序列化时在 `name` 之后（约 line 220）。

#### 3.2.3 拓扑编辑器 UI 扩展

**位置**：`src/topology/...` 中的 `DevicePortEditorDialog`（或新建）

**交互流程**：

```
用户右键设备端口 → "绑定 ICD 帧..."
  ↓
弹出对话框:
  - 左侧: 当前项目可用的 ICD 帧列表（从 icd_utility::Repository 加载）
  - 右侧: 已绑定到该端口的帧
  - 按钮: 添加 →  移除 ←
  - 过滤: FunctionType (A429 / SERIAL / AD / DA ...)
  - 多选
  - 确认 → TopologyDocument.setDevicePortFrames()
```

**撤销/重做**：通过 `QUndoCommand` 集成到 `TopologyDocument::undoStack()`。

#### 3.2.4 端口绑定验证与清理

- 同一个 `frameName` 可绑定到多个端口（广播场景）
- 同一个端口可绑定多个帧
- **设备端口增删**：M2 新增 `devicePortAdded` / `devicePortRemoved` 信号，`TopologyDocument::addDevicePort` / `removeDevicePort` 发射这些信号
- **清理规则**：
  - 删除设备 → 监听 `deviceRemoved`，从 SignalRegistry 移除该设备所有绑定（遍历所有端口调 `unbindPort`）
  - 删除设备端口 → 监听 `devicePortRemoved`，从 SignalRegistry 解绑该端口（调 `unbindPort`）
  - 关闭/新建项目 → 监听 `documentCleared`，调 `SignalRegistry::clear()`
  - 监听 `deviceIdChanged`（极少出现，仅在老文件迁移生成 id 时），同步更新 SignalRegistry 的 deviceId

### 3.3 Layer 2 — ICD 感知的信号选择对话框

**位置**：`src/app/dialogs/SignalSelectionDialog.h/.cpp`（新建）

**类签名**：

```cpp
namespace etest::app {

class SignalSelectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit SignalSelectionDialog(
        const etest::core::SignalRegistry* registry,
        const icd::Repository* repository,   // 用于构建信号树
        QWidget* parent = nullptr);

    // 返回选中的 UUID
    QString selectedUuid() const;

private slots:
    void onDeviceChanged(const QString& deviceId);
    void onPortChanged(const QString& portName);
    void onFrameChanged(const QString& frameName);
    void onNodeSelected(const QModelIndex& index);
    void updateOkButton();

private:
    void populateDevices();
    void populatePorts(const QString& deviceId);
    void populateFrames(const QString& deviceId, const QString& portName);
    void populateNodes(const QString& frameName);

    const etest::core::SignalRegistry* registry_;
    const icd::Repository* repository_;
    QString current_uuid_;

    // UI
    QComboBox* deviceCombo_;   // 显示 deviceName，userData 存 deviceId
    QComboBox* portCombo_;
    QComboBox* frameCombo_;
    QTreeView* nodeTree_;      // 显示 frame 内的 Node 树
    QLabel* infoLabel_;        // 显示选中信号的元信息
};

}  // namespace etest::app
```

**界面布局**：

```
┌──────────────────────────────────────────────┐
│ 信号选择                                       │
├──────────────────────────────────────────────┤
│ 设备:  [EPH5272 (dev-id-001)  ▼]            │  ← 显示 name，内部用 id
│ 端口:  [ch0                   ▼]            │
│ 帧:    [A429_发送             ▼]            │
├──────────────────────────────────────────────┤
│ 信号树:                                       │
│  ▼ A429_发送                                  │
│    ├ 帧头                                     │
│    ├ 长度                                     │
│    ├ 校验和                                   │
│    └ 业务数据                                  │
│       ├ 燃油阀门1                              │
│       ├ 燃油阀门2                              │
│       └ 转速                                   │
├──────────────────────────────────────────────┤
│ 选中: 燃油阀门1 (uint16, Offset=2, Bit=0)     │
│ UUID: 3f7a2b9c...                            │
├──────────────────────────────────────────────┤
│              [取消]  [确定]                   │
└──────────────────────────────────────────────┘
```

**实现要点**：

1. **设备下拉**：从 `SignalRegistry::registeredDeviceIds()` 获取设备列表，`SignalRegistry::deviceName(id)` 获取显示名
2. **端口下拉**：从 `port_to_frames_` 获取该设备已注册绑定的端口列表（即 `findByPort` 的反向查询）
3. **帧下拉**：从 `port_to_frames_[(deviceId, port)]` 获取已绑定到该端口的帧
4. **信号树**：从 `icd::Repository::find(frameName)` 取出 Frame，遍历 `roots()` 和 `children()` 构建树；选中节点时用 3.7 节算法生成 `nodePath`
5. **UUID 计算**：选择信号后调用 `SignalRegistry::computeUuid(deviceId, portName, frameName, nodePath)` 得到 UUID
6. **元信息显示**：通过 `Node::offset()/bit_offset()/bit_width()/value_type()` 显示；`value_type()` 返回 `icd::ValueType` 枚举，UI 显示用 `IcdProtocolUtils` 的字符串映射

**QString/std::string 边界**：`Repository`/`Frame`/`Node` 接口是 `std::string_view`，对话框内部转换（见 3.8）。

**QSS 命名空间**（遵循项目规则 11）：

```css
etest--app--SignalSelectionDialog QComboBox { ... }
etest--app--SignalSelectionDialog QTreeView { ... }
```

### 3.4 Layer 3 — 替换默认信号选择

#### 3.4.0 前置：接入 ISignalSelection（M0）

**现状**：`ISignalSelection` / `DefaultSignalSelection` 已定义在 `SignalSelectionInterface.h`，但 grep 全 `src/` 无调用点。`StepTableWidget` 编辑 target 时未走此接口。

**M0 工作**：让 `StepTableWidget` 的 target 列（`kColTarget = 2`）编辑走 `ISignalSelection`（先用 `DefaultSignalSelection` 文本输入接入），为后续替换为 `IcdSignalSelection` 铺路。

**注入方式**：`ISignalSelection*` 由上层（`TestProgramEditorWidget` / `MainWindow`）通过 `StepTableWidget` 的 setter 或构造参数注入。UI 启动时注入带 registry+repository 的 `IcdSignalSelection`，测试时注入 mock。

```cpp
// src/test_program/SignalSelectionInterface.h — 保持不变
// ISignalSelection — 抽象接口
// DefaultSignalSelection — 纯文本降级（QInputDialog::getText）

namespace etest::app {

class ISignalSelection {
public:
    virtual ~ISignalSelection() = default;
    virtual QString selectSignal(QWidget* parent) = 0;
};

class DefaultSignalSelection : public ISignalSelection {
public:
    QString selectSignal(QWidget* parent) override {
        bool ok;
        QString result = QInputDialog::getText(
            parent, QStringLiteral("选择信号"),
            QStringLiteral("信号 UUID 或名称:"), QLineEdit::Normal,
            QString(), &ok);
        return ok ? result.trimmed() : QString();
    }
};

}  // namespace etest::app
```

**ICD 感知实现（新文件，属 etest_app）**：

```cpp
// src/app/dialogs/IcdSignalSelection.h / .cpp — 新增
namespace etest::app {

class IcdSignalSelection : public ISignalSelection {
public:
    explicit IcdSignalSelection(etest::core::SignalRegistry* registry,
                                const icd::Repository* repository)
        : registry_(registry), repository_(repository) {}

    QString selectSignal(QWidget* parent) override {
        if (registry_ && repository_) {
            SignalSelectionDialog dlg(registry_, repository_, parent);
            if (dlg.exec() == QDialog::Accepted)
                return dlg.selectedUuid();
            return QString();  // 取消
        }
        // 降级为文本输入
        DefaultSignalSelection fallback;
        return fallback.selectSignal(parent);
    }

private:
    etest::core::SignalRegistry* registry_;
    const icd::Repository* repository_;
};

}  // namespace etest::app
```

**分层说明**：
- `ISignalSelection` + `DefaultSignalSelection` → `src/test_program/SignalSelectionInterface.h`（`etest_test_program` 模块）
- `IcdSignalSelection` → `src/app/dialogs/IcdSignalSelection.h/.cpp`（`etest_app` 模块，可依赖 `icd_utility` 和 `etest_core`）
- `etest_test_program` 不新增对 `icd_utility` 的依赖，保持模块独立性
- `etest_app` 在 M5 中构造 `IcdSignalSelection` 并注入给 `StepTableWidget`

**注入机制（M0 实现细节）**：

```cpp
// StepTableWidget 新增
class StepTableWidget : public QTableView {
public:
    // M0: 设置信号选择接口（nullptr 降级为直接文本编辑）
    void setSignalSelection(ISignalSelection* sel) { signal_selection_ = sel; }
    // M5: 设置 SignalRegistry（用于 UUID → 可读名称 resolve）
    void setRegistry(etest::core::SignalRegistry* reg) { registry_ = reg; }

    // M0 编辑拦截（在 model 的 itemChanged 或 delegate 中）：
    // 当编辑 kColTarget 列时，若 signal_selection_ 非空：
    //   QString uuid = signal_selection_->selectSignal(parent);
    //   if (!uuid.isEmpty()) setCellText(row, kColTarget, uuid);
    // 否则走 QTableView 默认文本编辑
private:
    ISignalSelection* signal_selection_ = nullptr;
    etest::core::SignalRegistry* registry_ = nullptr;
};

// TestProgramEditorWidget 新增
class TestProgramEditorWidget : public QMainWindow, public IEditor {
public:
    // M0: 传播 ISignalSelection 到所有 StepTableWidget
    void setSignalSelection(ISignalSelection* sel);
    // M5: 传播 SignalRegistry（用于 resolve→显示名称）
    void setRegistry(etest::core::SignalRegistry* reg);
};

// TestProgramEditorWidget 实现
void TestProgramEditorWidget::setSignalSelection(ISignalSelection* sel) {
    setup_table_->setSignalSelection(sel);
    teardown_table_->setSignalSelection(sel);
    for (int t = 2; t < tab_widget_->count(); ++t) {
        if (auto* table = qobject_cast<StepTableWidget*>(tab_widget_->widget(t)))
            table->setSignalSelection(sel);
    }
}
```

**EditorManager 注入点（M5）**：

```cpp
// src/app/EditorManager.cpp — 工厂注册
EditorFactoryRegistry::registerFactory(
    "testprogram",
    [](const QString& id, QWidget* parent) -> IEditor* {
        auto* editor = new TestProgramEditorWidget(id, parent);
        editor->setEmbeddedMode(true);
        // M0: ISignalSelection 不在此注入（editor 内 nullptr 降级文本编辑）
        return editor;
    },
    [](IEditor* editor, ads::CDockWidget* dock, EditorManager* mgr) {
        auto* te = qobject_cast<TestProgramEditorWidget*>(editor->widget());
        if (!te) return;
        // M5: 注入 IcdSignalSelection + SignalRegistry
        auto* signalSel = new IcdSignalSelection(
            /*registry*/, /*repository*/);  // 从 mgr 或全局获取
        te->setSignalSelection(signalSel);
        te->setRegistry(/*registry*/);
        // ... 现有 modificationChanged / editorIdChanged 连接 ...
    });
```

**Standalone demo 安全**：独立可执行模式（`examples/testprogram-demo`）不链接 `icd_utility`，
不会走 M5 的 `IcdSignalSelection` 注入。M0 代码中 `StepTableWidget` 对 `signal_selection_`
为 nullptr 的情况做 null guards，降级为 QTableView 默认文本编辑。

**UUID 显示策略（M5 实现）**：

表格单元格存储的是 UUID hex 字符串，但 32 字符对用户不可读。采用 **resolve→名称优先** 策略：

```
加载时：
  step.target = "3f7a2b9c..."  ← UUID（存 ext data 或 UserRole）
  if registry_->resolve(uuid) 有值 →
      显示可读名（如 "燃油阀门1" 或 "EPH5272-1/ch0/A429_发送/燃油阀门1"）
      完整 4 元组 + UUID 放 tooltip
  else →
      显示 UUID 前 8 位 + "…"（灰色斜体）
      tooltip: "未找到关联信号（ICD 配置可能已变更）"

保存时：
  readStepData 中的 target 始终从 UserRole / ext data 取 UUID，不从显示文本反推

列宽：
  kColTarget: setColumnWidth(180) + setTextElideMode(Qt::ElideRight)
```

关键约束：**单元格存的是 UUID hex，名称仅用于显示**，否则 `readStepData` 的 `cellText(kColTarget)` 会读到显示名而非 UUID，导致保存错误。

### 3.5 Layer 4 — 运行时解析（Phase 5 实现）

> 本节为远期规划，**不在 V1.1 实施范围内**，但需为它留出接口。

**Engine API 调整**（Phase 5）：

```cpp
class IEngine {
    virtual void setSignal(const QString& uuid, const QVariant& value,
                            std::function<void(bool, QString)> done) = 0;
};
```

**解析流程**（Engine 内部）：

```
setSignal(uuid, value)
  ↓
SignalRegistry.resolve(uuid)  → {deviceId, portName, frameName, nodePath}
  ↓
├─→ TopologyDocument.findDeviceIndexById(deviceId) → 找设备
│     → 按 portName 找端口
│     → TopologyConnection 找物理通道
│     → HAL.setOutput(pluginId, channel, raw_value)
│
└─→ Repository::find(frameName) → Frame
      → 按 nodePath 定位 Node（见 3.7.3）
      → ProtocolRegistry.pack(value, frame, node) → 原始字节
      → 传输通道 (Serial / TCP / UDP) 发送
```

#### 读路径（Phase 5 实现，V1.1 不覆盖）

V1.1 为读路径提供了 `SignalRegistry.resolve(uuid)` → 4 元组的定位能力，
但运行时值存取和执行引擎不在本方案范围。

**Engine API**（Phase 5 补充）：

```cpp
class IEngine {
    // ── 写 ──
    virtual void setSignal(const QString& uuid, const QVariant& value,
                            std::function<void(bool, QString)> done) = 0;
    // ── 读 ──
    virtual tl::expected<QVariant, Error> getSignal(const QString& uuid) = 0;
};
```

**读路径流程**：

```
getSignal(uuid)
  ↓
SignalRegistry.resolve(uuid)  → {deviceId, portName, frameName, nodePath}   ← ✅ V1.1 建好
  ↓
Repository::find(frameName) → Frame
  → findNodeByPath(frame, nodePath) → Node                                  ← ✅ 3.7.3 方案
  ↓
SignalValueCache::get(uuid)                                                   ← ❌ 需 Phase 5 实现
  ├─ 命中 → 返回缓存值（监控面板 / Lua 条件判断用）
  └─ 未命中 → HAL 重新采集 → ICD::Node::decode → 入 Cache → 返回

对应的测试步骤：
  VERIFY target 期望值         → getSignal(uuid) + 容差比较
  WAIT   target 条件值          → 循环 getSignal(uuid) + 条件判断
  WHILE  同 WAIT（持续执行子步骤）
```

**Phase 5 需要实现的组件**：

| 组件 | 职责 | 依赖 |
|---|---|---|
| `SignalValueCache` | `Map<QString, QVariant>` + 按需刷新 | 无 |
| `Engine::getSignal` | Cache miss → HAL 采集 → ICD decode → 返回 | `icd::Node::decode` |
| `Engine::setSignal` | ICD pack → HAL 输出（写路径） | `icd::Node::set_value` |
| VERIFY 执行器 | getSignal → 容差范围比较 | `ToleranceSpec` |
| 监控面板数据源 | 订阅 SignalValueCache → UI 刷新 | `SignalValueCache` |

### 3.6 文件格式兼容性

#### `.etprog`（test program）

**当前**：
```json
{
  "cmd": "SET",
  "target": "温度",
  "value": 37.5
}
```

**目标**：
```json
{
  "cmd": "SET",
  "target": "3f7a2b9c4d8e1f0a6b5c7d8e9f0a1b2c",
  "value": 37.5
}
```

**兼容性**：
- 加载时不区分 UUID 格式和文本格式（`target` 始终是 QString，旧文本正常加载）
- 新文件统一写入 UUID hex
- 旧文本 target 在信号选择对话框中不会出现在结果里（SignalRegistry::resolve 找不到，表格显示原文本）

#### `.etopo`（topology）

**当前**（设备）：
```json
{
  "name": "EPH5272",
  "deviceType": "...",
  "pluginId": "..."
}
```

**目标**：
```json
{
  "id": "550e8400-e29b-41d4-a716-446655440000",
  "name": "EPH5272",
  "deviceType": "...",
  "pluginId": "...",
  "ports": [
    {
      "name": "ch0",
      "direction": "output",
      "functionType": "A429",
      "boundFrames": ["A429_发送_Label050", "A429_发送_Label060"]
    }
  ]
}
```

**兼容性**：
- `id` 缺省：反序列化时生成新 UUID，**标记 document modified**，提示用户保存（确保 id 持久化）
- `boundFrames` 缺省：视为空数组
- `version` 保持 `1`：新增字段都是可选的，不破坏前向兼容

#### `ICDConfig.xml` / `.eproto`（ICD）

**无需修改**。Node 的 UUID 由 `(frameName, nodePath)` 派生，不给 Node 加字段。

### 3.7 nodePath 定义（新增）

UUID 计算输入之一是 `nodePath`，需明确其定义，否则不同实现算出不同 UUID。

#### 3.7.1 格式

- 分隔符：`/`（正斜杠）
- 方向：从 Frame 的 root Node 到目标 Node 的 **name 拼接**
- root 节点本身：其 name 作为 path 的第一段
- 示例：Frame `A429_发送` 的结构
  ```
  root: 业务数据
    └ 燃油阀门1
  ```
  则 `燃油阀门1` 的 nodePath = `业务数据/燃油阀门1`

#### 3.7.2 生成算法

`Node` 类没有 `path()` 方法，需遍历 parent 链拼接：

```cpp
// 在 SignalSelectionDialog / SignalRegistry 调用层实现
QString buildNodePath(const icd::Node* node) {
    QStringList segments;
    const icd::Node* cur = node;
    while (cur) {
        // string_view → QString：用 fromUtf8 避免 string_view 不保证 \0 终止
        segments.prepend(
            QString::fromUtf8(cur->name().data(),
                              static_cast<int>(cur->name().size())));
        cur = cur->parent();
    }
    return segments.join('/');
}
```

#### 3.7.3 反向定位（UUID → Node）— Phase 5 实现

`Repository::find(frame_name, node_name)` 按 name 查找，不支持 path，重名 Node 有歧义。UUID 反向定位（`nodePath → Node*`）属于 Phase 5（运行时引擎）的范围，M1-M6 不实现。

**Phase 5 实现方案**：在 etest_app（Engine 所在层）实现 `findNodeByPath` 辅助函数，**不缓存裸指针，不侵入 icd_utility**：

```cpp
// etest_app 层辅助函数（Phase 5）
const icd::Node* findNodeByPath(const icd::Frame* frame,
                                 const QString& nodePath) {
    const QStringList segments = nodePath.split('/');
    if (segments.isEmpty()) return nullptr;
    // 匹配 root 层：遍历 roots() 匹配第一段
    for (const auto& root : frame->roots()) {
        auto* result = matchNodePath(root.get(), segments, 0);
        if (result) return result;
    }
    return nullptr;
}

static const icd::Node* matchNodePath(const icd::Node* node,
                                       const QStringList& segments,
                                       int depth) {
    // 比较当前节点 name 与 segments[depth]
    if (depth >= segments.size()) return nullptr;
    QString seg = QString::fromUtf8(node->name().data(),
                                     static_cast<int>(node->name().size()));
    if (seg != segments[depth]) return nullptr;
    if (depth == segments.size() - 1) return node;  // 匹配到底
    // 递归匹配子节点
    for (const auto& child : node->children()) {
        auto* result = matchNodePath(child.get(), segments, depth + 1);
        if (result) return result;
    }
    return nullptr;
}
```

**不采用 Node* 缓存方案的原因**：
1. SignalRegistry（etest_core）不应缓存 icd 类型的指针，保持架构纯净
2. Node* 的生命周期由 Repository 管理，Repository 重建（编辑保存 ICD 文件）后裸指针立即 dangling
3. 反向定位只在 Phase 5 运行时引擎路径上需要，M1-M6（编辑期）不需要

#### 3.7.4 重名 Node 的处理

- 同一 Frame 内、同一父节点下，不允许同名子节点（ICD 编辑器需校验）
- 不同父节点下可同名（如 `业务数据/校验` 和 `配置区/校验`），靠 path 区分，UUID 不同 ✅

### 3.8 QString ↔ std::string 边界（新增）

`icd_utility` 是纯 C++17 无 Qt 依赖（CLAUDE.md 明确），`Node::name()` / `Frame::name()` 返回 `std::string_view`。`SignalRegistry` 用 `QString`。

**转换原则**：
- 转换发生在**调用层**（`SignalSelectionDialog`、etest_app 的 `synchronizeRegistry`、Engine），不在 `icd_utility` 内部
- `icd_utility` **不引入 Qt**，不 include 任何 Qt 头
- `std::string_view` → `QString`：`QString::fromUtf8(sv.data(), static_cast<int>(sv.size()))`（注意 `string_view` 不保证 `\0` 终止，不能用 `fromStdString`）
- `std::string` → `QString`：`QString::fromStdString(s)` 或 `QString::fromUtf8(s.data(), s.size())`
- `QString` → `std::string`：`qstr.toStdString()`（UTF-8 编码）

### 3.9 linkTo 废弃说明（新增）

`NodeAttrs::link_to`（形如 `\DA0\ch0`）是"信号→硬件路径"的早期雏形，但有四个问题：

1. **放错层**：绑定信息写死在 ICD（Node 身上），真相源在拓扑
2. **无强校验**：字符串写错静默失败
3. **粒度过细**：信号级绑定，现实是端口绑帧级
4. **跨项目不通用**：同一份 `.eproto` 换拓扑，`link_to` 全是错的

UUID 方案是对 `link_to` 的全面升级：

| 维度 | link_to | UUID 方案 |
|------|---------|----------|
| 绑定位置 | ICD 层（Node 身上） | 拓扑层（`boundFrames`） |
| 身份稳定性 | 字符串路径，拓扑改名失效 | `deviceId` 持久，设备改名不影响 |
| 校验 | 无 | SignalRegistry 强校验 |
| 粒度 | 信号级 | 端口绑帧级 |
| 跨项目 | 不通用 | 通用（绑定在拓扑） |

**处理方式**：
- `link_to` 字段在 `NodeAttrs` 中**留代码、留格式、不用它**——不删除实现，不改 `.eproto` 格式定义，老文件正常读写
- 新建绑定走 UUID 方案（拓扑 `boundFrames`），不再填写 `link_to`
- `link_to` 视为 deprecated，ICD 编辑器 UI 可隐藏该字段编辑入口
- 不做主动迁移（`link_to` 与 UUID 语义不同，无法自动转换；代码留着不影响任何东西）
- **文档协调**：`docs/规划/帧协议编辑器设计.md` 2.3.3 节（属性面板）的 LinkTo 条目需同步更新为 deprecated 说明

### 3.10 同步时序定义（新增）

定义 `SignalRegistry`、`TopologyDocument`、`icd::Repository` 三者的同步编排。核心原则：**SignalRegistry 不持有 icd 类型引用，同步由 etest_app 层编排**。

#### 项目打开时序

```
1. ProjectManager::openProject(path)
   │
2. TopologyDocument 加载 .etopo
   │  deserialize: 老文件缺 id → 生成并 setModified(true)
   │  forEachDevice → SignalRegistry::registerDevice(id, name)       ← M2
   │  forEachPort  → SignalRegistry::bindPortToFrames(...)          ← M2
   │
3. icd::Repository 加载所有 .eproto（由 ProtocolManager 或 MainWindow 负责）
   │  forEachFile → json_parser → schema::build_repository()
   │
4. etest_app::synchronizeRegistry(registry, repo)                   ← M4
   │  forEachPortBinding → Repository::find(frame) → nodes()
   │  → buildNodePath() → registerSignals(entries)
   │
5. 加载 .etprog 文件中的 UUID
   │  → SignalRegistry::resolve(uuid) 交叉验证（存在？名称匹配？）
```

**关键约束**：步骤 2 和 3 必须先于步骤 4，但 2 和 3 之间无顺序依赖（port_to_frames_ 存的是 frameName 字符串，icd::Repository 不需要先加载）。

#### ICD 文件编辑保存后

```
1. IcdSerializer 写回 .eproto → Repository 重建（或增量更新）
2. Repository 持有者发射 repositoryRebuilt() 信号
3. 槽调 synchronizeRegistry(registry, repo) → 重建 uuid_index_
```

**Repository 持有者**：在项目级是 ProtocolManagerWidget（侧边栏）或 MainWindow。Repository 的生命周期由项目打开/关闭管理。

#### 拓扑绑定变更后

```
1. setDevicePortFrames → 更新 port_to_frames_
2. devicePortFramesChanged 信号
3. 是否触发重新 synchronize 取决于变更类型：
   a) 新增绑定 → 仅注册新增帧的信号（增量），或全量重建
   b) 删除绑定 → 遍历 uuid_index_ 移除该 (deviceId, port, frame) 的所有条目
4. 全量重建：调 synchronizeRegistry(registry, repo)
```

**M2 实现建议**：绑定变更后走 **全量重建**（保证一致性），待 Profile 发现性能瓶颈后再优化为增量。

#### SignalRegistry 生命周期

- 每个项目打开时**新建** SignalRegistry 实例（或 `clear()` 复用）
- 项目关闭时 `clear()` 清理所有索引（连接 `documentCleared` 信号）
- 不跨项目共享

### 3.11 三个 standalone demo 的独立性

三个示例程序会作为独立产品发布，各自输出对应格式的文件，V1.1 方案不破坏它们的独立性。

| Demo | 输出格式 | 依赖链 | V1.1 影响 |
|---|---|---|---|
| **topology-demo** | `.etopo` | `etest_topology` + `etest_ui` + Qt5 | `TopologyDevice.id`/`boundFrameNames` 是纯 `QString`/`QStringList` 数据字段，头文件即可读；`QUuid::createUuid()` 在 `Qt5::Core`（已有）。**CMake 无需改** ✅ |
| **protocol-demo** | `.eproto` / `.eprotox` | `etest_protocol` + `etest_ui` + Qt5 | `icd_utility` 零改动（不删代码、不改格式、不加字段、不侵入 `find_by_path`）；`link_to` 废弃只是"不用它"，代码和格式完整保留。**CMake 无需改** ✅ |
| **testprogram-demo** | `.etprog` | `etest_core` + `etest_program` + `etest_ui` + Qt5 | `ISignalSelection` + `DefaultSignalSelection` 在 `etest_test_program` 模块内部，M0 的 nullptr 守卫确保 standalone 下降级为文本编辑；`IcdSignalSelection`、`SignalSyncHelper` 等 ICD 感知组件全部在 `etest_app`（主程序），testprogram-demo 不链接就不编译。**CMake 无需改** ✅ |

**关键原则**："增强功能堆在 `etest_app`"——`SignalSyncHelper`、`IcdSignalSelection`、`SignalSelectionDialog`、UUID→名称的 display resolve 全在 `etest_app` 层。下层各 demo 模块不新增对 `icd_utility` 或 `etest_app` 的依赖。

testprogram-demo 中的 UUID hex 无法 resolve 为名称的问题：在 standalone 模式下，`SignalRegistry`（etest_core）无 ICD 数据，`resolve(uuid)` 返回 nullopt，表格按 fallback 策略显示 UUID hex 前 8 位（或灰色显示"未关联"），不影响保存/加载。

---

## 4 实施计划

### 4.1 阶段拆分

| 阶段 | 内容 | 模块 | 工时估算 |
|------|------|------|----------|
| **M0** | `StepTableWidget.target` 列编辑接入 `ISignalSelection`（`StepTableWidget::setSignalSelection` + `TestProgramEditorWidget::setSignalSelection` 传播 + nullptr 守卫降级 + `DefaultSignalSelection` 文本输入） | `etest_test_program` | 0.5 周 |
| **M1** | `SignalRegistry` 实现（`registerSignals`/`forEachPortBinding`/`clearSignals`/`findByNode`/`findByPort`）+ `SignalEntry` 结构 + `nodePath` 定义 + 单测 | `etest_core` | 1 周 |
| **M2** | `TopologyDevice.id` + `boundFrameNames` 数据模型 + `devicePortAdded/Removed`/`deviceIdChanged` 信号 + `findDeviceById` + JSON 序列化 + 老文件迁移（标记 modified 确保持久化）+ `documentCleared` 处理 | `etest_topology` | 1 周 |
| **M3** | 拓扑 UI 端口绑定对话框 + 撤销重做 | `etest_topology` | 1 周 |
| **M4** | `SignalSelectionDialog` + `IcdSignalSelection` + `SignalSyncHelper`（`synchronizeRegistry` 自由函数 + 同步编排） | `etest_app` | 1.5 周 |
| **M5** | `IcdSignalSelection` 注入给 `TestProgramEditorWidget`（EditorManager 工厂回调）+ `StepTableWidget::setRegistry` 绑定 + UUID 显示策略（resolve→名称、列宽、tooltip、保存始终存 UUID）+ 集成测试 | `etest_app` + `etest_test_program` | 0.5 周 |
| **M6** | MainWindow 接线（SignalRegistry 注入 EditorManager，项目打开/关闭时初始化/清理）+ 跨模块集成测试 | `etest_app` | 0.5 周 |
| **合计** | | | **6 周** |

### 4.2 与 IATP 7 阶段的关系

| IATP 阶段 | 关系 |
|-----------|------|
| 1 基础框架（✅） | 已完成，本规划在它之上 |
| 2 HAL 接口+Mock | 无直接依赖 |
| 3 ICD 信号层 | **强依赖**：本规划需要 `icd::Repository` 存在 |
| 4 用例管理层 | **强依赖**：本规划需要 `ISignalSelection` 接入 `StepTableWidget`（M0） |
| 5 测试引擎层 | 消费方：Layer 4 运行时解析 |
| 6 真实硬件 | 验证 M3-M5 的完整链路 |
| 7 测试与优化 | 回归测试 |

**建议实施窗口**：在 IATP 阶段 3（ICD 信号层）启动后、阶段 4（用例管理层）开始编辑测试步骤前完成 M0-M2。M3-M5 可在阶段 4 中并行。

### 4.3 依赖关系

```
M0 (接入 ISignalSelection)  ←── 独立，可最先做
  ↓
M1 (SignalRegistry + nodePath 定义)
  ↓
M2 (Topology id + boundFrames 数据模型 + 信号 + findDeviceById)
  ↓
M3 (Topology UI 绑定)
  ↓
M4 (SignalSelectionDialog + IcdSignalSelection)  ←── 需 icd::Repository
  ↓
M5 (注入 + 集成)
  ↓
M6 (MainWindow 接线 + 集成测试)
```

---

## 5 测试策略

### 5.1 单元测试（Google Test）

| 测试文件 | 覆盖 |
|----------|------|
| `tests/etest_core/test_signal_registry.cpp` | UUID 确定性、多设备不碰撞、`computeUuid`/`resolveByTuple` 纯函数正确性、`registerDevice`/`bindPortToFrames`/`synchronizeFrames` 索引构建、`resolve`/`findByNode`/`findByPort` 查询、`clear` 清理 |
| `tests/etest_topology/test_topology_serializer.cpp` | `id` + `boundFrames` 序列化往返、老文件迁移（缺 id 时生成并标记 modified）、版本兼容 |
| `tests/etest_topology/test_topology_document.cpp` | `setDevicePortFrames` 信号触发、`addDevice` 自动生成 id、`findDeviceById` 正确性、`devicePortAdded/Removed` 信号、`documentCleared` + `clear` 联动 |

### 5.2 集成测试

| 测试 | 验证 |
|------|------|
| 加载示例项目 | ICD 帧、拓扑端口、UUID 全部一致 |
| 端到端选择流程 | 拓扑绑定 → `synchronizeFrames` 建立索引 → `IcdSignalSelection` 选择 → UUID 写入 `.etprog` |
| 跨文件版本兼容 | 老 `.etprog` (无 UUID)、老 `.etopo` (无 id/boundFrames) 正常加载 |
| 多台同型号设备 | 两台 EPH5272 的同名信号 UUID 不同 |
| `documentCleared` 清理 | 关闭项目后 `SignalRegistry` 清空，重新打开重建索引 |

### 5.3 手动验收

| 场景 | 期望 |
|------|------|
| 打开一个旧 `.etprog` | 正常加载，旧文本 target 保持原样显示，不崩溃不报错 |
| 绑定一个端口到帧 | 拓扑 UI 可视化绑定，信号对话框可选中该帧的信号 |
| 重命名设备（name） | UUID 不变，已绑定信号不失效 ✅；但拓扑连接可能断裂（已知） |
| 重命名 ICD 帧 | UUID 失效，提示用户重新绑定（重算工具兜底） |
| 删除设备/端口 | SignalRegistry 同步清理（通过 `deviceRemoved`/`devicePortRemoved` 信号） |
| 关闭项目 | SignalRegistry 清空（通过 `documentCleared` 信号） |
| 多台同型号设备 | 各自独立绑定，UUID 互不碰撞 |
| 添加端口后立即绑定 | `devicePortAdded` 信号未错过，SignalRegistry 可绑定该端口 |

---

## 6 风险与权衡

| 风险 | 影响 | 缓解 |
|------|------|------|
| 设备改名（name） | 无影响 ✅ | `deviceId` 持久，UUID 用 id |
| 设备改名导致 TopologyConnection 断裂 | 连接断裂 | **既有问题**，`TopologyConnection` 按 `deviceName` 引用设备。改名修复不在 UUID 方案范围。可在 `renameDevice` 中添加同步更新 `Connection` 的 `deviceName` 的兜逻辑（独立修复） |
| 端口改名（portName） | UUID 失效 | 低概率（端口名标准化），重算工具兜底；未来若需求大可给 port 加 id |
| ICD 帧重命名 / 节点重构 | UUID 失效 | 重算工具兜底；ICD 编辑器重命名帧时检查拓扑 boundFrames 引用 |
| `deviceId` 迁移风险 | 低 | 老文件缺 id 时自动生成并标记 modified，提示保存；生成后持久化，后续稳定 |
| 确定性 UUID 缺少版本字段 | 低 | 未来如需改算法，可加 version 字段保留兼容；或换 hash 算法时同步提供迁移工具 |
| `Node*` 缓存生命周期 | 中 | Repository 重建时调 `synchronizeFrames` 重建缓存；不持有所有权 |
| `ISignalSelection` 注入路径 | 中 | 采用构造注入 + setter，M0 先接入 `DefaultSignalSelection`，M5 替换为 `IcdSignalSelection` |
| 跨模块循环依赖 | 中 | SignalRegistry 放 `etest_core`（最低层），ICD 感知实现在 `etest_app`（最高层），test_program 中间层不依赖 icd_utility |
| 老项目立即保存不带 uuid | 低 | 加载时 `setModified(true)` 提示保存；若用户强制关闭不保存，下次打开重新生成 id |

---

## 7 参考与相关文档

- `docs/thinking/IATP_设计方案_精简版.md` — 第 4、7 章
- `docs/thinking/IATP_设计方案.md` — V2.0 完整版（如有）
- `src/test_program/SignalSelectionInterface.h` — `ISignalSelection` 定义
- `src/test_program/TestProgramData.h` — `TestStepData` 数据结构
- `src/icd_utility/include/icd/node.hpp` — `icd::Node` 类
- `src/icd_utility/include/icd/repository.hpp` — `icd::Repository` 类
- `src/topology/TopologyDocument.h` — `TopologyDevice` / `TopologyDevicePort` 结构
- `src/topology/TopologyJsonSerializer.cpp` — 拓扑 JSON 序列化
- `docs/规划/帧协议编辑器设计.md` — **需同步更新 LinkTo 部分（见 3.9）**

---

## 8 变更记录

| 版本 | 日期 | 变更 |
|------|------|------|
| V1.0 | 2026-07-06 | 初始方案 |
| V1.1-RC1 | 2026-07-07 | 评审前修订：device 维度改用持久 id；修正 SD1 矛盾；补 nodePath/QString边界/linkTo；新增 M0 |
| V1.1-RC2 | 2026-07-07 | **第一轮评审修订**：SignalRegistry 补全索引填充路径（`registerDevice` + `registerSignals`/`forEachPortBinding`），修复 `resolve()`/`findByNode` 不可用缺口；修正分层错误（`IcdSignalSelection` 放 etest_app）；补全 TopologyDocument 信号（`devicePortAdded/Removed`/`deviceIdChanged`/`findDeviceById`）；补充 TopologyConnection 设备改名连带说明；补充 `documentCleared` 处理、老文件迁移持久化策略、帧协议编辑器 linkTo 协调；修正 SD5 碰撞概率表述 |
| V1.1-RC3 | 2026-07-07 | **第二轮评审（ICD 关系）修订**：`synchronizeFrames` 从 SignalRegistry 移至 etest_app（`SignalSyncHelper`），SignalRegistry 不依赖 `icd_utility` 类型；Node* 缓存方案废弃，改用 etest_app 层 `findNodeByPath`（Phase 5 实现）；新增 3.10 同步时序定义（项目打开/ICD 保存/拓扑绑定变更编排） |
| V1.1-RC4 | 2026-07-07 | **第三轮评审（测试程序）修订**：补充 ISignalSelection 注入机制（`StepTableWidget::setSignalSelection` + `TestProgramEditorWidget` 传播 + EditorManager 工厂回调 + nullptr 守卫）；补充 UUID 显示策略（resolve→名称优先、列宽 180px + elide、tooltip、保存始终存 UUID）；补充 Standalone demo 安全说明；补充 `StepTableWidget::setRegistry` 声明 |
| V1.1-RC5 | 2026-07-07 | **实施完成**：M0~M6 全部实现。M0（StepTableWidget 注入通道）、M1（SignalRegistry + 15 单测）、M2（Topology id/boundFrames + 信号 + 8 单测）、M3（DevicePortBindingDialog + 撤销重做 + 2 单测）、M4（SignalSyncHelper + SignalSelectionDialog + IcdSignalSelection + 5 单测）、M5（EditorManager 注入 + UuidDisplayDelegate）、M6（MainWindow 接线 + 集成验证） |
