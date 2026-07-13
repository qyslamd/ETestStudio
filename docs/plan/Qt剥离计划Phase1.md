# Qt 剥离计划 — Phase 1：数据类型本地化

> 目标：将 `etest_core` 中的 Qt 专有数据类型替换为 C++ 标准库等价物，
> 保留 QObject 继承体系和信号槽机制不动。
> 逐步减少 `etest_core` 对 Qt 的编译依赖，为后续彻底解耦做准备。

---

## 总原则

1. **只改数据类型**：`QString → std::string`、`QJson* → nlohmann::json`、`QDir/QFile → std::filesystem` 等
2. **保留 QObject**：所有继承 QObject 的类保持原样，信号/槽不改
3. **不动机制**：`QPluginLoader`、`QSettings`、`Q_DECLARE_INTERFACE`、`QTimer`、`QCoreApplication` 等运行时机制不改
4. **不动 Terminal**：`VtParser`、`PtyProcess`、`ConPtyProcess` 重度依赖 Qt 事件机制，Phase 1 不动
5. **CMake 不改**：`AUTOMOC ON` 保留，`Qt5::Core Qt5::Network` 链接保留

### 🔑 关键规则：信号参数保持 QString

Phase 1 中，所有信号的参数**保持 `QString` 不变**。
emit 信号的代码在发送前做一次 `QString::fromStdString(str)` 转换，接收端收到 `QString`
后在 handler 内部按需转回 `std::string`。

理由是：信号签名变更会断裂所有 `connect()` 调用方（跨越 `etest_app`、`engine` 等模块），
超出了 Phase 1 "etest_core 内部数据类型替换" 的控制范围。

### 🔑 关键规则：插件接口保持 QByteArray

`ISerialDevicePlugin`、`ICANPlugin`、`IArinc429Plugin`、`IADevicePlugin`、`IDADevicePlugin`
等纯虚接口的**方法签名保持 `QByteArray` 不变**。原因是：
- 接口的实现方跨越 `examples/plugins/mock_*` 和未来真实硬件插件
- 改接口等同于改所有插件实现，超出 Phase 1 范围

etest_core 内部使用时在接口边界做 `QByteArray ↔ std::vector<std::byte>` 转换。

---

## Phase 1 改什么、不改什么

### ✅ 改

| Qt 类型 | 替换为 |
|---------|--------|
| `QString` | `std::string` |
| `QStringList` | `std::vector<std::string>` |
| `QByteArray` | `std::vector<std::byte>`（数据）或 `std::string`（文本） |
| `QVector<T>` / `QList<T>` | `std::vector<T>` |
| `QHash<K,V>` / `QMap<K,V>` | `std::unordered_map<K,V>` |
| `QSet<T>` | `std::set<T>` 或 `std::unordered_set<T>` |
| `QPair<A,B>` | `std::pair<A,B>` |
| `QMutex` | `std::mutex` |
| `QJson*` (Object/Array/Document) | `nlohmann::json`（3rdparty 已有） |
| `QDir` / `QFile` / `QFileInfo` | `std::filesystem` |
| `QDateTime` | `<chrono>` + 手写格式化 |
| `QTextStream` | `std::ofstream` / `std::stringstream` |
| `QTextCodec` | `MultiByteToWideChar`（Win32）/ `<codecvt>` |
| `QCryptographicHash` | Windows `BCrypt` API（BCryptHash）|
| `QDebug` | spdlog |
| `QLocale` | C++ locale / 手写 |
| `QVariant` | `std::any` 或 `nlohmann::json` |
| `QDataStream` | 仅 SingleInstance 使用，Phase 1 不动 |
| `QVariantMap` | `std::map<std::string, nlohmann::json>` |
| `QPointer` | `std::weak_ptr` |
| `QChar` | `char32_t` 或 `std::string` 处理 |
| `qint64` / `quint64` / `qint32` 等 | `int64_t` / `uint64_t` / `int32_t` |
| `QtGlobal` / `QtEndian` | `<bit>` / 手写字节序 |
| `QFileInfoList` | `std::vector<std::filesystem::directory_entry>` |

### ❌ 不改（Phase 1 范围外）

