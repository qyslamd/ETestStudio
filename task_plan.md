# IATP — 任务计划

## 项目概述
综合性自动化测试平台（IATP），Qt/C++实现，CMake构建。采用六层架构（架构梳理V1.0）。详细设计见 `docs/thinking/IATP_设计方案.md`。

---

## 当前进度总览

| 阶段 | 状态 | 完成度 |
|------|------|--------|
| 1 基础框架搭建 | ✅ 已完成 | 100% |
| 2 设备管理（HAL层） | ❌ 未开始 | 0% |
| 3 ICD信号层 | ❌ 未开始 | 0% |
| 4 用例管理层 | ❌ 未开始 | 0% |
| 5 测试引擎层 | ❌ 未开始 | 0% |
| 6 测试与优化 | ❌ 未开始 | 0% |

---

## 里程碑

| 里程碑 | 完成标志 | 对应阶段 |
|--------|---------|---------|
| M1：框架就绪 | 主窗口可运行，插件可加载，编辑器可使用 | 阶段1 ✅ |
| M2：设备就绪 | 所有插件接口定义完成，Mock插件可运行，设备树可展示，自检通过 | 阶段2 |
| M3：信号就绪 | ICD信号映射可用，协议转换正确，拓扑编辑器可连线 | 阶段3 |
| M4：用例就绪 | JSON用例可编辑、校验、转换为Lua脚本 | 阶段4 |
| M5：引擎就绪 | Lua脚本可执行、调试、生成报告 | 阶段5 |
| M6：产品发布 | 全链路闭环测试通过，安装包可交付 | 阶段6 |

---

## 阶段1 基础框架搭建（已完成）

### 1.1 开发环境搭建 ✅
- [x] CMake + Ninja构建环境，VS2019 x64编译
- [x] 第三方依赖集成（Qt/QADS/QXlsx/spdlog/gtest/lua/libharu/QScintilla等）
- [x] 代码格式化（clang-format + Google C++风格）

### 1.2 核心基础设施 ✅
- [x] Logger（spdlog异步，多sink，运行时级别切换）
- [x] ConfigManager（QSettings INI存储，模板get/set，JSON导入导出）
- [x] CrashHandler（VEH + MiniDump，Windows平台）
- [x] GlobalExceptionHandler（信号捕获6种、Qt消息重定向）
- [x] 异常体系（ByteException/FileException/StringException/TimeException）
- [x] 工具类（FileUtil/StringUtil/ByteUtil/TimeUtil）
- [x] 自动备份（BackupManager）
- [x] 单实例检测（SingleInstance）

### 1.3 主窗口框架 ✅
- [x] 通用插件框架（PluginManager + IPlugin + IDevicePlugin）
- [x] 主窗口6区布局（QADS dock: 活动栏/侧边栏/编辑区/底部面板/属性面板/状态栏）
- [x] 项目管理（.etproj格式、创建/打开/关闭/最近项目）
- [x] 文件浏览器（QFileSystemModel、右键CRUD、双击打开、文件类型图标）
- [x] 多标签编辑器（QScintilla、语法高亮、脏标记、拖拽tab）
- [x] 活动栏与视图切换（7按钮、toggle显隐）
- [x] 底部三面板（OutputPanel日志、ProblemsPanel、TerminalPanel嵌入式PTY终端）
- [x] 全局搜索（SearchWidget、项目文本搜索、点击跳转行、Ctrl+Shift+F）
- [x] 设置页面（SettingsWidget独立对话框、分类树+表单、ConfigManager双向绑定）
- [x] 会话持久化（captureSessionData/writeSessionFile/restoreSession）
- [x] 单元测试（10+测试文件）
- [x] AnimationDialog动画对话框基类 + 版本管理模块

### 遗留可选项
- [ ] SidebarWidget Git/调试/扩展页仍为占位符（P2，后续阶段按需实现）

---

## 阶段2 设备管理（HAL层）

> 架构V1.0决策：HAL层只保留物理硬件插件，TCP/UDP作为ICD传输通道选项，不在HAL层

