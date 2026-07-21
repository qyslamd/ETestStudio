# Linux 平台 CrashHandler 实现方案

## 背景

ETestStudio 的崩溃捕获体系由两套机制组成：

1. **`CrashHandler`**（`src/core/crashhandler/`）：平台相关的崩溃处理器，负责写崩溃日志、调用栈解析、生成 dump、弹友好提示框
2. **`GlobalExceptionHandler`**（`src/core/common/`）：平台无关的信号捕获 + Qt 消息重定向，负责日志记录

`main.cpp` 的初始化顺序：

```
L54: GlobalExceptionHandler::init()   -> 装 signal() 处理器 + Qt 消息重定向
L71: CrashHandler::create()           -> Windows 返回 WindowsCrashHandler，Linux 返回 nullptr
```

当前 Windows 端两套机制协同工作：`CrashHandler` 接管崩溃信号生成富信息日志和 dump，`GlobalExceptionHandler` 兜底处理未被 `CrashHandler` 覆盖的信号并重定向 Qt 消息。

Linux 端存在缺口：

| 能力 | Windows | Linux 现状 |
|------|---------|-----------|
| `CrashHandler::create()` | `WindowsCrashHandler` | **返回 `nullptr`** |
| 崩溃日志文件 | 写 `.log`（含调用栈符号） | 不写 |
| 进程 dump | `.dmp`（MiniDump） | 不生成 |
| 符号解析 | `SymFromAddr` + `SymGetLineFromAddr64` | 不解析 |
| 友好提示框 | `MessageBoxA` | 无 |
| 信号兜底 | `GlobalExceptionHandler` 记录帧数 | 同上，但 `backtrace()` 只数帧数不解析 |

Linux 上崩溃后用户只能看到终端的 `Segmentation fault`，无法定位崩溃位置，严重影响调试效率。本文档记录 Linux 平台 `CrashHandler` 的实现方案。

## 设计目标

1. **与 Windows 实现对称**：复用 `CrashHandler` 抽象基类接口（`init` / `setDumpPath` / `setCrashCallback`），`create()` 在 Linux 上返回 `LinuxCrashHandler`
2. **生成可用崩溃日志**：包含信号信息、故障地址、调用栈符号（函数名 + 偏移）、系统/程序信息
3. **信号处理器安全**：严格遵守 POSIX async-signal-safe 约束，不调用 `malloc`/`new`/`QString`/`QFile` 等
4. **不破坏现有 `GlobalExceptionHandler`**：两者职责分离，互不冲突
5. **可测试**：单元测试验证 `create()` 非空 + 子进程触发 SIGSEGV 验证日志生成

## 技术约束：POSIX 信号处理器安全

这是与 Windows 实现的**根本差异**。Windows 的 SEH（Structured Exception Handler）允许在异常处理代码中使用 C++ 异常捕获，因此 `WindowsCrashHandler::doCrashHandler` 中可以自由使用 `QString`、`QFile`、`MessageBoxA`。

Linux 的信号处理器（`sigaction` 注册的函数）运行在**被中断的线程上下文**中，必须遵守 async-signal-safe 约束：

| 禁止 | 原因 | 安全替代 |
|------|------|----------|
| `malloc` / `new` | 全局堆锁可能被中断线程持有 -> 死锁 | 栈缓冲 / 预分配 |
| `QString` | 内部 `new` 分配 | `char[]` + `snprintf` |
| `QFile` / `QTextStream` | Qt 对象内部有堆操作 | `open()` + `write()` |
| `std::function` 调用 | 可能涉及堆分配 | 函数指针 + 预存状态 |
| `printf` | 内部有 `malloc`（格式化缓冲） | `write()` + `snprintf` |
| `backtrace_symbols()` | 内部 `malloc` | `backtrace_symbols_fd()` 直接写 fd |
| `dladdr()` | 内部可能有锁（best-effort） | 包裹在 try 中或接受可能死锁 |
| `exit()` | 执行 atexit 注册函数，可能二次崩溃 | `_exit()` 直接终止 |