| 保留项 | 原因 |
|--------|------|
| `QObject` 继承 + 信号槽 | Phase 2 |
| `QPluginLoader` + `Q_DECLARE_INTERFACE` | Phase 3 |
| `QLocalServer` / `QLocalSocket` / `QSharedMemory` | SingleInstance，Phase 3 |
| `QWinEventNotifier` | ConPTY，Phase 3 |
| `QTimer` | BackupManager，Phase 2 |
| `QSettings` | ConfigManager，替换为 nlohmann::json 工作量偏大 |
| `QCoreApplication` | 应用级基础设施 |
| `QMessageLogContext` / `QtMsgType` | Qt 消息 handler |
| `QSysInfo` | CrashHandler 系统信息 |
| `QMetaType` / `Q_DECLARE_METATYPE` | AD 插件接口 |

---

## 模块分档清单

### 🟢 第一批：纯数据类（9组，无 QObject，无文件 IO，无机制调用）

这些文件只用了 `QString` / `QList` 等字符串类型，是机械替换，风险最低。

| # | 文件 | 涉及的 Qt 类型 | 预计改动量 |
|---|------|---------------|-----------|
| 1 | `common/FileException.h/.cpp` | `QString` | 极小（10行） |
| 2 | `common/TimeException.h/.cpp` | `QString` | 极小（10行） |
| 3 | `common/StringException.h/.cpp` | `QString` | 极小（10行） |
| 4 | `common/ByteException.h/.cpp` | `QString` | 极小（10行） |
| 5 | `utils/ByteUtil.h/.cpp` | `QByteArray`, `QString`, `QtGlobal`, `QtEndian`, `quint16/32/64`, `QStringList`, `QChar` | 中（50行） |
| 6 | `auth/AuthTypes.h` | `QString` | 极小（2行） |
| 7 | `auth/PasswordHasher.h/.cpp` | `QString` | 小（10行） |
| 8 | `config/ConfigDefs.h` | 无 | 无需改 |
| 9 | `plugin/PluginMetaData.h` | `QString`, `QStringList` | 小（8行） |

### 🟡 第二批：有文件 IO / 机制调用的非 QObject 类（8组）

这些类涉及文件读写、JSON 序列化、或调用了 `QCoreApplication` / `QStandardPaths` 等 Qt 机制。
需要 `std::filesystem` + `nlohmann::json` 组合替换。涉及 Qt 机制调用的部分保留，仅替换数据传参类型。

| # | 文件 | 涉及的 Qt 类型 | 预计改动量 |
|---|------|---------------|-----------|
| 10 | `utils/FileUtil.h/.cpp` | `QString`, `QStringList`, `qint64`, `QFile`, `QTextStream`, `QCryptographicHash`, `QFileInfo`, `QIODevice` | 大（200行） |
| 11 | `utils/TimeUtil.h/.cpp` | `QString`, `qint64`, `QDateTime`, `QStringList` | 中（80行） |
| 12 | `utils/StringUtil.h/.cpp` | `QString`, `QStringList`, `QByteArray`, `QChar`, `QTextCodec`, `QLocale` | 中（100行） |
| 13 | `auth/UserManager.h/.cpp` | `QString`, `QList`, `QDir`, `QFile`, `QFileInfo`, `QJsonArray/Document/Object`, `QStandardPaths`, `QIODevice`, `QByteArray` | 中（120行） |
| 14 | `project/ProjectInfo.h/.cpp` | `QString`, `QDateTime`, `QJsonObject`, `QStringList`, `QVariantMap`, `QDir`, `QFile`, `QFileInfo`, `QJsonArray/Document` | 大（150行） |
| 15 | `crashhandler/CrashHandler.h/.cpp` | `QString`, `QDateTime`, `QSysInfo`, `QCoreApplication`, `QStandardPaths` | 中（60行） |
| 16 | `crashhandler/WindowsCrashHandler.h/.cpp` | `QString`, `QDir`, `QFile`, `QDebug`, `QCoreApplication`, `QStandardPaths` | 中（80行） |
| 17 | Plugin 接口层（6个 .h） | `QByteArray`, `QString`, `QVector`, `quint32`, `Q_DECLARE_INTERFACE`, `Q_DECLARE_METATYPE` | 小（签名保持 QByteArray，仅改纯数据传参） |

