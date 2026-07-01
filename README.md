# ETestStudio

基于 Qt/C++ 的学习研究项目，仿照凯云 ETest 测试系统实现其核心功能。

## 开发状态

目前由单人使用 AI Agent（Claude Code）辅助开发。团队成员后续可能引入 AI Agent 协作，请务必阅读 `CLAUDE.md` 了解项目约定和 AI 协作规则。

## 软件架构

分层、模块化架构设计，`src/` 下每个子目录对应一个独立的 CMake 构建目标：

| 模块 | 目录 | 说明 |
|------|------|------|
| **etest_core** | `src/core/` | 核心功能层：配置管理、日志系统（spdlog）、插件框架、项目管理、崩溃处理、通用工具 |
| **etest_api** | `src/api/` | 纯头文件接口库（IEditor 等），无链接依赖 |
| **etest_topology** | `src/topology/` | 拓扑编辑器：场景/视图/项/撤销/序列化/导出 |
| **etest_protocol** | `src/protocol/` | ICD 协议编辑器：节点树/位图视图/属性面板 |
| **etest_ui** | `src/libui/` | 跨模块共享 UI 组件库（如 DockTitleBar），被 topology、protocol 共用 |
| **icd_utility** | `src/icd_utility/` | ICD 数据格式工具库（纯 C++17，无 Qt 依赖） |
| **etest** | `src/app/` | 主程序：停靠界面布局、编辑器管理、欢迎页、终端、输出面板、屏保组件等 |

### 模组依赖关系

```
etest (主程序)
├── etest_topology ─┬── etest_ui ─── etest_core
├── etest_protocol ─┤              └── Qt5::Widgets
├── icd_utility     └── etest_api (header-only)
└── Qt5 / 第三方库 (QScintilla, SARibbon, QADS, QXlsx, libharu...)
```

## 开发环境

- Windows 10/11 64位
- Visual Studio 2019 Community（需安装"使用 C++ 的桌面开发"工作负载）
- Qt 5.15.2 (msvc2019_64)
- CMake 3.19+
- Git

## 环境搭建（给新同学）

### 1. 安装 Visual Studio 2019

