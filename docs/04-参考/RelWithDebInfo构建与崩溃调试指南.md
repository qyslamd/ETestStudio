# RelWithDebInfo 构建与崩溃 Dump 调试指南

## 1. 什么是 RelWithDebInfo

RelWithDebInfo（Release With Debug Info）是 CMake 的一种构建类型，介于 Debug 和 Release 之间：

| 构建类型 | 优化 | 调试信息 | PDB文件 | 适用场景 |
|----------|------|----------|---------|----------|
| Debug | 无(/Od) | 完整 | 有 | 日常开发调试 |
| Release | 全优化(/Ox) | 无 | 无 | 最终发布 |
| RelWithDebInfo | 优化(/O2) | 完整 | 有 | 发布带调试信息 |

RelWithDebInfo 的核心价值：**程序以接近 Release 的性能运行，但崩溃时生成的 .dmp 文件配合 PDB 可以在任意计算机上还原完整调用栈和源码位置**。Release 构建没有 PDB，崩溃后只能看到地址，无法定位问题。

---

## 2. 构建 RelWithDebInfo 版本

### 2.1 使用构建脚本

```cmd
scripts\build_ninja.bat relwithdebinfo
```

脚本支持三种构建类型：

| 命令 | 构建类型 | 输出目录 |
|------|----------|----------|
| `build_ninja.bat` | Debug | `build\ninja-debug\` |
| `build_ninja.bat relwithdebinfo` | RelWithDebInfo | `build\ninja-relwithdebinfo\` |
| `build_ninja.bat release` | Release | `build\ninja-release\` |

### 2.2 手动 CMake 命令

```cmd
:: 在 x64 Native Tools Command Prompt for VS2019 中执行

:: 配置
cmake -S . --preset ninja-relwithdebinfo

:: 构建
cmake --build build\ninja-relwithdebinfo
```

### 2.3 构建产物

构建完成后，关键文件位于 `build\ninja-relwithdebinfo\bin\`：

```
build\ninja-relwithdebinfo\bin\
├── etest_demo.exe          # 主程序
├── etest_demo.pdb          # 主程序调试符号 ← 关键文件
├── core.pdb                # 核心库调试符号
└── tests\
    ├── test_core_crashhandler.exe
    ├── test_core_crashhandler.pdb
    └── ...
```

**重要：发布时必须把 .exe 和对应的 .pdb 文件一起保存。PDB 是调试 dump 的唯一依据，丢了就无法还原符号信息。**

---

## 3. 崩溃时自动生成的文件

程序崩溃时，`WindowsCrashHandler` 会在以下目录生成两个文件：

```
C:\Users\<用户名>\AppData\Local\etest_demo\crash\
├── etest_crash_20260509_143052.log   # 文本崩溃日志（异常代码、寄存器、调用栈）
└── etest_crash_20260509_143052.dmp   # MiniDump 文件（完整进程内存快照）
```

### 3.1 文本日志内容

.log 文件包含：
- 异常代码和异常地址
- CPU 寄存器上下文（RAX/RBX/RCX.../RIP）
- 调用栈（函数名 + 源码文件:行号，依赖 PDB 才能解析）
- 系统信息（OS版本、CPU、内存）
- 进程信息（路径、PID、命令行）

### 3.2 MiniDump 内容

.dmp 文件包含：
- 所有线程的栈内存
- 进程虚拟内存布局信息
- 加载的模块列表
- 句柄信息
- 数据段内容

当前配置的 dump 类型为：
```cpp
MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo
```

这个级别在文件大小和调试信息之间做了平衡，通常 10-50MB，足以还原完整调用栈和局部变量。

---

## 4. 使用 Visual Studio 调试 Dump

### 4.1 打开 Dump 文件

1. 启动 Visual Studio 2019
2. 文件 → 打开 → 文件（Ctrl+O）
3. 选择 `.dmp` 文件
4. VS 会显示一个摘要页面，包含异常信息和系统信息

### 4.2 设置符号路径

1. 在摘要页面点击 **"设置符号路径"**
2. 添加 PDB 所在目录：`D:\trae_workspace\etest-demo\build\ninja-relwithdebinfo\bin`
3. （可选）添加 Microsoft 公共符号服务器：`https://msdl.microsoft.com/download/symbols`

也可以通过菜单设置：工具 → 选项 → 调试 → 符号

### 4.3 设置源码路径

1. 工具 → 选项 → 调试 → 常规
2. 取消勾选 **"要求源文件与原始版本完全匹配"**（如果源码有改动）
3. 工具 → 选项 → 调试 → 符号 → 源码服务器（可选）

### 4.4 开始调试

1. 在 dump 摘要页面点击 **"仅使用本机进行调试"**
2. VS 会加载符号并停在崩溃点
3. 可以查看：
   - **调用堆栈**窗口（Ctrl+Alt+C）：完整调用链
   - **局部变量**窗口：当前函数的局部变量值
   - **监视**窗口：输入表达式查看变量值
   - **反汇编**窗口：崩溃点的汇编代码
   - **线程**窗口：所有线程及其调用栈

