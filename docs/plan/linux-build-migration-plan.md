# Linux (WSL Ubuntu 20.04) 构建迁移计划

## 目标

在 WSL2 Ubuntu 20.04 环境下，用 VSCode + 自编写脚本方式编译通过本项目（主程序 `ETestStudio`），保持 Windows 侧构建不受影响。本期范围仅打通"配置 + 编译"，不实现 Linux 专属功能（终端、崩溃转储）。

## 现状

### 宿主环境

| 项 | 版本 |
|---|---|
| 内核 | Linux 6.6.87.2-microsoft-standard-WSL2 (x86_64) |
| GCC | 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.2) |
| Ninja | 1.10.0 |
| 系统 CMake | 3.16.3（apt，保留不动） |
| 预编译 CMake | 3.31.12（`/home/zhou/env_software/cmake-3.31.12-linux-x86_64/`，加 PATH 优先） |
| Qt5 | 5.12.8（系统包，`qtbase5-dev` 已装） |
| tar | GNU tar 1.30 |
| 7z | 未安装（`.zip` 走 CMake 内置 `file(ARCHIVE_EXTRACT)`，不影响） |

### 源码平台隔离评估

源码层面的 Windows API 调用全部正确包在 `#ifdef Q_OS_WIN` / `#ifdef _WIN32` 内，已有平台工厂拆分：

- `src/core/CMakeLists.txt:69-77`：`if(WIN32)` 编译 `WindowsCrashHandler`/`ConPtyProcess`，`else()` 编译 `PosixPtyProcess`
- `src/core/crashhandler/WindowsCrashHandler.h:2`、`src/core/terminal/ConPtyProcess.h:6`：整文件 `Q_OS_WIN` 守卫
- `src/core/common/SingleInstance.cpp:1`：`#ifdef _WIN32` 分支，Linux 走 `emit showApplication()`
- `src/icd_utility/include/icd/export.hpp:8-24`：已有 GCC `__attribute__((visibility))` 分支

**结论：源码无硬性编译阻断。** 阻断点集中在 CMake 配置层与环境层。

## 阻断点清单

| # | 类别 | 问题 | 位置 |
|---|---|---|---|
| 1 | 环境 | `libqt5svg5-dev` 未装，`Qt5Svg` 的 pkg-config / cmake config 缺失 | apt |
| 2 | 环境 | 系统 CMake 3.16.3 满足 `cmake_minimum_required(3.15)`，但 `CMakePresets.json` 是 version 8（需 CMake 3.25+），`cmake --preset` 不可用 | `CMakePresets.json:1` |
| 3 | 配置 | `local.cmake` 不存在；顶层 `CMakeLists.txt:31-39` 在非 WIN32 下直接 `FATAL_ERROR`；模板 Linux 分支硬编码错误路径 `/opt/qt5.12.12/5.12.12/gcc_64` | `CMakeLists.txt:27-39`、`cmake/local.cmake.in:12` |
| 4 | 配置 | 无 Linux preset；`scripts/build_ninja.sh` 仅 `echo hello world` | `scripts/build_ninja.sh:1-3` |
| 5 | 功能 | `PosixPtyProcess` 是空壳（`start()` 直接 `return false`），内置终端不可用 | `src/core/terminal/PosixPtyProcess.cpp:15-21` |
| 6 | 功能 | 无 Linux crash handler，`CrashHandler::create()` 返回 `nullptr` | `src/core/crashhandler/CrashHandler.cpp:19-21` |
| 7 | 功能 Bug | 硬编码 `\\` 路径后缀判断，Linux 上静默失效 | `src/app/MainWindow.cpp:576,579,582`、`src/core/.../ProjectStructureWidget.cpp:1362` |

本期处理 #1-#4（编译阻断）：#1 #2 在阶段一解决，#3 #4 在阶段二解决；#5-#7 留待后续。

## 改造方案

按阶段推进，每阶段独立可验证。

