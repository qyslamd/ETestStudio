# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述
这是一个基于Qt/C++的学习研究项目，目标是实现自动化测试系统实现其核心功能。项目采用CMake构建系统，支持Windows平台。后续考虑迁移并支持常见的Linux发行版。

## 常用命令
### 构建项目
```bash
# 全量构建（默认 debug 64位）
scripts/build_ninja.bat

# 全量构建 32 位 debug
scripts/build_ninja.bat -a x86

# 仅构建主程序
scripts/build_ninja.bat -t debug -m ETestStudio

# 构建 + 部署 + 打包安装程序
scripts/build_ninja.bat -t relwithdebinfo -m ETestStudio -p

```
- 自动配置VS2019 x64编译环境
- 使用Ninja生成器构建
- 构建输出目录：`build/ninja-debug/`（debug）、`build/ninja-relwithdebinfo/`（relwithdebinfo）、`build/ninja-release/`（release）
- 支持两种参数模式：旧模式（位置参数 `build_ninja.bat debug deploy`）和新模式（显式参数 `-t <type> -m <target> -d/-p`），详见 `scripts/build_ninja.bat -h`
- 可执行文件输出到：`build/ninja-debug/bin/`

## 代码风格
- 所有生成的代码需要遵循clang-format的Google C++风格规范
- for语句，如果只有一行，也请加上`{}`
- if语句，如果只有一行，也请加上`{}`
- Qt 界面样式禁止在 C++ 代码中通过 `setStyleSheet` 硬编码
  - 所有样式统一写入 `src/app/resources/styles/` 下的 QSS 文件中；优先通过 `setObjectName` 使用 `#objectName` 选择器定位控件
  - 对于QPushButton或者QToolButon而言，如果明确不需要对外观定制，用QPushButton,否则用QToolButton
  - 如果 QSS 必须使用 Type Selector、Descendant Selector 或 Child Selector 定位带命名空间的 Qt/C++ 类，必须写 Qt 元对象系统使用的完整命名空间选择器：将 C++ 命名空间中的 `::` 替换为 `--`

## 代码架构
项目采用分层、模块化架构设计，`src/` 下每个子目录对应一个独立的 CMake 构建目标：

### 核心层（Core Layer）
| 模块           | 目录        | 说明                                                                                                                       |
| -------------- | ----------- | -------------------------------------------------------------------------------------------------------------------------- |
| **etest_core** | `src/core/` | 基础服务与核心功能：配置管理（ConfigManager）、日志（spdlog）、插件框架（PluginManager）、项目管理、崩溃处理、通用工具函数 |
| **etest_api**  | `src/api/`  | 纯头文件接口库（IEditor 等），无链接依赖                                                                                   |

### 业务模块层（Business Module Layer）
| 模块               | 目录            | 说明                                                              |
| ------------------ | --------------- | ----------------------------------------------------------------- |
| **etest_topology** | `src/topology/` | 拓扑编辑器（ETest 核心功能），实现场景/视图/项/撤销/序列化/导出等 |
| **etest_protocol** | `src/protocol/` | ICD协议编辑器，实现节点树/位图视图/属性面板                       |

### 共享 UI 层（Shared UI Layer）
| 模块         | 目录         | 说明                                                                                         |
| ------------ | ------------ | -------------------------------------------------------------------------------------------- |
| **etest_ui** | `src/libui/` | 跨模块共享的 UI 组件库（如 DockTitleBar 自定义标题栏），被 topology、protocol 及独立产品共用 |

### 工具库层（Utility Layer）
| 模块               | 目录               | 说明                                       |
| ------------------ | ------------------ | ------------------------------------------ |
| **etest_tuxsaver** | `src/tuxsaver/`    | 屏保动画模块，独立静态库                   |
| **icd_utility**    | `src/icd_utility/` | ICD 数据格式工具库（纯 C++17，无 Qt 依赖） |

