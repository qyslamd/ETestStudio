# ETestStudio

基于 Qt/C++ 的学习研究项目，仿照凯云 ETest 测试系统实现其核心功能。

## 开发状态

目前由单人使用 AI Agent（Claude Code）辅助开发。团队成员后续可能引入 AI Agent 协作，请务必阅读 `CLAUDE.md` 了解项目约定和 AI 协作规则。

## 软件架构

分层架构设计：

- **src/core/** — 核心功能层：配置管理、日志系统、插件框架、项目管理、通用工具
- **src/app/** — 应用界面层：基于 Qt-Advanced-Docking-System 的主窗口、编辑器、面板等
- **src/protocal/** — 帧协议编辑器
- **src/topology/** — 拓扑编辑器

## 开发环境

- Windows 10/11 64位
- Visual Studio 2019 Community（需安装"使用 C++ 的桌面开发"工作负载）
- Qt 5.14.2 (msvc2017_64)
- CMake 3.15+
- Git

## 环境搭建（给新同学）

### 1. 安装 Visual Studio 2019

下载 [Visual Studio 2019 Community](https://visualstudio.microsoft.com/vs/older-downloads/)，安装时勾选：

- **使用 C++ 的桌面开发**（Desktop development with C++）
- 右侧可选组件中确保 **MSVC v142 - VS 2019 C++ x64/x86 生成工具** 已选中

安装路径保持默认，脚本会从 `D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\` 查找。如果装到了其他盘，需要修改 `scripts/build_ninja.bat` 中的路径。

### 2. 安装 Qt 5.14.2

1. 下载 [Qt 5.14.2](https://download.qt.io/archive/qt/5.14/5.14.2/) 的 `qt-opensource-windows-x86-5.14.2.exe`
2. 安装到默认路径 `D:\Qt\Qt5.14.2`
3. 组件选择时展开 **Qt → Qt 5.14.2**，勾选 **msvc2017_64**
4. 设置系统环境变量 `QT5_DEFAULT_SDK_PATH`：
   - 此电脑 → 属性 → 高级系统设置 → 环境变量
   - 新建系统变量
   - 变量名：`QT5_DEFAULT_SDK_PATH`
   - 变量值：`D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\`
   - 注意末尾的反斜杠 `\`
5. 重启电脑或重启资源管理器使环境变量生效

### 3. 安装 CMake

1. 下载 [CMake](https://cmake.org/download/) Windows x64 Installer
2. 安装时勾选 **Add CMake to the system PATH for all users**
3. 验证：打开 CMD，执行 `cmake --version` 看是否识别

### 4. 安装 Git

1. 下载 [Git for Windows](https://git-scm.com/download/win)
2. 一路默认安装即可

### 5. 克隆并构建

```bash
# 克隆仓库
git clone https://gitee.com/你的仓库地址/ETestStudio.git
cd ETestStudio

# 构建项目（首次执行会自动下载第三方依赖，需要联网）
scripts/build_ninja.bat

# 运行
scripts/run_app.bat
```

首次构建较慢，因为 CMake 会自动下载并编译第三方库（zlib、spdlog、QScintilla 等）。后续构建只会编译你改动的代码。

### 确认环境变量

以下变量需要正确设置：

| 变量名 | 示例值 | 说明 |
|---|---|---|
| `QT5_DEFAULT_SDK_PATH` | `D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\` | Qt SDK 路径，末尾带 `\` |

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
2. 构建套件选择 **msvc2017_64**（需要 Qt 5.14.2 安装时已勾选 msvc2017_64 组件）
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

```bash
# 构建项目
scripts/build_ninja.bat

# 运行主程序
scripts/run_app.bat
```

可执行文件输出到 `build/ninja-debug/bin/`。

## 协作约定

1. **AI Agent**：开发者使用 AI Agent 时，Agent 会自动读取 `CLAUDE.md` 遵守项目规则。如果团队新增成员使用 AI Agent，请确保其加载了项目根目录的 `CLAUDE.md`
2. **合并策略**：`git pull` 默认使用 rebase（已通过 `git config --global pull.rebase true` 启用），避免多余的 merge commit，保持历史线性
3. **构建验证**：提交前确保 `scripts/build_ninja.bat` 编译通过

## 第三方依赖

见 `CLAUDE.md` 中完整列表。