### 阶段一：环境准备

#### 1.1 安装 Qt5Svg 开发包

```bash
sudo apt install libqt5svg5-dev
```

其余 Qt5 模块（Core/Gui/Widgets/Network/Concurrent/Sql/PrintSupport/Test/Xml）的 dev 头文件已由 `qtbase5-dev` 提供，`Qt5Test` 的 cmake config 亦就绪。仅需补 Svg。

配置阶段实测还需两个包：

```bash
sudo apt install qttools5-dev qtbase5-private-dev
```

- `qttools5-dev`：提供 `Qt5LinguistTools`（`src/app/CMakeLists.txt:214` 用到，缺则 Warning 且后续 `.ts` 翻译编译失败）
- `qtbase5-private-dev`：提供 `Qt5::GuiPrivate` / `Qt5::CorePrivate` 目标（QXlsx 用了 Qt Gui 私有 API，`3rdparty/QXlsx-1.5.0/QXlsx/CMakeLists.txt:169` 硬编码依赖，缺则生成阶段 Error）

#### 1.2 安装 CMake 3.31.12（预编译二进制）

系统 CMake 3.16.3 满足 `cmake_minimum_required(3.15)`，但 `CMakePresets.json` 是 version 8（需 CMake 3.25+），必须升级。采用官方预编译二进制，不卸载系统 cmake（避免连带风险）。

二进制已下载至 `/home/zhou/env_software/cmake-3.31.12-linux-x86_64/`，只需加入 PATH 使其优先于系统 `/usr/bin/cmake`(3.16.3)。

写入 `~/.bashrc`（见本节末尾代码块）。

验证：

```bash
$ which cmake
/home/zhou/env_software/cmake-3.31.12-linux-x86_64/bin/cmake
$ cmake --version
cmake version 3.31.12
```

满足 `CMakePresets.json` version 8 要求（3.25+）。回退方式：删除 `~/.bashrc` 中那行 export 即可恢复系统 cmake。

**`.bashrc` 修改代码**：

在 `~/.bashrc` 末尾追加：

```bash
# CMake 3.31.12（预编译二进制，优先于系统 3.16.3）
export PATH=/home/zhou/env_software/cmake-3.31.12-linux-x86_64/bin:$PATH
```

修改后执行 `source ~/.bashrc` 生效。

### 阶段二：CMake 配置改造

#### 2.1 创建 `local.cmake`（项目根目录）

`local.cmake` 是本地文件（已在 `.gitignore`），指向系统 Qt5：

```cmake
# Linux Qt5 配置（系统包 5.12.8）
set(Qt5_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5")
set(QT_DIR  "/usr/lib/x86_64-linux-gnu/cmake/Qt5")
```

这步绕开 `CMakeLists.txt:27-39` 的 `FATAL_ERROR`。

#### 2.2 修改 `CMakePresets.json`：新增 Linux preset 体系

现有 `ninja-debug` 等 preset 本身平台无关（仅指定 Ninja 生成器），配好 `local.cmake` 后理论上可直接 `--preset ninja-debug`。但为避免和 Windows 构建产物混在同一 `build/ninja-debug` 目录，新增显式 Linux preset。

新增 configurePresets（继承现有 `ninja-<type>`）：

| 新 preset | 继承 | binaryDir |
|---|---|---|
| `ninja-debug-linux` | `ninja-debug` | `build/ninja-debug-linux` |
| `ninja-release-linux` | `ninja-release` | `build/ninja-release-linux` |
| `ninja-relwithdebinfo-linux` | `ninja-relwithdebinfo` | `build/ninja-relwithdebinfo-linux` |

同步新增对应 buildPresets（`targets: "all"`）与 testPresets。

#### 2.3 修改 `cmake/local.cmake.in`：修正 Linux 分支路径（可选）

