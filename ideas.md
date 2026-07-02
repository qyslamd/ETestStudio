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