> **非 POSIX 标注**：
> - `snprintf` 在 POSIX.1-2017 的 async-signal-safe 列表中**未列出**，但 glibc 实现不调用 malloc，实践中安全。如需严格标准合规，可用 `write` + 手工整数格式化
> - `backtrace()` / `backtrace_symbols_fd()` 是 GNU 扩展（glibc 特有），非 POSIX 标准。glibc 文档明确说明可在信号处理器中使用，但不可移植到 BSD/macOS
> - `open()` / `write()` / `close()` / `_exit()` / `time()` 均在 POSIX async-signal-safe 列表中，标准保证安全

> **间接修复已有隐患**：`GlobalExceptionHandler::signalHandler`（`GlobalExceptionHandler.cpp:82-125`）现有代码在信号处理器中使用了 `QString`（含 `malloc`），是 async-signal-safety 违规。`LinuxCrashHandler` 的 `sigaction` 覆盖 `signal` 注册后，该处理器不再被调用，从而间接消除了这个隐患。`GlobalExceptionHandler` 仍保留 Qt 消息重定向（`qInstallMessageHandler`）职责，不涉及信号安全。

## 设计方案

### 关键前提：`sigaltstack` 备用栈

SIGSEGV 的常见原因之一是**栈耗尽**（stack overflow / 无限递归）。此时内核尝试调用信号处理器，但栈已满 -> 再次触发 SIGSEGV -> 信号处理器无法执行 -> 进程直接终止，崩溃日志无法生成。

Windows 的 SEH 不存在此问题（操作系统自动切换到 reserve stack）。Linux 必须显式使用 `sigaltstack` + `SA_ONSTACK` 标志：

```cpp
// init() 中分配备用栈（正常运行上下文，可安全 malloc）
stack_t altstack;
altstack.ss_sp = malloc(SIGSTKSZ);   // SIGSTKSZ 通常 8KB，足够 backtrace
altstack.ss_size = SIGSTKSZ;
altstack.ss_flags = 0;
sigaltstack(&altstack, &old_altstack_);

// sigaction 时设标志：
struct sigaction sa;
sa.sa_sigaction = signalHandler;
sa.sa_flags = SA_SIGINFO | SA_ONSTACK;  // 关键：SA_ONSTACK 切换到备用栈
sigemptyset(&sa.sa_mask);
```

析构时 `free(altstack.ss_sp)` 并 `sigaltstack(&old_altstack_, nullptr)` 恢复。

> **`SIGSTKSZ` 大小说明**：本系统 `SIGSTKSZ = 8192`（8KB）。`backtrace()` 64 帧的指针数组占 512 字节，加上信号处理器自身的栈帧和局部变量，8KB 在实践中足够。如需更大余量，可用 `max(SIGSTKSZ, 32768)`。

**不实现 `sigaltstack` 的后果**：栈溢出场景（最常见的 SIGSEGV 子类之一）完全失效，崩溃日志无法生成。

### 两级策略（镜像 Windows 的 `doCrashHandler` + `doCrashFallback`）

#### Tier 1：信号安全层（必定执行）

信号处理器的核心逻辑，**零堆分配**：

1. `backtrace(buffer, N)` -> 栈帧地址存入栈数组
2. `sigaction` 的 `siginfo_t` 提取故障地址（`si_addr`）和信号代码（`si_code`）
3. `snprintf` 组装头部信息到栈 `char[]` 缓冲
4. `open()` 崩溃日志文件（路径在 `init()` 时预存为 `char[]`）
5. `write()` 写入头部 + 预计算系统信息（`init()` 时预存为固定 `char[]` 缓冲，只读）
6. `backtrace_symbols_fd(buffer, frames, fd)` 直接写调用栈到文件 fd（零分配）
7. `write(STDERR_FILENO, ...)` 打印日志路径到终端
8. `_exit(128 + signo)` 终止进程（不用 `exit`，后者会跑 atexit 注册的清理函数，可能再次崩溃）

#### Tier 2：富信息层（best-effort，可选增强）

在 Tier 1 基础上尝试增强（失败则降级为 Tier 1）：

- `dladdr()` 解析每帧的符号名和模块路径 -> 写入日志
  - 风险：`dladdr` 内部可能持锁，如果崩溃时该锁已被占用会死锁
  - 缓解：限制最多解析 32 帧，每帧设超时不可行（信号处理器中不能用 alarm）
  - 决策：**首版不实现 `dladdr`**，仅用 `backtrace_symbols_fd` 输出原始地址和模块偏移，事后用 `addr2line` 解析。后续版本再评估 `dladdr` 的稳定性

