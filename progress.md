# IATP — 进度日志

## 2026-05-22

### 帧协议编辑器实现
- icd_utility 扩展：Frame/Node 新增 setter + remove 方法，Repository 新增 remove_frame
- JSON 序列化：新增 json_serializer 实现 .eproto 格式序列化写出
- ProtocalEditorWidget：三面板联动（树↔位图↔属性面板），双向信号连接
- IcdNodeTreeWidget：加载 Repository 展示帧/节点树，搜索过滤
- IcdBitLayoutView：收集叶子节点按位宽着色渲染，支持点击高亮
- IcdPropertyPanel：动态表单编辑（基本/帧属性/节点属性/缩放/扩展属性），信号安全连接管理
- .eproto 文件加载/保存/新建帧/删除帧
- protocal-demo：文件菜单增加「打开...」（Ctrl+O）
- Schema XML → .eproto 格式转换验证（A429_11_ISI_02_发送_Label221_6272T_11.xml）
- 已合并到 master（71d4164）

### 当前状态
- **阶段1.5（编辑器完善）** ✅ 已完成（拓扑+帧协议编辑器）
- **阶段2.5（拓扑增强）** ✅ 已完成
- **阶段2（HAL层）** ❌ 待开始

### 已完成
- 监听器模式设计（commit 36dd608）

### 监听器（Monitor）功能实现
- 数据模型：新增 TopologyMonitorTap / TopologyMonitor 结构体，monitors_ 成员，monitor 增删改查 + tap 管理
- UndoCommands：新增 AddMonitorCommand / RemoveMonitorCommand / TapConnectionCommand / UnTapConnectionCommand + ResizeItemCommand::Monitor
- JSON 序列化：monitors 数组读写（含 taps）
- 设备面板：新增 Monitor-4CH 条目（独立 MIME 类型标识）
- MonitorItem 图形项：继承 TopologyBlockItem，紫色系 block + 显示名称/设备类型/挂载数 + 8方向缩放
- TopologyScene：monitor_items_ 管理、tap 模式交互（startTapMode/finishTap/cancelTapMode）、tap 虚线视觉
- TopologyEditorWidget：挂载模式工具栏按钮（topo_tap_dark.svg 图标）、onDropMonitor 处理、delete 支持
- 属性面板：新增 PageMonitor（名称/设备类型/已挂载连线列表）
- 大纲面板：新增 Monitors 分支，展开显示已挂载连线

## 2026-05-22（下午）

### 协议管理器页面实现
- 新增 ProtocolManagerWidget：协议树（.eproto 文件列表 + 帧子节点）、+新建/导入XML/重命名/删除
- SidebarWidget 集成：添加协议管理按钮（第6个面板）
- main_window 信号连接（projectOpened → refreshList, openFileRequested → editor open）
- QSS 样式适配

### XML 导入功能实现（onImportXml）
- ICDConfig 多帧配置 → Loader::init() 自动解析
- ICDData 单帧文件 → parse_xml_frame() + build_repository() 转换
- 自动检测 XML 根元素（兼容 xmlns 属性）
- 保存 .eproto 到协议目录，注册并打开

### 单元测试
- 新增 test_xml_import：单帧/配置/无效 XML 三条管线测试
- 新增 test_schema_import：批量导入测试（--schema_dir 参数或 SCHEMA_DIR 环境变量）
- 测试数据：frame-simple.xml + config-simple.xml
- XML 检测修复：head.contains 改为不含尾部 `>` 的匹配，兼容 xmlns 属性

## 2026-05-20

### 已完成
- 分析 FlowGraph 项目（`D:\workspace\self\works\workspace\flow-graph\FlowGraph`）的设计模式
- 识别 7 个可借鉴到拓扑编辑器的设计点
- 编写研究文档：`docs/02-研究/FlowGraph拓扑设计借鉴.md`（含必要代码说明）
- 编写实施计划：`docs/01-规划/阶段2.5-拓扑编辑器增强计划.md`
- 更新 task_plan.md：新增阶段 2.5 拓扑编辑器增强（7 项任务，预估 8.5 天）
- 更新 findings.md：增加 FlowGraph 设计分析章节