### 🟠 第三批：QObject 子类（12组，只改数据类型不动信号槽）

继承 QObject 的文件。根据关键规则，信号参数保持 `QString`，emit 处做
`QString::fromStdString()` 转换，不影响调用方。

| # | 文件 | 除 QObject 外的 Qt 类型 | 备注 |
|---|------|------------------------|------|
| 18 | `logger/LogHistoryBuffer.h/.cpp` | `QList`, `QMutex`, `QString`, `QPointer` | QObject + 信号 `drained` |
| 19 | `auth/AuthService.h/.cpp` | `QString`, `QSet<QString>` | 信号 `loginSucceeded(const User&)` 等 |
| 20 | `config/ConfigManager.h/.cpp` | `QString`, `QVariant`, `QMap`, `QDir`, `QFile`, `QJson*` | 保留 `QSettings` 不变 |
| 21 | `plugin/PluginManager.h/.cpp` | `QString`, `QStringList`, `QList`, `QMap`, `QDir`, `QJson*` | 保留 `QPluginLoader` 不变 |
| 22 | `project/ProjectManager.h/.cpp` | `QDir`, `QFile`, `QFileInfo`, `QJson*`, `QStringList` | 信号含 `QString` 参数 |
| 23 | `backup/BackupManager.h/.cpp` | `QString`, `QFileInfo`, `QTimer`, `QDir`, `QFile`, `QDateTime` | 保留 `QTimer` 不变 |
| 24 | `common/GlobalExceptionHandler.h/.cpp` | `QString`, `QDebug` | 信号参数含 `QString`；`qtMessageHandler` 回调签名由 Qt 定义，其中的 `QString` 必须保留 |
| 25 | `logger/Logger.h/.cpp` | `QMutex`, `QString`, `QDateTime`, `QDir`, `QStandardPaths` | 静态类，无信号槽 |
| 26 | `logger/QtConsoleSink.h/.cpp` | `QObject`, `QString`（signal 参数） | spdlog sink |
| 27 | `SignalRegistry.h/.cpp` | `QHash`, `QPair`, `QString`, `QStringList`, `QVector`, `QCryptographicHash` | **核心模块，影响面最大** |

### ⏸️ 留存不动（Phase 2/3）

| 模块 | 原因 |
|------|------|
| `common/SingleInstance.h/.cpp` | 重度依赖 `QLocalServer`/`QLocalSocket`/`QSharedMemory`，Phase 3 |
| `terminal/VtParser.h/.cpp` | QObject + 大量信号 + 字节流处理 |
| `terminal/PtyProcess.h/.cpp` | QObject + 信号 |
| `terminal/ConPtyProcess.h/.cpp` | 依赖 `QWinEventNotifier`，仅 Windows |
| `terminal/PosixPtyProcess.h/.cpp` | 同上模式，仅 Linux（当前未用） |

---

## 影响面分析

### 当前 `etest_core` 被哪些模块引用

检查 `etest_core` 的消费者——这些模块都要跟着改：

```
etest_app (主程序)        → 最高频引用
etest_topology            → 中频
etest_protocol            → 中频
etest_engine              → 高频（SignalRegistry、HardwareManager）
tools/test-executor-cli   → 低频
tools/topology-editor     → 低频
tools/protocol-editor     → 低频
tools/test-program-editor → 低频
```

### 关键风险点

1. **`SignalRegistry::computeUuid()` — SHA1 必须 bit 一致** — UUID 是 32 字符 SHA1 hex，
   外部 Python 工具也生成同样的 UUID 做测试用例。替换 `QCryptographicHash` 后，
   新的 SHA1 实现必须对相同输入产生完全相同的 160-bit 输出。
   **建议：使用 Windows `BCrypt` API（`BCryptHash`），不自己实现 SHA1。**

2. **信号参数保持 QString —— 这条是硬约束** — 见关键规则。emit 处 `QString::fromStdString()`，
   接收 handler 内 `str.toStdString()`。不要在信号签名里引入 `std::string`。

3. **插件接口保持 QByteArray** — 见关键规则。在 `etest_core` 内部接口边界做
   `QByteArray ↔ std::vector<std::byte>` 转换。不要改 `ISerialDevicePlugin` 等纯虚接口的签名。

