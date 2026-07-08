# Ideas / 待办想法

记录开发过程中发现但暂不处理的改进点、技术债、待办想法。
每条注明发现日期、来源、现状与建议，便于后续按优先级安排。

---

## 安装包单实例 mutex 与架构后缀

- **发现日期**: 2026-07-02
- **来源**: make_install_package.iss 双架构改造（commit 56d928a 后续）
- **现状**:
  - `src/app/make_install_package.iss` 中 `AppMutex=ETestStudioAppMutex` 和 `CheckForMutexes('{#MyAppMutex}')` 是**死代码**。
  - app 实际单实例机制是 `src/core/common/SingleInstance.cpp` 里的 `QLocalServer` + `QSharedMemory`（key 为 `"ETestStudio"`），从不创建名为 `ETestStudioAppMutex` 的 Windows mutex。
  - 因此安装时的 mutex 检测永远不触发，`AppMutex` 指令也永不生效。
- **风险**:
  - 若以后 app 改用 Windows mutex 做单实例（或补充 mutex 检测），且 mutex 名固定为 `ETestStudioAppMutex`，则 x86 和 x64 版本会互相误判对方在运行——本架构包安装时把另一架构的运行实例当成自己，提示关闭。
- **建议**:
  - 现状可保留死代码不动（不影响功能）。
  - 若以后引入 mutex：让 mutex 名带架构后缀，例如 `ETestStudioAppMutex-x64` / `ETestStudioAppMutex-x86`。.iss 里通过已有的 `MyAppArch` 宏拼接：`#define MyAppMutex "ETestStudioAppMutex-" + MyAppArch`，app 侧 `CreateMutex` 时用同名。
  - 同步评估 `QLocalServer`/`QSharedMemory` 的 key 是否也需要架构后缀——目前 x86/x64 共用 `"ETestStudio"`，意味着两个架构版本当前就会互相当成单实例（同一台机器先开 x64 再开 x86，x86 会被 x64 拦下）。这是另一个待评估的问题，见下条。

---

---

## SignalRegistry 四本内部索引的一致性风险

- **发现日期**: 2026-07-08
- **来源**: UUID V1.1 实现代码审查
- **现状**:
  - `SignalRegistry` 内部维护四个索引（`uuid_index_`、`port_to_frames_`、`device_names_`、`node_to_uuids_`），mutations 散布在 6 个方法中，无显式不变式约束。
  - 代码审查已发现 `unbindPort` 错误删除整个 `node_to_uuids_` key 的 bug（多设备共享同一节点路径时其他设备的索引被连带清除）。
- **风险**: 后续新增 `removeDevice`、增量绑定更新等方法时，大概率又会引入类似的不一致。
- **建议**:
  - 考虑将四个索引的写入收敛到少数内部方法，每个方法执行前后用 `assert`/`checkInvariants()` 校验一致性（如 `node_to_uuids_` 中每个 UUID 必在 `uuid_index_` 中存在）。
  - 后续若出现第三个索引不一致的 bug，则重构为单一数据源 + 派生视图的模式。

---

## SignalSelectionDialog 存储裸 Node* 指针

- **发现日期**: 2026-07-08
- **来源**: UUID V1.1 实现代码审查
- **现状**:
  - `SignalSelectionDialog::populateNodes` 用 `reinterpret_cast<quintptr>(node)` 将 `icd::Node*` 存入 `QStandardItem::UserRole`，选中节点时又 `reinterpret_cast` 取回。
  - 方案 3.7.3 明确反对此做法："Node* 的生命周期由 Repository 管理，Repository 重建后裸指针立即 dangling"，但当前 dialog 是 modal `exec()` 同步使用，生命周期安全。
- **风险**: 若未来将 dialog 改为非模态、缓存选中结果、或跨异步操作复用，裸指针将成为段错误的定时炸弹。
- **建议**:
  - 保持现状（modal 短生命周期没问题）。
  - 若以后改为非模态，需移除裸指针存储，改为存 nodePath(QString)，用时通过 `findNodeByPath` 定位。

---

## IcdSignalSelection / synchronizeRegistry 未经运行验证

- **发现日期**: 2026-07-08
- **来源**: UUID V1.1 实现代码审查
- **现状**:
  - `IcdSignalSelection`、`SignalSelectionDialog`、`synchronizeRegistry` 已编译通过但从未在运行时执行——`MainWindow` 的 `icd_repository_` 始终为 null。
  - `IcdSignalSelection` 降级为 `DefaultSignalSelection` 文本输入；`synchronizeRegistry` 不会被调用；`PropertyPanelWidget::available_icd_frames_` 未被赋值，"绑定 ICD 帧..."按钮弹出空列表。
- **风险**: 这些代码在实际 ICD 数据下的行为未经验证，未来接入时可能发现隐藏问题。
- **建议**:
  - 接入 ICD Repository 后，先手动验收以下场景：
    1. 打开含 ICD 协议的项目 → `synchronizeRegistry` 正确建立索引
    2. 信号选择对话框显示实际 ICD 帧树
    3. 拓扑端口绑帧后 registry 同步更新
  - 补充集成测试覆盖完整链路。

---

## x86 / x64 版本互相视为同一实例（QLocalServer/SharedMemory key 共用）

- **发现日期**: 2026-07-02
- **来源**: 同上
- **现状**: `SingleInstance("ETestStudio")` 在 x86 和 x64 构建里用同一个 key。
  - `QLocalServer` 监听名 = `"ETestStudio"`
  - `QSharedMemory` key = `"ETestStudio"`
- **影响**: 同一台机器同时装了 x86 和 x64 两个版本时，先启动的版本会拦下后启动的另一架构版本（被当成"已有实例运行"并转发参数）。对于"共存"目标（见 make_install_package.iss 双架构改造）是矛盾的。
- **建议**:
  - 若希望 x86/x64 真正独立共存，`SingleInstance` 的 key 应带架构标识，例如 `"ETestStudio-x64"` / `"ETestStudio-x86"`。可在编译期通过宏（如 `CMAKE_SIZEOF_VOID_P` → `-DAPP_ARCH_SUFFIX`）注入，或运行期检测 `sizeof(void*)`。
  - 若不希望共存运行（只允许装一个），则当前行为符合预期，可忽略本条；但需与安装包的"共存"策略对齐——目前 .iss 已改为共存（不同 AppId），与单实例 key 共用存在语义冲突，需明确取舍。