当前模板 `cmake/local.cmake.in:12` 硬编码 `/opt/qt5.12.12/5.12.12/gcc_64`，与实际系统 Qt5.12.8 不符。可改为更通用的提示，或保留不动（因 Linux 下不再走 `configure_file` 自动生成流程，`local.cmake` 手工创建）。本期保留不动，避免影响 Windows 侧。

#### 2.4 实写 `scripts/build_ninja.sh`

当前 `scripts/build_ninja.sh:1-3` 仅 `echo hello world`。镜像 `build_ninja.bat` 的参数体系（`-t/-m/-c/-h`），但去掉 Windows 专属逻辑：

| 参数 | 行为 |
|---|---|
| `-t <type>` | debug / relwithdebinfo / release，映射到 `ninja-<type>-linux` preset |
| `-m <target>` | 指定构建目标，省略则全量 |
| `-c` / `--configure` | 仅 configure，不 build |
| `-h` / `--help` | 帮助 |

去除项（Windows 专属）：
- 无 vcvars / VS2019 环境初始化
- 无 `-a` 架构参数（Linux 固定 x64）
- 无 `-d` / `-p`（`windeployqt` / ISCC 打包），若传入则提示"Linux 不支持"并退出

脚本骨架（伪代码）：

```bash
#!/bin/bash
set -e
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

# 参数解析（-t/-m/-c/-h）
# ...
PRESET="ninja-${BUILD_TYPE}-linux"
BUILD_DIR="build/${PRESET}"

cmake -S . --preset "$PRESET"
[ -n "$CONFIGURE_ONLY" ] && exit 0

if [ -n "$TARGET" ]; then
    cmake --build "$BUILD_DIR" --target "$TARGET"
else
    cmake --build "$BUILD_DIR"
fi
```

### 阶段三：构建排错

首次配置 + 构建：

```bash
./scripts/build_ninja.sh -t debug                # 全量
./scripts/build_ninja.sh -t debug -m ETestStudio # 仅主程序
```

预计出现的 GCC vs MSVC 差异问题（逐个修，不预判）：

- narrowing conversion（MSVC 宽松，GCC 严格）
- missing return（GCC 警告升错误）
- template 推导差异
- 第三方库 SARibbon / QWindowKit 的 X11 无边框窗口适配
- `Q_OS_WIN` 守卫内可能有遗漏的符号泄漏

排错原则：仅修阻断编译的最小改动，不做功能重构。

### 阶段三：构建排错 -- 已完成

全量构建通过（`cmake --build build/ninja-debug-linux`，EXIT_CODE=0），产物齐全：
- 主程序 ETestStudio（134M debug）
- 独立工具 5 个（topology-editor / protocol-editor / test-program-editor / test-executor / test-executor-cli）
- 示例 4 个 + lua 解释器
- 插件 6 个 .so（hello + 5 mock）
- 测试 57 个

实际修复的问题（均为 MSVC 宽松 / GCC 严格 或 Qt 版本差异，非功能 bug）：