### 预计算策略

在 `init()`（正常运行上下文，可自由分配）中预存以下信息到成员变量：

```cpp
// 信号处理器中只读取这些缓冲，不分配
char dump_path_[4096] = {0};          // 崩溃日志目录，init 时拷贝
char precomputed_info_[4096] = {0};   // 系统+程序信息，init 时用 snprintf 填充
size_t precomputed_info_len_ = 0;     // 预计算信息有效长度
int   log_fd_ = -1;                   // 预打开的日志文件 fd（可选，或信号时 open）
```

> **不用 `std::string`**：`std::string` 的 `c_str()` 在信号处理器中读取虽然技术上安全（只读指针），但 `std::string` 内部可能使用 SSO（小字符串优化）或堆分配，读取其内部指针存在不确定性。固定 `char[]` 缓冲 + `size_t` 长度是确定性的、零依赖的方案。

### 信号覆盖关系

```
main.cpp L54: GlobalExceptionHandler::init()
  -> std::signal(SIGSEGV, GlobalExceptionHandler::signalHandler)
  -> std::signal(SIGABRT, GlobalExceptionHandler::signalHandler)
  -> ...

main.cpp L71: LinuxCrashHandler::init()
  -> sigaction(SIGSEGV, {LinuxCrashHandler::signalHandler}, &old_action)
  -> sigaction(SIGABRT, ...)
  -> ...
```

`sigaction` 覆盖 `signal` 的注册，`LinuxCrashHandler` 接管崩溃信号。`GlobalExceptionHandler` 仍保留 Qt 消息重定向职责，两者职责分离互不冲突。

## 接口设计

### `LinuxCrashHandler.h`

```cpp
#include <QtGlobal>
#ifndef Q_OS_WIN

#ifndef ETEST_CORE_CRASHHANDLER_LINUXCRASHHANDLER_H_
#define ETEST_CORE_CRASHHANDLER_LINUXCRASHHANDLER_H_

#include "CrashHandler.h"
#include <csignal>
#include <cstddef>
#include <functional>

namespace etest {
namespace core {
namespace crashhandler {

class LinuxCrashHandler : public CrashHandler {
 public:
  LinuxCrashHandler();
  ~LinuxCrashHandler() override;

  bool init() override;
  void setDumpPath(const QString& path) override;

  /// @note Linux 平台下回调不会在信号处理器中执行（async-signal-safe 约束，
  ///       std::function 不能安全调用）。崩溃信息仅写入日志文件。
  ///       回调保留用于未来 fork 子进程方案。
  void setCrashCallback(std::function<void(const QString& crashLog)> callback) override;

 private:
  // 信号处理器（async-signal-safe）
  static void signalHandler(int signum, siginfo_t* info, void* context);

  // Tier 1：信号安全核心逻辑（零堆分配）
  static void doCrashDump(int signum, siginfo_t* info, void* context);

  // 预存状态（init 时填充，信号处理器中只读）
  static LinuxCrashHandler* s_instance;
  static volatile sig_atomic_t s_inHandler;  // 防重入，POSIX 标准信号安全类型

  // 备用栈（init 时分配，用于栈溢出场景）
  void* altstack_mem_ = nullptr;
  stack_t old_altstack_ = {};  // 零初始化，防止析构时使用未初始化值

  char dump_path_[4096] = {0};          // 崩溃日志目录
  char precomputed_info_[4096] = {0};   // 预计算系统+程序信息
  size_t precomputed_info_len_ = 0;
  std::function<void(const QString&)> crash_callback_;  // 信号处理器中不调用

  // 保存旧信号处理器，析构时恢复。_NSIG = 65（含实时信号），覆盖所有可能的信号编号。
  // sizeof(struct sigaction) = 152（x86_64），数组占用约 10KB，可接受。
  struct sigaction old_actions_[_NSIG];
  bool initialized_ = false;
};

}  // namespace crashhandler
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_CRASHHANDLER_LINUXCRASHHANDLER_H_

#endif  // !Q_OS_WIN
```

