# IATP — 进度日志

## 2026-05-11（下午）

### 已完成
- IATP设计方案文档编写完成（`docs/thinking/IATP_设计方案.md`），11章完整方案
- 方案内容：项目概述（7项痛点）、需求分析（8项功能需求）、六层架构设计、技术选型论证（Qt/Lua/CMake等7项）、各层接口级详细设计、数据流、关键技术方案、编辑器设计、远期展望、开发计划（6阶段+里程碑）、验证方案
- 技术选型论证：Qt vs Electron、Lua vs Python/JS、QPluginLoader vs 独立进程
- 与凯云ETest对比：9个维度差异化分析
- 开发计划重新规划：6阶段+6里程碑，总预估17.5周
- task_plan.md全面更新：阶段2任务拆分细化（2.1~2.5五类子任务），新增里程碑表，新增决策记录2条

### 新增决策
1. 项目正式名称定为IATP（综合性自动化测试平台）
2. 设计方案文档作为项目正式设计文档，与架构梳理.md并存

### 当前状态
- **阶段1全部完成** ✅
- 设计方案文档已完成
- 下一步：**阶段2 设备管理（HAL层）**

### 阻塞项
- 无

## 2026-05-11（上午）

### 已完成
- 架构梳理V1.0文档完成（替换原草案），10项关键决策逐项讨论确认
- task_plan.md根据架构V1.0全面更新（六层架构、阶段2-6重新规划）
- 控制流指令设计（LOOP/WHILE/IF + 条件表达式统一 + 嵌套约束）

### 架构V1.0关键决策
1. 五层→六层（新增用例管理层）
2. DataPool融入ICD（仅离散事件型Pub/Sub）
3. TCP/UDP作为ICD传输通道选项，不在HAL层
4. 新增IVisaPlugin、IPulsePlugin
5. UUID信号标识机制
6. 帧编辑器/ICD编辑器/拓扑编辑器共享数据模型
7. JSON用例格式v1.0（10基本指令+3控制流指令）
8. WHILE必填timeout防止死循环
9. MVP仅Lua脚本引擎
10. MVP同进程插件隔离(QPluginLoader)

### 当前状态
- **阶段1.3全部完成** ✅
- 架构梳理V1.0文档已完成
- 下一步：**阶段2 设备管理（HAL层）**

### 阻塞项
- 无

## 2026-05-09

### 已完成
- GlobalExceptionHandler实现（信号捕获6种、Qt消息重定向、单例QObject、exceptionCaught信号）
- main.cpp初始化顺序调整：Logger → GlobalExceptionHandler → CrashHandler
- GlobalExceptionHandler单元测试（4个常规 + 5个DISABLED崩溃测试）
- run_disabled_test.bat.in模板，含disabled测试的模块生成专用bat脚本
- CMakePresets新增ninja-relwithdebinfo配置/构建/测试预设
- build_ninja.bat支持构建类型参数[debug|relwithdebinfo|release]
- WindowsCrashHandler添加MiniDumpWriteDump生成.dmp转储文件
- WindowsCrashHandler添加AddVectoredExceptionHandler优先捕获崩溃
- 崩溃弹窗仅在QApplication环境中显示（inherits判断）
- 日志/崩溃路径从Documents迁移到AppData/Local/etest_demo/
- RelWithDebInfo构建与崩溃调试指南文档
- 主程序崩溃测试验证通过（.log + .dmp均生成）
- 会话持久化：captureSessionData/writeSessionFile/restoreSession
- EditorManager::openFiles() 接口
- PanelContainerWidget::setMaximized(bool) setter
- 工具/帮助菜单补全，修复空菜单崩溃

### 当前状态
- **阶段1.3全部完成** ✅
- 下一步：**阶段2 设备管理**

### 阻塞项
- 无

## 2026-05-08

### 已完成
- TerminalPanel嵌入式PTY终端实现（scrollback缓冲、Shell退出重启、VT100解析）
- 中央编辑区占位widget暗色背景样式
- SettingsWidget设置页面（独立QDialog、分类树+表单、ConfigManager绑定）
- FileTypeIconProvider文件类型图标（11类Seti风格彩色SVG图标）
- SearchWidget全局搜索（项目文本搜索、结果分组、点击跳转行、Ctrl+Shift+F）
- EditorManager::openFileAtLine()搜索结果跳转支持
- SearchWidget QSS暗色主题样式
- 更新项目计划文件（Phase 1.3标记为完成）

### 当前状态
- **阶段1.3已完成** ✅
- 下一步：**阶段2 设备管理**

### 阻塞项
- 无

## 2026-05-07

### 已完成
- 全局字体设置为微软雅黑（`main.cpp`中`QApplication::setFont`）
- QADS标题栏按钮按objectName单独隐藏（PanelDock + AuxSidebarDock）
- PanelContainerWidget最大化/还原功能实现，dark主题SVG图标替换
- 创建项目计划文件（task_plan.md, findings.md, progress.md）

### 当前状态
- **阶段1.3进行中**，完成度约80%

### 阻塞项
- 无