### 当前状态
- **阶段1（基础框架）** ✅ 已完成
- **阶段1.5（编辑器完善）** ✅ 已完成
- **阶段2.5（拓扑编辑器增强）** 🔄 规划中（0%）
- **阶段2（HAL层）** ❌ 待开始

### 新增文档
1. `docs/02-研究/FlowGraph拓扑设计借鉴.md` — 7 项设计点的详细分析
2. `docs/01-规划/阶段2.5-拓扑编辑器增强计划.md` — 实施计划

## 2026-05-19

### 计划变更
- 用户决定改变开发优先级：**先完善拓扑编辑器和帧协议编辑器**，再进入 HAL 层开发
- 新增阶段 **1.5 编辑器完善**，排在阶段2之前
- task_plan.md 已更新：进度总览 + 阶段1.5 拓扑编辑器 6 项改进任务

### 拓扑编辑器现状评估
已完成拓扑编辑器全部源文件审查，结论：**核心功能完整可用**，6 项锦上添花的改进任务已列入计划：
1. 暗色主题适配
2. 连线方向校验
3. 复制/粘贴
4. 导出为图片
5. 缩放状态显示
6. 从模板新建设备

### 当前状态
- **阶段1（基础框架）** ✅ 已完成
- **阶段1.5（编辑器完善）** 🔄 进行中（0%）
- **阶段2（HAL层）** ❌ 待开始
- 阻塞项：无

## 2026-05-14

### 已完成
- IATP设计方案（`docs/thinking/IATP_设计方案.md`）设计盘问（brainstorming）
- 验证"Signal抽象"在三领域（工控/航空/车载）下的可行性：用户做过A429项目，使用"迎角50°"高级语义而非底层协议字段操作，证明Signal抽象本身够用
- LSD信号映射流程仍需重新设计

### 设计盘问发现的缺口
1. **三领域统一问题**：工控（模拟/数字信号）、航空（协议报文）、车载（事件驱动总线）测试范式不同，但可以共用"Signal抽象 + 领域特化ICD映射 + 领域特化硬件插件"路线，领域专用Lua API不是必须的
2. **A429映射暴露SignalMapper缺口**：用户实际项目经验 — "设置迎角50°"需要填充data字段 + 自动设置SSM=3 + 计算并填充parity。当前设计方案SignalMapper只有一对一转换规则，缺少多字段/级联写入支持
3. **Engine同步阻塞问题**：IICDEngineInterface是同步API，但硬件响应可能几十到几百毫秒，Lua VM在此期间阻塞。暂停/恢复/断点的调试功能和同步阻塞模型冲突 — 用户承认ICD层"还没想好"
4. **QPluginLoader同进程风险**：厂商SDK质量参差不齐，一个插件段错误导致整个进程崩溃。讨论结论：接口设计上预留IPC边界（IDevicePlugin参数只用基本类型/QVariant/QByteArray），为将来通过QLocalSocket做进程转发做准备

### 验证通过的架构决策
- **Lua选型确认**：非程序员用GUI/JSON，高级用户写Lua，金字塔模型成立
- **JSON中间格式价值确认**：可diff、可review、可在CI中做语法校验，Excel二进制无法做版本控制
- **Signal抽象足够跨领域**：A429项目已验证，不需要为每个领域定制Lua API

### 当前状态
- **阶段1全部完成** ✅
- 设计方案文档已完成，但ICD层设计待修订
- 下一步：**阶段2 设备管理（HAL层）**，ICD层设计需同步修改

### 阻塞项
- ICD层同步/异步边界未定义，SignalMapper多字段映射未设计。建议在设计修订前，阶段2（HAL层）优先开始，阶段3（ICD层）设计待修订后再执行

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