> **`sig_atomic_t` vs `std::atomic`**：`volatile sig_atomic_t` 是 POSIX 标准保证可在信号处理器中安全访问的整数类型。`std::atomic<bool>` 在 lock-free 时等效，但 `sig_atomic_t` 是可移植的标准选择。
>
> **`_NSIG` 数组大小**：实测 `_NSIG = 65`（`SIGRTMAX = 64`，`SIGRTMIN = 34`）。用信号号做下标时数组必须覆盖到 `_NSIG`，否则实时信号越界。`sizeof(struct sigaction) = 152`（x86_64），65 个元素约 10KB，作为类成员可接受。
>
> **C++17 兼容性**：`sigaction` 结构体初始化不能用 C99 designated initializer（`.sa_sigaction = ...`），C++17 不支持。必须用逐字段赋值：
> ```cpp
> struct sigaction sa;
> sa.sa_sigaction = signalHandler;
> sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
> sigemptyset(&sa.sa_mask);
> sigaction(signum, &sa, &old_actions_[signum]);
> ```

### `CrashHandler::create()` 修改

```cpp
std::unique_ptr<CrashHandler> CrashHandler::create() {
#ifdef Q_OS_WIN
    return std::make_unique<WindowsCrashHandler>();
#else
    return std::make_unique<LinuxCrashHandler>();
#endif
}
```

## 实现细节

### 1. `init()` 流程

```
1. 设置 dump_path_（默认 AppLocalDataLocation/crash/，与 Windows 一致）
2. mkpath(dump_path_)
3. 预计算 precomputed_info_：用 collectCommonInfo() 生成，snprintf 到固定缓冲
4. 分配备用栈：altstack_mem_ = malloc(SIGSTKSZ)
   sigaltstack({.ss_sp = altstack_mem_, .ss_size = SIGSTKSZ}, &old_altstack_)
5. 对每个目标信号 (SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS)：
     struct sigaction sa;
     sa.sa_sigaction = signalHandler;
     sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
     sigemptyset(&sa.sa_mask);
     sigaction(signum, &sa, &old_actions_[signum])
6. initialized_ = true
```

`SA_SIGINFO` 标志使信号处理器能收到 `siginfo_t`（含 `si_addr` 故障地址、`si_code` 信号原因）。`SA_ONSTACK` 标志使内核在备用栈上调用信号处理器，确保栈溢出场景仍可捕获。

> **信号列表排除 SIGTRAP**：SIGTRAP 由调试器（GDB 断点、单步执行）触发。在 crash handler 中捕获 SIGTRAP 会导致 GDB 下断点时触发 crash handler 而非暂停调试，严重干扰开发。仅处理真正的崩溃信号。
>
> **`si_addr` 有效性**：`siginfo_t->si_addr` 仅对硬件异常信号有意义：

| 信号 | `si_addr` 含义 |
|------|----------------|
| SIGSEGV | 故障内存地址 ✅ |
| SIGBUS | 故障地址 ✅ |
| SIGFPE | 错误指令地址 ✅ |
| SIGILL | 非法指令地址 ✅ |
| SIGABRT | **无意义**，不读取 |

### 2. `signalHandler()` 流程

```
1. 检查 s_inHandler，若非零说明嵌套崩溃 -> 直接 _exit(128 + signo)
2. s_inHandler = 1
3. 调用 doCrashDump(signum, info, context)
4. _exit(128 + signo)   // 不走 atexit，避免二次崩溃
```

### 3. `doCrashDump()` 流程（Tier 1 核心）

```
1. void* buffer[64]; int frames = backtrace(buffer, 64);
2. char filepath[4096];
   snprintf(filepath, sizeof(filepath), "%s/etest_crash_%lld.log",
            dump_path_, (long long)time(nullptr));
3. int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
4. 写入头部（栈缓冲 char[512] + snprintf + write）：
   - 信号名称、编号、si_code
   - 故障地址 si_addr（仅对 SIGSEGV/SIGBUS/SIGFPE/SIGILL 读取，SIGABRT 跳过）
   - 帧数
5. write(fd, precomputed_info_, precomputed_info_len_)   // 写预计算系统信息
6. backtrace_symbols_fd(buffer, frames, fd)   // 直接写 fd，零分配
7. close(fd)
8. write(STDERR_FILENO, "崩溃日志已写入: ...\n") 给用户提示
```

