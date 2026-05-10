# ETest Demo — 任务计划

## 项目概述
仿凯云ETest测试系统，Qt/C++实现，CMake构建。采用六层架构（架构梳理V1.0）。

---

## 当前进度总览

| 阶段 | 状态 | 完成度 |
|------|------|--------|
| 1.1 开发环境搭建 | ✅ 已完成 | 100% |
| 1.2 核心基础设施 | ✅ 已完成 | 100% |
| 1.3 主窗口框架 | ✅ 已完成 | 100% |
| 2 设备管理（HAL层） | ❌ 未开始 | 0% |
| 3 ICD信号层 | ❌ 未开始 | 0% |
| 4 用例管理层 | ❌ 未开始 | 0% |
| 5 测试引擎层 | ❌ 未开始 | 0% |
| 6 测试与优化 | ❌ 未开始 | 0% |

---

## 阶段1.3 完成总结

### 已完成项目
- [x] 通用插件框架（PluginManager + IPlugin）
- [x] 主窗口6区布局（QADS dock: 活动栏/侧边栏/编辑区/底部面板/属性面板/状态栏）
- [x] 项目管理（.etproj格式、创建/打开/关闭/最近项目）
- [x] 文件浏览器（QFileSystemModel、右键CRUD、双击打开、文件类型图标）
- [x] 多标签编辑器（QScintilla、语法高亮、脏标记、拖拽tab）
- [x] 活动栏与视图切换（7按钮、toggle显隐）
- [x] 底部三面板（OutputPanel日志、ProblemsPanel、TerminalPanel嵌入式PTY终端）
- [x] 全局搜索（SearchWidget、项目文本搜索、点击跳转行、Ctrl+Shift+F）
- [x] 设置页面（SettingsWidget独立对话框、分类树+表单、ConfigManager双向绑定）
- [x] 文件类型图标（FileTypeIconProvider、11类Seti风格彩色SVG图标）
- [x] 会话持久化（captureSessionData/writeSessionFile/restoreSession，恢复编辑器/侧边栏/面板状态）
- [x] 窗口大小/位置/布局状态持久化（ConfigManager）
- [x] 单元测试（10+测试文件）
- [x] SingleInstance单实例检测
- [x] AnimationDialog动画对话框基类 + 版本管理模块
- [x] EphQtSwitchButton开关按钮控件

### 遗留可选项
- [ ] SidebarWidget Git/调试/扩展页仍为占位符（P2，后续阶段按需实现）
- [x] 异常框架补全（1.2遗留）— GlobalExceptionHandler已实现
- [x] 自动备份功能（1.2遗留）— BackupManager已实现

---

## 阶段2 设备管理（HAL层）

> 架构V1.0决策：HAL层只保留物理硬件插件，TCP/UDP作为ICD传输通道选项，不在HAL层

### 核心任务
1. **IDevicePlugin基类增强**：添加simulate()/selfTest()/deviceStatus()/configMetaData()通用接口
2. **IADevicePlugin v3.0**：耦合6种、触发8种、读取模式4种、扫描表、原始码读取（已有头文件，需补充剩余插件实现）
3. **IDADevicePlugin**：模拟量输出插件接口定义+Mock实现
4. **IDioPlugin**：开关量IO插件接口定义+Mock实现
5. **IPulsePlugin**：脉冲信号插件接口定义+Mock实现（FreqOut/PulseOut/CountOut/QuadratureIn）
6. **ISerialPlugin**：串口族插件接口定义+Mock实现（RS232/422/485）
7. **ICanPlugin**：CAN/CAN FD插件接口定义+Mock实现
8. **IArinc429Plugin**：A429插件接口定义+Mock实现
9. **IMil1553Plugin**：1553B插件接口定义+Mock实现（BC/RT/MT模式）
10. **IVisaPlugin**：VISA SCPI程控仪器插件接口定义+Mock实现
11. **设备管理器UI**：设备树侧边栏视图（厂家→分类→设备→通道，VISA类无通道）
12. **设备自检流程**：启动时统一自检，VISA发*IDN?验证
13. **Dry Run支持**：每个插件simulate()模式切换
14. **DeviceInfo增强**：bus_number/slot_number/card_serial字段