4. **`std::filesystem` vs `QDir` 行为差异** — `QDir::filePath("a", "b")` 返回 `a/b`，
   `std::filesystem::path("a") / "b"` 返回 `a/b`，基本相同。但 `QDir::entryList` 的过滤行为
   与 `std::filesystem::directory_iterator` 不同，需要逐项检查。特别是：
   - `QDir::setNameFilters({"*.log"})` → 需手写 `path.extension() == ".log"`
   - `QDir::entryInfoList(filters, QDir::Files)` → `is_regular_file(entry)`

5. **`nlohmann::json` vs `QJsonObject` API 差异** — 虽然 3rdparty 已有，但某些操作需要适配，
   比如 `QJsonObject::value("key").toString("default")` 对应 `json.value("key", "default")`，
   `QJsonDocument::fromJson(data)` 对应 `nlohmann::json::parse(data)`。

6. **`QDateTime` 替换受限于 C++17** — MSVC 2019 C++17 模式下 `std::chrono` 没有 `format()`
   （C++20 特性）。日期格式化使用 C 标准库 `strftime`，时间戳运算使用 `<chrono>`。
   自定义日期格式化字符串（如 `"yyyy-MM-dd HH:mm:ss"`）需要手写映射到 `strftime` 格式。

7. **`QStringList` → `std::vector<std::string>` API 差异** — `QStringList` 有 `removeAll()`、`prepend()`、`join()` 等便捷方法，`std::vector` 没有。替换后需改用 erase-remove idiom 或 `std::ranges`。常见模式对照：
   - `list.removeAll(x)` → `std::erase(vec, x)`（C++20）或 `vec.erase(std::remove(...))`
   - `list.prepend(x)` → `vec.insert(vec.begin(), x)`
   - `list.join(sep)` → 手写循环或用 `fmt::join`

8. **`QCryptographicHash` 替换为 Windows BCrypt** — `FileUtil` 用到 MD5 和 SHA256，
   `SignalRegistry` 用到 SHA1。不使用手写实现，改用 Windows `BCrypt` API：
   - `BCryptOpenAlgorithmProvider(alg, L"SHA1", ...)`
   - `BCryptHash(alg, NULL, 0, data, len, result, resultLen)`
   对于跨平台需求，后续再抽象。目前仅有 Windows 目标。

---

## 执行顺序建议

```
Phase 1 执行路径：

第一批（纯数据类，无外部依赖）
  ├── common/FileException          ← 最简单，先练手
  ├── common/TimeException
  ├── common/StringException
  ├── common/ByteException
  ├── auth/AuthTypes
  ├── auth/PasswordHasher
  ├── config/ConfigDefs               ← 已无需改，确认即可
  ├── plugin/PluginMetaData
  └── utils/ByteUtil

第二批（有文件 IO / 机制调用）
  ├── utils/TimeUtil
  ├── utils/FileUtil                  ← 核心工具类，其他模块依赖
  ├── utils/StringUtil                ← 编码转换依赖 Win32 API
  ├── auth/UserManager                ← 依赖 FileUtil + TimeUtil 改完后
  ├── project/ProjectInfo             ← 依赖 FileUtil + nlohmann/json
  ├── crashhandler/CrashHandler
  ├── crashhandler/WindowsCrashHandler
  └── plugin/接口层                   ← 签名保持 QByteArray，仅改传参数据类型

第三批（QObject 子类，信号参数保持 QString 不动）
  ├── logger/LogHistoryBuffer
  ├── auth/AuthService
  ├── logger/Logger                   ← 静态类，不影响调用方接口
  ├── logger/QtConsoleSink
  ├── SignalRegistry                  ← 影响面最大，需协调 engine 层修改
  ├── config/ConfigManager
  ├── plugin/PluginManager
  ├── project/ProjectManager
  ├── backup/BackupManager
  └── common/GlobalExceptionHandler
```

---

## 附录：CMake 当前配置

```cmake
target_link_libraries(etest_core PUBLIC
    Qt5::Core
    Qt5::Network
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(etest_core PRIVATE dbghelp)
endif()
```

Phase 1 完成后仍保留 `Qt5::Core` 和 `Qt5::Network` 链接，因为 QObject 继承还在。
后续 Phase 2（信号槽解耦）完成后才可能移除。
