# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述
这是一个基于Qt/C++的学习研究项目，目标是仿照凯云ETest测试系统实现其核心功能。项目采用CMake构建系统，支持Windows平台。

## 常用命令
### 构建项目
```bash
scripts/build_ninja.bat
```
- 自动配置VS2019 x64编译环境
- 使用Ninja生成器进行Debug构建
- 构建输出目录：`build/ninja-debug/`
- 可执行文件输出到：`build/ninja-debug/bin/`

### 运行主程序
```bash
run_app.bat
```
- 自动配置Qt环境变量
- 启动编译好的etest_demo.exe

### 代码格式化
```bash
format_code.bat
```
- 使用clang-format格式化代码，遵循Google C++风格规范

## 代码架构
项目采用分层架构设计：

### Core层（核心功能层）
位于`src/core/`目录，提供基础服务和核心功能：
- **common/**：通用异常类定义（ByteException、FileException、StringException、TimeException）
- **config/**：配置管理系统（ConfigManager）
- **crashhandler/**：程序崩溃处理机制，支持Windows平台崩溃捕获
- **logger/**：基于spdlog的日志系统
- **plugin/**：插件框架，支持动态加载插件（IPlugin接口、PluginManager）
- **project/**：项目管理系统（ProjectInfo、ProjectManager）
- **utils/**：通用工具函数库（ByteUtil、FileUtil、StringUtil、TimeUtil）

### App层（应用界面层）
位于`src/app/`目录，实现Qt UI界面：
- **MainWindow**：主窗口，基于Qt-Advanced-Docking-System实现停靠式界面布局
- **ActivityBarWidget**：左侧活动栏，功能模块切换
- **SidebarWidget**：侧边栏面板容器
- **FileExplorerWidget**：文件浏览器面板
- **EditorManager**：编辑器管理
- **EditorWidget**：代码编辑器（基于QScintilla）
- **OutputPanel**：输出信息面板
- **ProblemsPanel**：问题列表面板
- **TerminalPanel**：终端面板
- **dialogs/**：各类对话框（如NewProjectDialog新建项目对话框）

## 项目规则
必须严格遵守以下规则：
1. 代码修改必须经过用户明确授权，不得擅自改动未确认的内容
2. C++代码遵循Google C++ Style Guide，Qt界面代码需将UI初始化和信号槽连接分离到`initUi()`和`initSignals()`函数
3. 代码设计需遵循SOLID原则
4. 禁止执行会修改git仓库的命令，仅允许使用查看类git命令（git status、git log等）
5. Markdown文档遵循Google文档风格指南
6. 文档统一存放在`docs/`目录下，按规划、研究、开发、参考分类组织
7. 回答问题的语气禁止阿谀奉承，不用故意谄媚，只说问题的解决办法和思路
8. 不要每次改动之后都给我说百分百没问题！

## 第三方依赖
项目集成了以下第三方库，均已在CMake中配置为静态编译：
- Qt 5.12.12（Core、Gui、Widgets、PrintSupport、Test、Xml、Svg），Qt使用官方编译的二进制发布包，默认是共享库
- Qt-Advanced-Docking-System 3.8.3（高级停靠系统）
- QXlsx 1.5.0（Excel读写）
- zlib 1.3.2（压缩库）
- libpng 1.6.43（PNG图像处理）
- spdlog 1.17.0（日志库）
- googletest 1.17.0（单元测试框架）
- lua 5.4.4（脚本语言支持）
- libharu 2.4.6（PDF生成）
- QScintilla 2.11.3（高级文本编辑器组件）

## 其它注意事项
 - Qt使用的SDK是 5.12.12 msvc2017_64，但是我并未使用MSVC2017的环境，实际测试下来 MSVC2019编译环境完全兼容 MSVC2017