### 2.1 插件基类增强
- [ ] IDevicePlugin添加`selfTest()`通用自检接口
- [ ] IDevicePlugin添加`simulate(bool enable)`虚拟设备切换
- [ ] IDevicePlugin添加`configMetaData()`配置参数元数据
- [ ] DeviceStatus枚举增加`Simulated`状态
- [ ] DeviceInfo结构体已有bus_number/slot_number/card_serial字段

### 2.2 已有插件接口完善
- [ ] IADevicePlugin v3.0：已有完整头文件（耦合6种+触发8种+读取模式4种+扫描表+原始码），需补充剩余Mock实现
- [ ] IDADevicePlugin：已有头文件（writeChannel/readbackChannel），需补充Mock实现
- [ ] ISerialDevicePlugin→ISerialPlugin重命名：增加SerialMode(RS232/422/485)、DataBits/Parity/StopBits配置
- [ ] ICANPlugin增强：增加CanMode/CanFrameType/CanFrame结构体、IDFilter、fd_format/bit_rate_switch
- [ ] IArinc429Plugin增强：增加Arinc429Word结构体（label/sdi/data/ssm/parity）

### 2.3 新增插件接口
- [ ] IDioPlugin：DioDirection枚举、setDirection/readInput/setOutput
- [ ] IPulsePlugin：PulseMode枚举(FreqOut/PulseOut/CountOut/QuadratureIn)、PulseConfig结构体、startOutput/stopOutput/readFrequency/readCount
- [ ] IMil1553Plugin：Mil1553Mode枚举(BC/RT/MT)、Mil1553Message结构体、bcSchedule/bcSendMessage/rtSetResponse/mtStartMonitor
- [ ] IVisaPlugin：openResource/closeResource/sendCommand/query、自检用*IDN?、simulate(bool)

### 2.4 Mock插件实现
- [ ] MockADPlugin完善（已有基础，需补充完整功能）
- [ ] MockDAPlugin完善（已有基础）
- [ ] MockSerialPlugin→MockSerialPlugin重命名（已有基础，需适配新接口）
- [ ] MockCANPlugin增强（已有基础，需适配CanFrame结构）
- [ ] MockA429Plugin增强（已有基础，需适配Arinc429Word结构）
- [ ] MockDioPlugin新增
- [ ] MockPulsePlugin新增
- [ ] MockMil1553Plugin新增
- [ ] MockVisaPlugin新增

### 2.5 设备管理UI与流程
- [ ] HardwareTreeWidget增强：树形视图（厂家→分类→设备→通道），VISA类无通道
- [ ] 设备状态图标（在线/离线/异常/模拟四种状态，不同颜色）
- [ ] 设备自检流程：系统启动→PluginManager.loadAll()→遍历IDevicePlugin→openDevice()→selfTest()
- [ ] VISA设备自检：发送*IDN?验证
- [ ] Dry Run支持：全局开关+单设备开关，simulate()模式切换

### 预估工期：3周

---

## 阶段3 ICD信号层

> 架构V1.0决策：DataPool融入ICD（仅离散事件型Pub/Sub），UUID标识信号，传输通道（串口/TCP/UDP）在此层选择

### 3.1 核心数据模型
- [ ] SignalMapper：UUID→deviceId+channelId+protocolId映射，转换规则（线性/多项式/枚举/脚本）
- [ ] ProtocolRegistry：协议定义仓库，Protocol{字段列表+字节序+校验+pack/unpack}
- [ ] RawData结构体：deviceId/channelId/data/timestamp/protocolId
- [ ] SignalValue结构体：signalId/value/timestamp/isValid/quality/sourceInfo

### 3.2 转换与传输
- [ ] 协议转换引擎：pack(工程值→原始字节)/unpack(原始字节→工程值)
- [ ] 传输通道层：串口/TCP/UDP通道抽象，ICD选择通道发送原始字节
- [ ] 传输通道实现：串口→HAL ISerialPlugin，TCP/UDP→Qt网络模块

### 3.3 故障注入与缓存
- [ ] FaultManager：7种故障类型（死滞/偏置/CRC错误/校验错误/延迟/丢包/采样率异常）
- [ ] 故障清除：clearFault(signalId)/clearAllFaults()，用例结束自动清除
- [ ] SignalValueCache (CVT)：信号最新值缓存，Engine快速读取
- [ ] DataPool (Pub/Sub)：离散事件型发布订阅