### 4.5 在另一台计算机上调试

如果要在非开发机上调试：

1. 复制以下文件到目标机器：
   - `.dmp` 文件
   - 对应的 `.pdb` 文件（exe 和所有 dll 的 pdb）
   - 源码（可选，用于查看源码行；没有源码也能看到函数名和行号）
2. 在目标机器上用 VS 打开 .dmp
3. 设置符号路径指向 pdb 所在目录
4. 开始调试

---

## 5. 使用 WinDbg 调试 Dump

### 5.1 安装 WinDbg

WinDbg 是 Windows SDK 的一部分，安装方式：
- Visual Studio Installer → 修改 → Windows SDK → Debugging Tools for Windows
- 或独立下载：https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/

### 5.2 打开 Dump 文件

```cmd
windbg -z "C:\Users\<用户名>\AppData\Local\etest_demo\crash\etest_crash_20260509_143052.dmp"
```

或启动 WinDbg 后：File → Open Crash Dump → 选择 .dmp 文件

### 5.3 设置符号路径

在 WinDbg 命令行中输入：

```
.sympath+ D:\trae_workspace\etest-demo\build\ninja-relwithdebinfo\bin
```

添加 Microsoft 公共符号（用于解析系统 DLL 的符号）：

```
.sympath+ srv*C:\Symbols*https://msdl.microsoft.com/download/symbols
```

重新加载符号：

```
.reload
```

### 5.4 设置源码路径

```
.srcpath+ D:\trae_workspace\etest-demo\src
```

### 5.5 常用调试命令

#### 崩溃分析（最重要的一条命令）

```
!analyze -v
```

输出内容包括：
- 异常代码和类型
- 故障模块和函数
- 完整调用栈
- 可能的崩溃原因

#### 查看调用栈

```
k          # 当前线程调用栈
kv         # 调用栈 + 帧参数
~*k        # 所有线程调用栈
```

#### 查看线程

```
~          # 列出所有线程
~0s        # 切换到 0 号线程
```

#### 查看局部变量

```
dv         # 显示当前帧的局部变量
dt 变量名   # 显示变量类型和值
```

#### 查看内存

```
db 地址     # 按字节查看
dd 地址     # 按 DWORD 查看
dq 地址     # 按 QWORD 查看
da 地址     # 按 ANSI 字符串查看
du 地址     # 按 Unicode 字符串查看
```

#### 查看寄存器

```
r          # 显示所有寄存器
```

#### 查看模块列表

```
lm         # 列出所有加载模块
lmv m etest_demo   # 查看 etest_demo 模块详细信息
```

### 5.6 典型调试流程

```
1. .sympath+ <pdb路径>           # 设置符号
2. .srcpath+ <源码路径>          # 设置源码
3. .reload                       # 重新加载符号
4. !analyze -v                   # 崩溃分析
5. k                             # 查看调用栈
6. frame /r <帧号>               # 切换到崩溃帧查看寄存器
7. dv                            # 查看局部变量
```

---

## 6. 常见问题

### Q: 打开 dump 后调用栈只显示地址，没有函数名

**原因**：PDB 文件未找到或不匹配。

解决：
1. 确认 PDB 文件与 exe 是同一次构建的产物
2. 确认符号路径设置正确
3. 在 WinDbg 中用 `!sym noisy` 打开详细日志，再 `.reload` 查看加载失败原因
4. 用 `chkmatch -m <exe> <pdb>` 工具验证 PDB 与 exe 是否匹配

### Q: 调用栈有函数名但没有源码行号

**原因**：源码路径未设置，或源码文件与构建时不一致。

解决：
1. 设置源码路径：`.srcpath+ <路径>`
2. 如果源码有改动，VS 中取消"要求源文件与原始版本完全匹配"

### Q: dump 文件太大

当前配置使用的是中等大小的 dump 类型。如果需要更小的 dump，可修改 `WindowsCrashHandler.cpp` 中的 `dumpType`：

```cpp
// 最小 dump（只包含线程栈，通常 < 1MB）
MINIDUMP_TYPE dumpType = MiniDumpNormal;

// 中等 dump（当前配置，10-50MB）
MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithDataSegs | MiniDumpWithHandleData |
    MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo);

// 完整 dump（包含全部进程内存，可能数百MB）
MINIDUMP_TYPE dumpType = MiniDumpWithFullMemory;
```

### Q: 如何在非开发机上调试

需要准备一个"调试包"：
1. `.dmp` 文件
2. 所有 `.pdb` 文件（与 exe 同目录的 pdb）
3. （可选）源码目录

在目标机器上安装 WinDbg 或 VS，设置符号路径和源码路径即可。

### Q: PDB 文件可以和不同次构建的 exe 混用吗

**不可以。** 每次构建生成的 PDB 包含唯一的签名/GUID，必须与对应 exe 匹配。建议每次发布时将 exe 和 pdb 打包保存，并标注版本号/构建时间。
