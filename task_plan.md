# IATP — 任务计划

## 项目概述
综合性自动化测试平台（IATP），Qt/C++实现，CMake构建。采用六层架构（架构梳理V1.0）。详细设计见 `docs/thinking/IATP_设计方案.md`。

---

## 当前进度总览

| 阶段 | 状态 | 完成度 |
|------|------|--------|
| 1 基础框架搭建 | ✅ 已完成 | 100% |
| 1.5 编辑器完善 | ✅ 已完成 | 100% |
| 1.6 GUI主题与图标管理 | ✅ 已完成 | 100% |
| 2.5 拓扑编辑器增强 | ✅ 已完成 | 100% |
| 2.5b 拓扑编辑器持续完善 | 🔄 进行中 | 待定 |
| 1.5.2 帧协议编辑器完善 | ✅ 已完成 | 100% |
| 2 HAL接口定义+Mock实现 | ❌ 未开始 | 0% |
| 3 ICD信号层 | ❌ 未开始 | 0% |
| 4 用例管理层 | ❌ 未开始 | 0% |
| 5 测试引擎层 | ❌ 未开始 | 0% |
| 6 设备管理-真实硬件对接 | ❌ 未开始 | 0% |
| 7 测试与优化 | ❌ 未开始 | 0% |

---

## 里程碑

| 里程碑 | 完成标志 | 对应阶段 |
|--------|---------|---------|
| M1：框架就绪 | 主窗口可运行，插件可加载，编辑器可使用 | 阶段1 ✅ |
| M2：HAL就绪 | 所有插件接口定义完成，Mock插件可运行，设备树可展示 | 阶段2 |
| M3：信号就绪 | ICD信号映射可用，协议转换正确，拓扑编辑器可连线 | 阶段3 |
| M4：用例就绪 | JSON用例可编辑、校验、转换为Lua脚本 | 阶段4 |
| M5：引擎就绪 | Lua脚本可执行、调试、生成报告 | 阶段5 |
| M6：硬件就绪 | 真实设备驱动可用，Dry Run/Real模式可切换，全链路闭环通过 | 阶段6 |
| M2.5：拓扑增强就绪 | 连线智能路由、公共基类提取、交互体验增强，topology-demo验证通过 | 阶段2.5 ✅ |
| M2.5b：拓扑编辑器持续完善 | 帧协议编辑器完善、属性面板优化、用户体验提升 | 阶段2.5b |
| M7：产品发布 | 全链路闭环测试通过，安装包可交付 | 阶段7 |

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
- [x] **主窗口迁移到 SARibbon 功能区**：QMainWindow → SARibbonMainWindow，Ribbon 替代传统菜单栏/工具栏
- [x] **UI 布局重构**：全 QADS 布局 → QSplitter 混合布局，活动栏/侧边栏/底部面板独立为普通 QWidget，QADS 仅管理编辑器区。附带修复关闭崩溃、Session 恢复、侧边栏显隐等问题。设计文档：`docs/01-规划/UI布局重构方案.md`（commit 3b526e1）
- [x] **core → etest_core 重命名**：统一 CMake 目标命名规范

### 遗留可选项
- [ ] SidebarWidget Git/调试/扩展页仍为占位符（P2，后续阶段按需实现）

---

## 阶段1.5 拓扑编辑器与帧协议编辑器完善（新增）

> 计划变更：优先完善已迁移到 `src/app/` 的拓扑编辑器和帧协议编辑器，再进入 HAL 层开发。

### 1.5.1 拓扑编辑器完善（6项）

- [x] **暗色主题适配**：TopologyScene 背景和 Item 颜色跟随应用主题，去掉硬编码浅色
- [x] **连线方向校验**：`canConnect()` 增加 Input→Output 方向匹配检查
- [x] **复制/粘贴**：选中 UUT/Device 后 Ctrl+C/V 复制粘贴支持
- [x] **导出为图片**：拓扑结构一键导出 PNG
- [x] **缩放状态显示**：状态栏显示当前缩放比例（如 `125%`）
- [x] **从模板新建设备**：右键菜单"从模板添加设备"，用已有 .dvt 文件快速添加

### 1.5.2 帧协议编辑器完善（已完成）

> 实际完成时间：2026-05-22

- [x] icd_utility 扩展：Frame/Node setter 方法、remove_child/remove_root、Repository remove_frame
- [x] JSON 序列化：.eproto 格式写入（json_serializer）
- [x] ProtocalEditorWidget：三面板联动（树↔位图↔属性面板）
- [x] IcdNodeTreeWidget：帧/节点树浏览 + 搜索过滤
- [x] IcdBitLayoutView：位级可视化布局展示
- [x] IcdPropertyPanel：动态属性编辑面板
- [x] .eproto 文件加载/保存/新建/删除帧
- [x] protocal-demo 文件菜单「打开...」（Ctrl+O）
- [x] Schema XML → .eproto 转换测试（A429_11_ISI_02_发送_Label221_6272T_11）