下载 [Visual Studio 2019 Community](https://visualstudio.microsoft.com/vs/older-downloads/)，安装时勾选：

- **使用 C++ 的桌面开发**（Desktop development with C++）
- 右侧可选组件中确保 **MSVC v142 - VS 2019 C++ x64/x86 生成工具** 已选中

### 2. 安装 Qt 5.15.2

1. 下载 [Qt 5.15.2](https://download.qt.io/archive/qt/5.15/5.15.2/) 的 `qt-opensource-windows-x86-5.15.2.exe`
2. 安装时组件选择 **msvc2019_64**
3. 设置系统环境变量 `ETest_Qt5_Path`：
   - 此电脑 → 属性 → 高级系统设置 → 环境变量
   - 新建系统变量
   - 变量名：`ETest_Qt5_Path`
   - 变量值：`D:\Qt22\5.15.2`
   - 不需要在末尾添加 kit 目录，调试配置会自行拼接 `msvc2019_64`
4. 重启电脑或重启资源管理器使环境变量生效

### 3. 安装 CMake

1. 下载 [CMake](https://cmake.org/download/) Windows x64 Installer
2. 安装时勾选 **Add CMake to the system PATH for all users**
3. 设置系统环境变量 `ETest_CMake_Path` 指向 CMake bin 目录
4. 验证：打开 CMD，执行 `cmake --version` 看是否识别

### 4. 安装 Git

1. 下载 [Git for Windows](https://git-scm.com/download/win)
2. 一路默认安装即可

### 5. 克隆并构建

```bash
# 克隆仓库
git clone https://gitee.com/slamdd/etest-demo.git
cd etest-demo

# 构建项目（首次执行会自动解压第三方依赖）
scripts/build_ninja.bat

# 运行
scripts/run_app.bat
```

首次构建较慢，因为 CMake 会自动解压并编译第三方库（zlib、spdlog、QScintilla、SARibbon 等）。后续构建只会编译你改动的代码。

### 确认环境变量

以下变量需要正确设置：

| 变量名 | 示例值 | 说明 |
|---|---|---|
| `VS2019_CMD_DIR` | `D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build` | VS2019 环境初始化脚本目录，需包含 `vcvars64.bat` 和 `vcvars32.bat` |
| `ETest_Qt5_Path` | `D:\Qt22\5.15.2` | Qt 5.15.2 安装路径 |
| `ETest_CMake_Path` | `C:\Program Files\CMake\bin` | CMake 可执行文件目录 |

## 使用 IDE 开发

### VSCode（推荐）

需要安装扩展：

- **CMake Tools**（ms-vscode.cmake-tools）— CMake 集成
- **C/C++**（ms-vscode.cpptools）— 代码补全、调试

步骤：

1. `文件 → 打开文件夹`，选择项目根目录
2. 按 `Ctrl+Shift+P`，运行 `CMake: Select Configure Preset`，选择 **ninja-debug**
3. 按 `Ctrl+Shift+P`，运行 `CMake: Select Build Preset`，选择 **ninja-debug-build**
4. 底部状态栏点击 `[Build]` 或按 `F7` 编译
5. 可执行文件输出到 `build/ninja-debug/bin/ETestStudio.exe`

**注意**：需要在 x64 Native Tools Command Prompt for VS2019 中启动 VSCode，或者手动执行 `scripts/build_ninja.bat` 完成首次配置。CMake Tools 会自动识别 `ninja-debug` 预设。

### QtCreator

1. `文件 → 打开文件或项目`，选择项目根目录的 `CMakeLists.txt`
2. 构建套件选择 **msvc2019_64**
3. CMake 预设选择 **ninja-debug**
4. 按 `Ctrl+B` 编译，按 `Ctrl+R` 运行

如果 QtCreator 提示找不到 CMake，需在 `工具 → 选项 → Kits → CMake` 中设置 CMake 可执行文件路径。

### Visual Studio 2019

VS2019 对 `CMakePresets.json` 的支持有限（16.10 后才引入，且 `$env{}` 变量引用可能无法解析），**不建议**直接将项目文件夹作为 CMake 项目打开。推荐以下两种方式：

**方式一：用脚本构建，用 VS 只调试**

```bash
# 先构建
scripts/build_ninja.bat

# 然后从 VS 打开生成的 .sln，或直接附加进程调试
```

**方式二：用 VS 的 MSBuild 构建**

需要先生成 Visual Studio 解决方案：

```bash
cmake -S . --preset windows-debug
```

然后在 `build/windows-debug/` 下打开生成的 `.sln` 文件，`Ctrl+Shift+B` 编译。注意这种方式不使用 Ninja，编译速度会比 `ninja-debug` 慢一些。

## 快速开始

### 构建

`build_ninja.bat` 支持两种参数模式，脚本会自动识别：第一个参数以 `-` 开头走新模式，否则走旧模式。

#### 旧模式（位置参数）

```bash
build_ninja.bat [<type>] [<action>]
```

| 命令 | 构建类型 | 目标 | 后续动作 |
|------|---------|------|---------|
| `build_ninja.bat` | debug | 全量 | — |
| `build_ninja.bat debug` | debug | 全量 | — |
| `build_ninja.bat relwithdebinfo` | relwithdebinfo | 全量 | — |
| `build_ninja.bat release` | release | 全量 | — |
| `build_ninja.bat debug deploy` | debug | 全量 | windeployqt |
| `build_ninja.bat debug package` | debug | 全量 | windeployqt + ISCC |
| `build_ninja.bat release package` | release | 全量 | windeployqt + ISCC |

#### 新模式（显式参数）

```bash
build_ninja.bat -t <type> [-m <target>] [-d|-p]
```

| 参数 | 说明 |
|------|------|
| `-t, --type <type>` | 构建类型：debug / relwithdebinfo / release（默认 debug） |
| `-m, --target <target>` | 构建目标（如 ETestStudio），省略则全量构建 |
| `-d, --deploy` | 编译后执行 windeployqt |
| `-p, --package` | 编译后执行 windeployqt + ISCC 打包 |
| `-h, --help` | 显示帮助 |

`--type` 和 `--target` 支持两种写法：`-t debug` 或 `--type=debug`，`-m ETestStudio` 或 `--target=ETestStudio`。

常用组合示例：

```bash
# 全量构建（debug）
scripts/build_ninja.bat -t debug

# 仅构建主程序
scripts/build_ninja.bat -t debug -m ETestStudio

# 构建并部署（windeployqt）
scripts/build_ninja.bat -t debug -m ETestStudio -d

# 构建 + 部署 + 打包安装程序
scripts/build_ninja.bat -t relwithdebinfo -m ETestStudio -p

# 等效的长参数写法
scripts/build_ninja.bat --type=debug --target=ETestStudio --package
```

### 运行

```bash
scripts/run_app.bat
```

可执行文件输出到 `build/ninja-debug/bin/`。

### 打包

使用 `-p` 参数自动完成编译 + windeployqt 部署 + Inno Setup 打包：

```bash
scripts/build_ninja.bat -t relwithdebinfo -m ETestStudio -p
```

安装程序输出到 `dist/ETestStudio-setup-x64-<version>.exe`。

## 协作约定

1. **AI Agent**：开发者使用 AI Agent 时，Agent 会自动读取 `CLAUDE.md` 遵守项目规则。如果团队新增成员使用 AI Agent，请确保其加载了项目根目录的 `CLAUDE.md`
2. **合并策略**：`git pull` 默认使用 rebase（已通过 `git config --global pull.rebase true` 启用），避免多余的 merge commit，保持历史线性
3. **构建验证**：提交前确保 `scripts/build_ninja.bat` 编译通过

## 第三方依赖

项目集成了以下第三方库，均已在 CMake 中配置为静态编译：

| 库 | 版本 | 说明 |
|---|---|---|
| Qt | 5.15.2 | Core、Gui、Widgets、PrintSupport、Test、Xml、Svg、Sql（官方共享库） |
| Qt-Advanced-Docking-System | 3.8.3 | 高级停靠系统 |
| SARibbon | 2.5.7 | Ribbon 风格界面 |
| QWindowKit | 1.5.0 | 无边框窗口解决方案 |
| QXlsx | 1.5.0 | Excel 读写 |
| QScintilla | 2.11.3 | 高级文本编辑器组件 |
| spdlog | 1.17.0 | 日志库 |
| googletest | 1.17.0 | 单元测试框架 |
| zlib | 1.3.2 | 压缩库 |
| libpng | 1.6.43 | PNG 图像处理 |
| libharu | 2.4.6 | PDF 生成 |
| lua | 5.4.4 | 脚本语言支持 |
| sol2 | 3.3.0 | Lua C++ 绑定库 |
| Inno Setup | 6.x | 安装程序打包 |

> Qt 使用官方编译的二进制发布包，默认是共享库。MSVC2019 编译环境完全兼容 msvc2019_64 编译的 Qt 库。