### 4. 析构

```
~LinuxCrashHandler():
  对每个信号 sigaction(signum, &old_actions_[signum], nullptr)  恢复
  sigaltstack(&old_altstack_, nullptr)  恢复原备用栈
  free(altstack_mem_)                   释放备用栈内存
  s_instance = nullptr
```

### 5. `crash_callback_` 的处理

`std::function` 不能在信号处理器中调用。处理策略：

- `setCrashCallback()` 仍存储回调（与 Windows 接口一致）
- 信号处理器中**不调用**回调
- 在 `doCrashDump` 写完日志后，写一行提示到日志末尾：「回调因信号安全约束未执行，请查看日志文件」
- 后续版本可考虑 `fork()` 子进程执行回调（子进程有自己的地址空间，可安全调用），但首版不做

## 涉及文件

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/core/crashhandler/LinuxCrashHandler.h` | 新增 | 类声明 |
| `src/core/crashhandler/LinuxCrashHandler.cpp` | 新增 | `sigaction` + `backtrace` 实现 |
| `src/core/crashhandler/CrashHandler.cpp` | 修改 | `create()` 的 `#else` 分支返回 `LinuxCrashHandler` |
| `src/core/CMakeLists.txt` | 修改 | `else()` 分支加入 `LinuxCrashHandler.cpp/.h` |
| `tests/core/crash_handler_test.cpp` | 新增 | 单元测试 |
| `tests/core/CMakeLists.txt` | 修改 | 注册新测试 |

## 依赖

| 依赖 | 用途 | 系统包 |
|------|------|--------|
| `execinfo.h` | `backtrace()` / `backtrace_symbols_fd()` | glibc 自带 |
| `signal.h` | `sigaction` / `siginfo_t` | glibc 自带 |
| `dlfcn.h` | `dladdr()`（首版不用，预留） | glibc 自带 |
| `unistd.h` | `open` / `write` / `_exit` | glibc 自带 |

**无需新增第三方依赖。**

## 测试方案

### 单元测试：`test_core_crash_handler`

| 测试用例 | 验证内容 |
|----------|----------|
| `CreateReturnsNonNull` | `CrashHandler::create()` 在 Linux 上返回非空 `unique_ptr` |
| `InitReturnsTrue` | `init()` 返回 true，`initialized_` 为 true |
| `SetDumpPathCreatesDir` | `setDumpPath(tmp)` 后目录存在 |
| `GenerateCrashFileName` | 文件名格式 `etest_crash_YYYYMMDD_HHmmss.log` |
| `CollectCommonInfo` | 返回字符串包含「操作系统」「程序路径」 |

### 集成测试：崩溃日志生成（子进程）

```
1. fork() 子进程
2. 子进程：init() + 触发 SIGSEGV（解引用 nullptr）
3. 父进程：waitpid()，检查退出码 == 128 + SIGSEGV
4. 父进程：检查 dump 目录下存在崩溃日志文件
5. 父进程：读取日志，验证包含 "SIGSEGV" 和 "backtrace" 字样
```

### 集成测试：栈溢出场景（验证 sigaltstack）

```
1. fork() 子进程
2. 子进程：init() + 无限递归调用 -> 栈耗尽 -> SIGSEGV
3. 父进程：waitpid()，检查退出码 == 128 + SIGSEGV
4. 父进程：检查崩溃日志文件存在（若无 sigaltstack 则无法生成）
5. 父进程：读取日志，验证包含 "SIGSEGV"
```

> 此测试验证 `sigaltstack` 的关键作用：不分配备用栈时，栈溢出场景信号处理器无法执行，日志不会生成。

### 手动验证

1. 编译运行 ETestStudio
2. `kill -SEGV <pid>` 发送段错误信号
3. 检查 `~/.local/share/ETestStudio/crash/` 下生成日志
4. 用 `addr2line -e bin/ETestStudio -f -C <地址>` 解析日志中的地址到源码行