---

## 阶段1.6 GUI主题与图标管理（进行中）

> 创建全局 ThemeManager 和 IconProvider 单例，统一管理主题切换和图标加载。
> 设计文档：`docs/01-规划/全局IconProvider和ThemeManager设计.md`

### 1.6.1 创建 ThemeManager + IconProvider（已完成）
- [x] 创建 `src/app/ThemeManager.h/.cpp`：主题状态管理 + 信号机制 + QSS 加载 + detectDarkFromQss
- [x] 创建 `src/app/IconProvider.h/.cpp`：图标路劲解析 + 主题感知 + QCache 缓存
- [x] `src/app/CMakeLists.txt` 添加源文件，编译通过

### 1.6.2 FileTypeIconProvider 融合（已完成）
- [x] `loadDualThemeIcon()` 委托 `IconProvider::icon()`
- [x] 增加 `reload()` 方法，清理缓存并重建图标映射
- [x] FileExplorerWidget 连接 `themeChanged` → `reload()`

### 1.6.3 MainWindow 接入（已完成）
- [x] 移除 `applyTheme()`，添加 `onThemeChanged(bool)` slot
- [x] `initUi()` 中调用 `ThemeManager::instance()` 替换 `setDarkTheme()`
- [x] `initSignals()` 中连接 `themeChanged` → 同步 settings_dialog
- [x] ConfigManager 的 configChanged 监听移至 ThemeManager 内部

### 1.6.4 ActivityBarWidget 迁移（已完成）
- [x] 移除 `IconPair` 结构体，改用 `QStringList icon_names_`
- [x] 图标加载改为 `IconProvider::icon("project")`
- [x] 连接 `ThemeManager::themeChanged` → `reloadIcons()`

### 1.6.5 其他 widget 接入 themeChanged（已完成）
- [x] SearchWidget：使用 `IconProvider::icon("search")`，连接 `themeChanged` 刷新按钮图标
- [x] GitWidget：使用 `IconProvider::icon("refresh")`，连接 `themeChanged` 刷新按钮图标
- [x] BottomContainerWidget：使用 `IconProvider::icon("close")`，连接 `themeChanged` 刷新关闭按钮图标
- [x] ImageViewerWidget：使用 `ThemeManager::isDarkTheme()`，连接 `themeChanged` 刷新背景色

### 注意（跨模块限制）
- IcdBitLayoutView（protocal）、TopologyEditorWidget（topology）位于独立静态库中，不链接 app 模块，因此继续使用 `core::common::isDarkTheme()`（由 ThemeManager 同步，初始值正确）。场景绘制（drawBackground、topologyColors）已是动态读取，主题切换后自动生效。

### 预估工期：1 天（4 个子任务可分批实施）

## 阶段2.5 拓扑编辑器增强（已完成）

> 基于 FlowGraph 项目设计分析，对拓扑编辑器进行 7 项增强。实际完成时间跨度：2026-05-20 ~ 2026-05-22。

### 2.5.1 端口连接约束增强 ✅
- [x] 同方向阻止（Input→Input / Output→Output 不允许）
- [x] 输入端单连线检查
- [x] 功能类型匹配校验

### 2.5.2 TopologyBlockItem 公共基类提取 ✅
- [x] 新增 `TopologyBlockItem`，将 UutItem/DeviceItem 通用交互逻辑上提
- [x] UutItem/DeviceItem 改为只实现 `paintContent()` 和 `calcContentHeight()`
- [x] 消除约 50% 重复代码

### 2.5.3 NicePathMaker 智能路径路由 ✅
- [x] 新增 `TopologyPathRouter` 类：支持曲线/折线/直线三种风格
- [x] 折线模式自动避障绕行
- [x] ConnectionItem 引入路由引擎

### 2.5.4 连线风格右键切换 ✅
- [x] ConnectionItem 右键菜单增加连线风格切换子菜单

### 2.5.5 端口可视化风格多样化 ✅
- [x] PortItem/DevicePortItem 支持圆形和三角形两种绘制风格
- [x] 端口样式持久化（序列化/反序列化）

### 2.5.6 拖放创建 Item ✅
- [x] TopologyScene 增加 drag/drop 事件处理
- [x] 自定义 MIME 类型，支持从面板拖入创建设备
- [x] DevicePaletteWidget 拖放预览

### 2.5.7 Item Resize 手柄 ✅
- [x] TopologyBlockItem 增加 8 方向 resize 手柄
- [x] 拖拽手柄改变块大小，鼠标悬停光标变化