### 应用层（Application Layer）
| 模块      | 目录       | 说明                                                                                                                                   |
| --------- | ---------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| **etest** | `src/app/` | 主程序可执行文件，集成所有模块。基于 Qt-Advanced-Docking-System 实现停靠界面布局，包含编辑器管理、欢迎页、终端、输出面板、各类对话框等 |

### 模组依赖关系
```
etest (主程序)
├── etest_topology ─┬── etest_ui ─── etest_core
├── etest_protocol ─┤              └── Qt5::Widgets
├── etest_tuxsaver  └── etest_api (header-only)
├── icd_utility
└── Qt5 / 第三方库 (QScintilla, SARibbon, QXlsx, libharu, QADS...)
```

## 附属工具约束

`src/tools/topology-editor`、`src/tools/protocol-editor`、`src/tools/test-program-editor` 是三个附属工具，
各自输出独自可用的文件格式（`.etopo` / `.eproto` / `.tcase`），
集成在主程序 ribbon 中随主程序发布。通过 `BUILD_ETEST_TOOLS` CMake 选项控制编译。

**核心约束**：功能变更不得导致一个产品引入另一产品所属模块的链接依赖：
- `topology-editor` 不得链接 `etest_protocol`、`etest_program` 或 `icd_utility`
- `protocol-editor` 不得链接 `etest_topology` 或 `etest_program`
- `test-program-editor` 不得链接 `etest_topology` 或 `etest_protocol` 或 `icd_utility`

增强功能（如跨模块的 ICD 信号选择、同步编排）只能实现在 `etest_app`（主程序），
下层模块只提供接口和纯数据模型。


## 项目规则
必须严格遵守以下规则：
1. 每次代码改动完成后都**必须**单独一行说一句 `我超威😁`
2. 所有生成的C++代码**必须**遵循Google C++ Style Guide，Qt界面代码需将UI初始化和信号槽连接分离到`initUi()`和`initSignals()`函数
3. 所有的Markdown文档**必须**遵循Google文档风格指南
4. 代码设计需**必须**遵循`SOLID`原则
5. 所有会修改git仓库的命令必须经过我的确认，使用查看类git命令（git status、git log等）的没有限制
6. 文档统一存放在`docs/`目录下，按意图放入到不同的目录中
7. 回答问题的语气**禁止**阿谀奉承，不用故意谄媚，只说问题的解决办法和思路，切实中肯，言简意赅
8. 不要每次改动之后都给我说百分百没问题！
9. 使用spdlog编写日志的时候，**尽可能**使用我封装好的Logger类及其宏定义
10. `powershell`命令在windows上很恼火，除了必要的脚本使用bat或者ps1编写外，**能用Python脚本的一律用python**
11. 改动完成代码，编译无误之后，**禁止**直接提交代码！除非是使用git worktree方式。
12. git提交规则：
   - 能使用中文的描述必须使用中文
   - 当你生成 git commit 信息时：
   - 使用 Co-Authored-By 信息（如果需要）：Co-Authored-By: claude code 助手 <zhouyohu@163.com>
   - 不使用默认的 Claude 署名
   - 提交信息必须通过 Bash heredoc (cat <<'EOF') 传入，禁止使用 PowerShell here-string (@'...'@)，避免 @ 与邮箱等内容的冲突
   - 提交信息的开始和结束不能有 @ 符号
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
1. `subject`：祈使句、现在时，不超过50字，**中文描述**，结尾不加句号
2. `scope`（可选）：模块名，如 `auth`、`api`
3. `body`（可选）：解释为什么，每行≤72字符
4. `footer`（可选）：`Closes #123` 或 `BREAKING CHANGE: 说明`
```

## 调试和定位问题
1. 该项目是一个Qt的GUI程序。大模型Agent工具无法运行查看运行后的状态，唯一的手段就是通过增加日志。我是用了spdlog封装了 `LOG_INFO`、`LOG_DEBUG`等，必要的时候添加上日志！

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