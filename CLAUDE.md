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

### 运行程序
#### 主程序
```batch
rem 自动配置Qt环境变量
rem 启动编译好的etest_demo.exe
scripts/run_app.bat
```
#### 其它程序
```batch
rem 自动配置Qt环境变量
rem <xxxx> 是cmake自动通过config_file生成的
rem 启动编译好的可执行程序
scripts/run_<xxxx>.bat
```

## 代码风格
- 所有生成的代码需要遵循clang-format的Google C++风格规范

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
4. 所有会修改git仓库的命令必须经过我的确认，使用查看类git命令（git status、git log等）的没有限制
5. Markdown文档遵循Google文档风格指南
6. 文档统一存放在`docs/`目录下，按规划、研究、开发、参考分类组织
7. 回答问题的语气禁止阿谀奉承，不用故意谄媚，只说问题的解决办法和思路
8. 不要每次改动之后都给我说百分百没问题！
9. git提交规则：
   - 改动了代码编译成功后，不要直接提交，不然我都看不到改动是什么 
   - 能使用中文的描述必须使用中文
   - 当你生成 git commit 信息时：
   - 使用 Co-Authored-By 信息（如果需要）：Co-Authored-By: claude code 助手 <zhouyohu@163.com>
   - 不使用默认的 Claude 署名
   - 提交信息必须通过 Bash heredoc (cat <<'EOF') 传入，禁止使用 PowerShell here-string (@'...'@)，避免 @ 与邮箱等内容的冲突
   - 提交信息的开始和结束不能有 @ 符号
   - 合并远程更新使用 git rebase，避免产生多余的 merge commit
   - 执行 git pull 时使用 git pull --rebase
   - Git 提交信息规范
```txt
<type>(<scope>): <subject>

[body]

[footer]

## Type（必选其一）
- `feat` - 新功能
- `fix` - 修复bug
- `docs` - 文档
- `style` - 代码格式（不影响运行）
- `refactor` - 重构
- `perf` - 性能优化
- `test` - 测试
- `build` - 构建/依赖
- `ci` - CI配置
- `chore` - 杂项
- `revert` - 回滚

## 规则
1. `subject`：祈使句、现在时，不超过50字，**英文首字母小写**，结尾不加句号
2. `scope`（可选）：模块名，如 `auth`、`api`
3. `body`（可选）：解释为什么，每行≤72字符
4. `footer`（可选）：`Closes #123` 或 `BREAKING CHANGE: 说明`
```
10.  Qt 界面样式禁止在 C++ 代码中通过 `setStyleSheet` 硬编码，所有样式统一写入 `src/app/resources/styles/` 下的 QSS 文件中，通过 `setObjectName` 选择器定位控件
11.  增加了新的代码片段后，必须看看是否需要为新增的代码片段引入必要的头文件
12.  每次会话开始时先读取 `ideas.md`，了解待办想法的最新进度

## 第三方依赖
项目集成了以下第三方库，均已在CMake中配置为静态编译：
- Qt 5.14.2（Core、Gui、Widgets、PrintSupport、Test、Xml、Svg），Qt使用官方编译的二进制发布包，默认是共享库
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
 - Qt使用的SDK是 5.14.2 msvc2017_64，但是编译器使用的是 MSVC2019，实际测试下来 MSVC2019编译环境完全兼容 MSVC2017 编译的 Qt 库
 - 可以直接在终端中执行 `scripts/build_ninja.bat` 构建项目。libpng 的 MSYS 环境兼容性问题已通过 `cmake/libpng/patch_msys_env.cmake` 补丁解决。