## 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 崩溃时堆已损坏 -> `backtrace` 可能死锁 | 日志无法生成 | `s_inHandler` 防重入 + `_exit` 兜底 |
| 栈溢出 -> 信号处理器在满栈上执行失败 | 日志无法生成 | `sigaltstack` 分配备用栈 + `SA_ONSTACK` |
| `dladdr` 持锁死锁 | 信号处理器卡住 | 首版不使用 `dladdr`，仅输出原始地址 |
| `open` 失败（磁盘满/权限） | 日志无法写入 | 降级为 `write(STDERR_FILENO, ...)` 打印到终端 |
| 与 `GlobalExceptionHandler` 信号冲突 | 重复处理 | `sigaction` 覆盖 `signal`，`LinuxCrashHandler` 接管 |
| `fork` 子进程执行回调（未来增强） | 子进程状态不确定 | 首版不实现 |
| SIGTRAP 干扰 GDB 调试 | 断点触发 crash handler | 信号列表排除 SIGTRAP |

## 不做的事

1. **不生成 core dump 文件**：由系统 `ulimit -c` 控制，应用层不干预。崩溃日志已含调用栈
2. **不实现 `dladdr` 符号解析**：风险高收益低，事后 `addr2line` 即可
3. **不实现 GUI 提示框**：信号处理器中不能创建 Qt 对象，终端提示已足够
4. **不调用 `crash_callback_`**：`std::function` 不安全，日志文件替代

## 后续演进

| 版本 | 增强项 |
|------|--------|
| V2 | `fork()` 子进程执行 `crash_callback_` + GUI 提示 |
| V2 | `dladdr` 符号解析（带超时保护） |
| V3 | 集成 Breakpad/Crashpad 生成 minidump |

## 设计评审记录

以下问题在首次设计评审中发现并已纳入方案：

| 编号 | 严重程度 | 问题 | 处置 |
|------|----------|------|------|
| R1 | 🔴 高 | `old_actions_[32]` 数组大小不足（`_NSIG=65`） | 改为 `old_actions_[_NSIG]` |
| R2 | 🔴 高 | 缺少 `sigaltstack`，栈溢出场景信号处理器无法执行 | 新增备用栈分配 + `SA_ONSTACK` 标志 |
| R3 | 🟡 中 | `snprintf` 非 POSIX async-signal-safe | 加注 glibc 实现安全，标准合规降级方案备选 |
| R4 | 🟡 中 | `backtrace` / `backtrace_symbols_fd` 是 GNU 扩展非 POSIX | 加注不可移植到 BSD/macOS |
| R5 | 🟡 中 | `std::atomic<bool>` 应为 `volatile sig_atomic_t` | 改用 POSIX 标准类型 |
| R6 | 🟡 中 | SIGTRAP 干扰 GDB 调试 | 信号列表移除 SIGTRAP |
| R7 | 🟡 中 | `si_addr` 对 SIGABRT 无意义 | doCrashDump 中按信号类型条件读取 |
| R8 | 🟡 中 | `std::string` 预计算信息在信号处理器中读取有隐患 | 改为固定 `char[]` + `size_t` 长度 |
| R9 | 🟡 中 | `setCrashCallback` API 契约未标注 Linux 限制 | 头文件加 `@note` 注明回调不执行 |
| R10 | 🟡 中 | `GlobalExceptionHandler` 现有 `QString` 信号安全违规 | 文档说明 `sigaction` 覆盖后间接消除 |
| R11 | 🟡 中 | Tier 1 描述残留 `std::string`，与预计算策略的 `char[]` 不一致 | 修正描述为「固定 `char[]` 缓冲」 |
| R12 | 🟡 中 | `init()` 伪代码用 C99 designated initializer（`.sa_flags = ...`），C++17 不支持 | 改为逐字段赋值 + 加 C++17 兼容性注释 |
| R13 | 🟡 低 | `old_altstack_` 未零初始化，析构可能使用垃圾值 | 改为 `stack_t old_altstack_ = {}` |
| R14 | 🟡 低 | 未说明 `SIGSTKSZ` 实际值（8KB）是否足够 | 加注实测值 + 余量评估 |
| R15 | 🟡 低 | 未说明 `old_actions_[_NSIG]` 内存占用 | 加注 `sizeof` 实测值（约 10KB） |