### 3.4 编辑器UI
- [ ] 帧协议编辑器UI：ProtocolRegistry可视化编辑前端，字段删除引用检查
- [ ] ICD编辑器UI：SignalMapper表单/表格编辑方式
- [ ] 拓扑编辑器UI：SignalMapper图形化连线编辑（Qt GraphicsView），可视化元数据与映射数据分开存储
- [ ] XML/JSON/YAML/Excel导入导出

### 3.5 Engine接口
- [ ] IICDEngineInterface：setSignal/getSignal/verifySignal/waitForSignal/injectFault/clearFault/clearAllFaults

### 预估工期：4周

---

## 阶段4 用例管理层

> 架构V1.0决策：JSON核心中间格式，脚本代码是中间产物，10种基本指令+3种控制流指令(LOOP/WHILE/IF)

### 4.1 JSON用例格式
- [ ] JSON用例格式v1.0完整Schema定义
- [ ] 条件表达式统一：target+op+value结构，IF/WHILE/WAIT复用
- [ ] 控制流指令：LOOP(固定次数)、WHILE(条件循环+interval+timeout)、IF(条件分支)
- [ ] 嵌套约束：steps/then_steps/else_steps内不允许嵌套控制流

### 4.2 格式转换
- [ ] JSON→Lua转换器：所有指令类型到Lua脚本
- [ ] Excel→JSON转换器：标准模板列映射，Sheet=用例
- [ ] YML→JSON转换器：结构完全对应，天然互换
- [ ] JSON Schema校验：语法校验+嵌套约束检查

### 4.3 用例管理
- [ ] 用例CRUD：创建/读取/更新/删除
- [ ] 版本管理：作者、修订记录
- [ ] 用例编辑器UI：可视化编辑界面

### 预估工期：2.5周

---

## 阶段5 测试引擎层

> 架构V1.0决策：仅Lua(sol2+Lua Debug Library)，Engine不直接操作数据池，所有操作通过ICD API

### 5.1 Lua引擎
- [ ] sol2嵌入，隔离VM
- [ ] IICDEngineInterface实现
- [ ] Lua API绑定10个：SetDevice/VerifyDevice/WaitFor/Delay/UserAction/TakePhoto/SetRecord/InjectFault/ClearFault/Log

### 5.2 执行控制
- [ ] 暂停/恢复/终止
- [ ] 调试器：断点(行断点+条件断点)/单步(Over/Into/Out)/变量监视/调用栈
- [ ] 故障自动清除：用例结束时自动清除所有活跃故障

### 5.3 记录与报告
- [ ] 数据记录：步骤执行记录、断言结果、故障注入记录
- [ ] 监控面板UI：实时日志/通道数据/变量值/执行进度
- [ ] 报告生成：MVP简单文本/HTML

### 预估工期：3周

---

## 阶段6 测试与优化

### 核心任务
- [ ] 全模块回归测试
- [ ] 全链路集成测试（HAL→ICD→用例→引擎→报告）
- [ ] Windows 10/11兼容性测试
- [ ] 压力/稳定性测试
- [ ] 用户体验优化
- [ ] 打包与文档

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
| 2026-05-11 | 正式项目名称定为IATP | 综合性自动化测试平台，覆盖工业控制+航空+车载三领域 |
| 2026-05-11 | IATP设计方案文档编写完成 | docs/thinking/IATP_设计方案.md，11章完整方案，含技术选型论证和开发计划 |
| 2026-05-14 | Signal抽象验证跨领域可行 | A429项目经验证明SetDevice("迎角",50°)高级语义可用，不需要领域专用Lua API |
| 2026-05-14 | ICD层SignalMapper需补充多字段映射 | A429场景下"迎角→data+SSM+parity"多字段级联写入，当前设计方案未覆盖 |
| 2026-05-14 | ICD层同步/异步问题待定 | Engine阻塞等待硬件返回时无法响应暂停/恢复等调试指令，用户承认"还没想好" |
| 2026-05-14 | QPluginLoader接口已预留IPC边界 | IDevicePlugin参数只用基本类型/QVariant，远期切换独立进程只需加proxy/stub层 |