### 额外完成（超出原计划）
- [x] **可搜索大纲导航面板**：TopologyOutlineWidget，树形结构展示 UUT/Device/Connection，支持搜索过滤和点击导航
- [x] **设备面板搜索栏**：DevicePaletteWidget 增加过滤搜索
- [x] **PropertyPanelWidget 端口表重构**：QTableWidget → QTableView + QStandardItemModel + ComboBoxDelegate
- [x] **工具栏 SVG 图标**：TopologyEditorWidget 工具栏使用 SVG 图标
- [x] **拖放预览优化**：拖放时显示设备预览效果
- [x] **分布对齐优化**：编辑器内设备分布和对齐功能
- [x] **外观优化 + QSS 样式迁移**：面板标题头、暗色主题一致性调整

### 实际工期：约 3 天

---

## 阶段2.5b 拓扑编辑器持续完善（进行中）

> 在前7项增强基础上继续完善拓扑编辑器的交互体验和功能完整性。具体任务待与用户确认。

### 待定任务

- 帧协议编辑器完善（UI 骨架已有，save/saveAs 未实现，三面板无联动，无数据模型）

---

## 阶段2 HAL接口定义 + Mock实现（仅接口与模拟，无真实硬件）

> 本阶段只完成接口定义和Mock模拟实现，不涉及任何真实硬件对接。
> 架构V1.0决策：HAL层只保留物理硬件插件，TCP/UDP作为ICD传输通道选项，不在HAL层

### 2.1 插件基类增强
- [ ] IDevicePlugin添加`selfTest()`通用自检接口
- [ ] IDevicePlugin添加`simulate(bool enable)`虚拟设备切换
- [ ] IDevicePlugin添加`configMetaData()`配置参数元数据
- [ ] DeviceStatus拆分为DeviceConnection(Offline/Online/Error) + DeviceRunMode(Real/Simulated)
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
- [ ] 设备状态图标（在线/离线/异常/模拟/真实，不同颜色）
- [ ] 设备右键菜单：打开设备/关闭设备/查看详细信息
- [ ] 设备自检流程：系统启动→PluginManager.loadAll()→遍历IDevicePlugin→openDevice()→selfTest()
- [ ] VISA设备自检：发送*IDN?验证
- [ ] Dry Run支持：全局开关+单设备开关，simulate()模式切换

### 预估工期：2.5周

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
- [ ] 报告生成：MVP简单文本/HTML，支持编辑修改
- [ ] 报告导出保存

### 预估工期：3周

---

## 阶段6 设备管理-真实硬件对接

> 阶段2定义的插件接口对接真实硬件，实现Dry Run→Real模式切换。此时ICD/用例/Engine已就绪，可在真实硬件上跑通全链路。

### 6.1 真实设备驱动实现
- [ ] 各设备类型从Mock替换/扩展为真实驱动实现（AD/DA/DIO/Pulse/Serial/CAN/A429/1553B/VISA）
- [ ] 设备发现与枚举：总线扫描、板卡识别、设备列表自动发现
- [ ] VISA SCPI真实通信：基于visa32.dll进行真实仪器程控，*IDN?验证
- [ ] 串口族真实驱动：RS232/422/485基于Windows API串口通信
- [ ] CAN/CAN FD真实驱动：基于PCAN/Vector等硬件驱动
- [ ] ARINC 429真实驱动：基于专用板卡API
- [ ] 1553B真实驱动：基于板卡SDK实现BC/RT/MT模式
- [ ] DA/DIO/Pulse真实驱动：基于板卡API

### 6.2 模式切换与管理
- [ ] Dry Run ↔ Real模式切换：每个插件可独立切换模拟/真实模式，混搭运行
- [ ] 真实设备自检：自检流程对接真实设备状态查询，区分在线/离线/故障
- [ ] 硬件配置UI：设备参数配置界面（总线地址/板卡号/通道映射等）

### 6.3 全链路联调
- [ ] HAL与ICD全链路联调：真实设备→ICD解包→CVT→脚本验证完整闭环

### 预估工期：3周

---

## 阶段7 测试与优化

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
| 2026-05-19 | 新增阶段1.5：编辑器完善（拓扑+帧协议） | 用户决定先完善已迁移的编辑器，再进入HAL层；拓扑编辑器核心功能完整，6项改进已列入计划 |
| 2026-05-19 | 6阶段→7阶段重组：阶段2拆分为接口+Mock(新阶段2)与真实硬件(新阶段6) | 用户希望先定义好所有接口、用Mock模拟数据源，让ICD/Engine在Dry Run模式下开发调试，最后再对接真实硬件 |
| 2026-05-20 | DeviceStatus拆分为DeviceConnection+DeviceRunMode二维枚举 | 硬件树需展示"在线/离线/模拟/真实"四种状态，原单枚举不能区分连接状态和运行模式 |
| 2026-05-20 | 硬件树右键菜单增加"打开/关闭/查看详情"操作 | 用户认为设备管理器应有基本的设备启停和信息查看功能 |
| 2026-05-20 | 报告增加编辑+导出功能 | 用户希望在测试报告自动生成后能人工编辑修改再导出保存 |