### 预估工期：3周

---

## 阶段3 ICD信号层

> 架构V1.0决策：DataPool融入ICD（仅离散事件型Pub/Sub），UUID标识信号，传输通道（串口/TCP/UDP）在此层选择

### 核心任务
1. **SignalMapper**：UUID→deviceId+channelId+protocolId映射，转换规则（线性/多项式/枚举/脚本）
2. **ProtocolRegistry**：协议定义仓库，Protocol{字段列表+字节序+校验+pack/unpack}
3. **协议转换引擎**：pack(工程值→原始字节)/unpack(原始字节→工程值)
4. **传输通道层**：串口/TCP/UDP通道抽象，ICD选择通道发送原始字节
5. **FaultManager**：故障注入管理（信号值死滞/偏置/CRC错误/延迟/丢包）
6. **SignalValueCache (CVT)**：信号最新值缓存，Engine快速读取
7. **DataPool (Pub/Sub)**：离散事件型发布订阅，UI监控订阅SignalValue通道
8. **帧协议编辑器UI**：ProtocolRegistry的可视化编辑前端，字段删除引用检查
9. **ICD编辑器UI**：SignalMapper的表单/表格编辑方式
10. **拓扑编辑器UI**：SignalMapper的图形化连线编辑（Qt GraphicsView），可视化元数据与映射数据分开存储
11. **XML/JSON/YAML/Excel导入导出**

### 预估工期：4周

---

## 阶段4 用例管理层

> 架构V1.0决策：JSON核心中间格式，脚本代码是中间产物，10种基本指令+3种控制流指令(LOOP/WHILE/IF)

### 核心任务
1. **JSON用例格式v1.0**：完整Schema定义，10种基本指令+3种控制流指令
2. **条件表达式统一**：target+op+value结构，IF/WHILE/WAIT复用
3. **控制流指令**：LOOP(固定次数)、WHILE(条件循环+interval+timeout)、IF(条件分支)
4. **嵌套约束**：steps/then_steps/else_steps内不允许嵌套控制流
5. **JSON→Lua转换器**：所有指令类型到Lua脚本的转换
6. **Excel→JSON转换器**：标准模板列映射，Sheet=用例
7. **YML→JSON转换器**：结构完全对应，天然互换
8. **JSON Schema校验**：语法校验+嵌套约束检查
9. **用例CRUD**：创建/读取/更新/删除
10. **版本管理**：作者、修订记录
11. **用例编辑器UI**：可视化编辑界面

### 预估工期：2.5周

---

## 阶段5 测试引擎层

> 架构V1.0决策：仅Lua(sol2+Lua Debug Library)，Engine不直接操作数据池，所有操作通过ICD API

### 核心任务
1. **Lua引擎集成**：sol2嵌入，隔离VM，C++端封装硬件控制供脚本调用
2. **IICDEngineInterface**：setSignal/getSignal/verifySignal/waitForSignal/injectFault/clearFault
3. **Lua API绑定**：SetDevice/VerifyDevice/WaitFor/Delay/UserAction/TakePhoto/SetRecord/InjectFault/ClearFault/Log
4. **执行控制**：暂停/恢复/终止
5. **调试器**：断点(行断点+条件断点)/单步(Over/Into/Out)/变量监视/调用栈
6. **数据记录**：步骤执行记录、断言结果、故障注入记录
7. **监控面板UI**：实时日志/通道数据/变量值/执行进度
8. **报告生成**：MVP阶段简单文本/HTML，远期PDF(libharu)/Excel(QXlsx)
9. **故障自动清除**：用例结束时自动清除所有活跃故障