| # | 文件 | 问题 | 修复 |
|---|---|---|---|
| 1 | `src/core/utils/TimeUtil.cpp:15` | `Qt::ISODate`(枚举) 赋给 `QString` | 改为字面量 `"yyyy-MM-ddTHH:mm:ss"` |
| 2 | `src/core/common/SingleInstance.cpp:128` | `QLocalSocket::errorOccurred` 是 Qt 5.15+ 信号 | `#if QT_VERSION` 降级到 `error` |
| 3 | `src/protocol/IcdFramePreview.cpp:65`、`src/engine/SignalResolver.cpp:148` | `Qt::SkipEmptyParts` 是 Qt 5.14+ | `#if QT_VERSION` 降级到 `QString::SkipEmptyParts` |
| 4 | `src/app/widgets/BottomContainerWidget.cpp:73,81` | `QTabBar::setTabVisible/isTabVisible` 是 Qt 5.15+ | `#if QT_VERSION` 降级到 `setTabEnabled/isTabEnabled` |
| 5 | `src/app/grid/layout_calculator_v1.h`、`layout_calculator_v3.h` | 嵌套 `QVector<QVector<T>>` 作成员，GCC 需完整类型 | 补 `#include <QVector>` |
| 6 | `src/app/visualizers/StateLEDWidget.cpp` | `QString` 转 `QVariant` 需完整类型 | 补 `#include <QVariant>` |
| 7 | `src/app/widgets/LoadingOverlay.cpp`、`src/app/VisualizationArea.cpp` | `std::sin/ceil/sqrt` 未声明 | 补 `#include <cmath>` |
| 8 | `src/tools/test-executor-cli/main.cpp` | `Qt::endl` 是 Qt 5.14+ | 定义 `QT_ENDL` 宏版本兼容 |
| 9 | `cmake/lua/CMakeLists.txt.in:95` | `GLOB_RECURSE` 误收 `wmain.c`(Windows 专属) 进 liblua | 显式 `list(REMOVE_ITEM ... wmain.c)` |
| 10 | `src/app/CMakeLists.txt` 等多处 | `qtadvanceddocking` 静态库依赖 xcb，链接行缺失 | 顶层给 `qtadvanceddocking` 注入 `INTERFACE_LINK_LIBRARIES xcb` |
| 11 | `CMakeLists.txt` | mock 插件(SHARED)链接静态库缺 `-fPIC` | 全局 `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` |
| 12 | `examples/lua-debugger-demo/CMakeLists.txt` | `liblua` 在 Linux 需 pthread | 加 `Threads::Threads`，顶层 `find_package(Threads REQUIRED)` |

### 阶段四：VSCode 适配 -- 待开始

#### 4.1 修改 `.vscode/settings.json`

当前 `settings.json:3` 硬编码 `clang-format.executable` 为 Windows 路径 `D:\Qt\...`，Linux 下失效。改为：

```json
"clang-format.executable": "clang-format"
```

直接用系统 PATH 中的 `clang-format`（Ubuntu 20.04 自带 clang-format-10）。

终端 profile 增加 Linux 配置（当前 `settings.json:7-16` 仅 Windows）。

#### 4.2 修改 `.vscode/tasks.json`

当前 `tasks.json:8,15` 的 preset 是 `user-debug`（不存在）。改为 `ninja-debug-linux`：

```json
{
    "type": "cmake",
    "label": "CMake: 配置",
    "command": "configure",
    "preset": "ninja-debug-linux"
},
{
    "type": "cmake",
    "label": "CMake: 构建",
    "command": "build",
    "preset": "ninja-debug-linux"
}
```

## 暂不处理项

| 项 | 位置 | 影响 | 后续方案 |
|---|---|---|---|
| `PosixPtyProcess` 空壳 | `src/core/terminal/PosixPtyProcess.cpp:15` | 内置终端面板不可用 | 用 `openpty()` + `fork()` + `exec()` 实现 |
| 无 Linux crash handler | `src/core/crashhandler/CrashHandler.cpp:19` | 无崩溃转储 | 用 `sigaction` + `backtrace` 实现轻量版 |
| 硬编码 `\\` 路径判断 | `MainWindow.cpp:576,579,582` 等 | 项目结构判断失效 | 改为 `endsWith("/protocol")` 或用 `QDir::separator()` |

## 文件修改清单

| 文件 | 操作 | 说明 |
|---|---|---|
| `local.cmake` | 新建 | 本地 Qt5 路径配置（不入库） |
| `CMakePresets.json` | 修改 | 新增 3 个 linux configurePreset + 3 个 buildPreset + 3 个 testPreset |
| `scripts/build_ninja.sh` | 修改 | 实写 Linux 构建脚本 |
| `CMakeLists.txt` | 修改 | `find_package(Qt5 ...)` 组件名 `network` 改 `Network`；全局开启 `CMAKE_POSITION_INDEPENDENT_CODE`；`find_package(Threads REQUIRED)`；给 `qtadvanceddocking` 注入 xcb INTERFACE 依赖 |
| `cmake/lua/CMakeLists.txt.in` | 修改 | `GLOB_RECURSE` 排除 `wmain.c`（Windows 专属，避免误入 liblua） |
| `src/core/utils/TimeUtil.cpp` | 修改 | `Qt::ISODate` 赋 `QString` 改字面量 |
| `src/core/common/SingleInstance.cpp` | 修改 | `errorOccurred` 信号 Qt 5.15+ 版本兼容 |
| `src/protocol/IcdFramePreview.cpp` | 修改 | `Qt::SkipEmptyParts` Qt 5.14+ 版本兼容 |
| `src/engine/SignalResolver.cpp` | 修改 | `Qt::SkipEmptyParts` Qt 5.14+ 版本兼容 |
| `src/app/widgets/BottomContainerWidget.cpp` | 修改 | `setTabVisible/isTabVisible` Qt 5.15+ 版本兼容 |
| `src/app/grid/layout_calculator_v1.h` | 修改 | 补 `#include <QVector>` |
| `src/app/grid/layout_calculator_v3.h` | 修改 | 补 `#include <QVector>` |
| `src/app/visualizers/StateLEDWidget.cpp` | 修改 | 补 `#include <QVariant>` |
| `src/app/widgets/LoadingOverlay.cpp` | 修改 | 补 `#include <cmath>` |
| `src/app/VisualizationArea.cpp` | 修改 | 补 `#include <cmath>` |
| `src/tools/test-executor-cli/main.cpp` | 修改 | `Qt::endl` Qt 5.14+ 版本兼容宏 |
| `examples/lua-debugger-demo/CMakeLists.txt` | 修改 | 链接 `Threads::Threads` |
| `.vscode/settings.json` | 修改 | clang-format 路径 + Linux 终端 profile |
| `.vscode/tasks.json` | 修改 | preset 换为 `ninja-debug-linux` |

所有 `src/` 改动均为 Qt 版本兼容（5.12 vs 5.15）或 GCC 严格性（缺 include）的最小修复，不改业务逻辑，Windows 侧不受影响。

## 执行进度

### 阶段一：环境准备 -- 已完成

- CMake 3.31.12 预编译二进制就位，`.bashrc:122` 加 PATH
- `libqt5svg5-dev` / `qttools5-dev` / `qtbase5-private-dev` 已装

### 阶段二：CMake 配置改造 -- 已完成

- `local.cmake` 已建（指向系统 Qt5 5.12.8）
- `CMakePresets.json` 已加 9 个 linux preset（3 configure + 3 build + 3 test）
- `scripts/build_ninja.sh` 已实写（`-t/-m/-c/-h` 参数体系）
- `CMakeLists.txt:71` 组件名 `network` -> `Network`（修复大小写）
- 验证：`cmake --preset ninja-debug-linux` 配置通过（8.7s），build files 已生成至 `build/ninja-debug-linux/`

### 阶段三：构建排错 -- 待开始

## 验证步骤

1. `cmake --version` 输出 3.28.3
2. `pkg-config --modversion Qt5Svg` 输出 5.12.8
3. `./scripts/build_ninja.sh -t debug -c` 配置成功，无 `FATAL_ERROR`
4. `./scripts/build_ninja.sh -t debug -m ETestStudio` 编译通过，产物在 `build/ninja-debug-linux/bin/`
5. VSCode 中执行"CMake: 配置" / "CMake: 构建"任务成功

## 风险与回退

- **风险**：`CMakePresets.json` 改动可能影响 Windows 侧。缓解：仅新增 preset，不改动现有任何 preset。
- **回退**：`local.cmake` 删除即可；`CMakePresets.json` / `build_ninja.sh` / `.vscode/*` 可 `git checkout` 还原。