### 预估工期：3周

---

## 阶段6 测试与优化

### 核心任务
1. 全模块回归测试
2. 全链路集成测试（HAL→ICD→用例→引擎→报告）
3. Windows 10/11兼容性测试
4. 压力/稳定性测试
5. 用户体验优化
6. 打包与文档

### 预估工期：2周

---

## 决策记录

| 日期 | 决策 | 原因 |
|------|------|------|
| 2026-05-07 | 全局字体设为微软雅黑过渡 | 默认宋体不好看，后续可替换为自定义ttf |
| 2026-05-07 | QADS标题栏按钮按objectName单独隐藏 | 整个titleBar()->hide()会连同tab一起隐藏 |
| 2026-05-07 | ActivityBar与Sidebar保持为独立CDockWidget | 合并后toggle行为异常 |
| 2026-05-08 | 设置页面改为独立QDialog而非侧边栏嵌入 | 类似VS Code以独立Tab打开设置，简化交互 |
| 2026-05-08 | 文件图标采用Seti风格（填充+底部彩色标签） | 纯描边图标在16px下难以区分，彩色标签一目了然 |
| 2026-05-08 | 全局搜索为主线程同步搜索（MVP） | 项目<10K文件够用，后续可改为QThreadPool |
| 2026-05-09 | 会话持久化采用JSON格式存入AppData/Local/session.json | 轻量、可读、与ConfigManager分离，关闭确认后才写盘 |
| 2026-05-09 | 日志/崩溃路径从Documents迁移到AppData/Local | 崩溃dump和日志是机器相关数据，不适合放用户文档目录 |
| 2026-05-09 | CrashHandler添加VEH+MiniDump | SetUnhandledExceptionFilter在gtest等SEH环境中不够优先，VEH在链最前端 |
| 2026-05-09 | 发布采用RelWithDebInfo而非Release | 带PDB的优化构建，崩溃dump可在任意机器用WinDbg/VS调试 |
| 2026-05-09 | disabled测试脚本加--gtest_catch_exceptions=0 | gtest默认SEH拦截崩溃，导致CrashHandler无法触发 |
| 2026-05-11 | 五层架构→六层架构（新增用例管理层） | 用例CRUD/校验/格式转换不应塞进Engine，Engine只负责执行 |
| 2026-05-11 | DataPool融入ICD，仅离散事件型Pub/Sub | MVP阶段信号量<100，ICD内SignalValueCache已够用，独立DataPool增加复杂度 |
| 2026-05-11 | TCP/UDP作为ICD传输通道选项，不在HAL层 | TCP/UDP不是硬件设备，是通信协议/传输方式，放HAL导致插件膨胀 |
| 2026-05-11 | 新增IVisaPlugin和IPulsePlugin | VISA设备在设备树上分类显示（无通道），脉冲是独立于DA/DIO的信号类型 |
| 2026-05-11 | 信号用UUID标识 | 改名不导致引用失效，帧编辑器/ICD/拓扑/用例全部基于UUID关联 |
| 2026-05-11 | 帧编辑器/ICD编辑器/拓扑编辑器共享SignalMapper数据 | 三种编辑方式操作同一份数据，可视化元数据与映射数据分开存储 |
| 2026-05-11 | JSON用例格式v1.0：10基本指令+3控制流指令 | JSON核心中间格式，LOOP/WHILE/IF支持简单控制流，嵌套控制流用Lua脚本 |
| 2026-05-11 | WHILE必填timeout防止死循环 | 条件循环必须有超时保护，interval防止CPU空转 |
| 2026-05-11 | MVP仅支持Lua脚本引擎 | 直接用Lua Debug Library实现调试，不做多语言抽象 |
| 2026-05-11 | 插件进程隔离：MVP同进程(QPluginLoader) | 简单、延迟低，后续评估独立进程隔离 |
