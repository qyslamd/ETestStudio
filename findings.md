# ETest Demo — 研究发现

## 代码架构现状

### 已完整实现的模块
- **MainWindow** (1106行): QADS dock布局、菜单栏/工具栏/状态栏、项目生命周期、编辑器操作、窗口状态持久化
- **ActivityBarWidget** (110行): 6个SVG图标按钮，切换侧边栏页面
- **SidebarWidget** (121行): 6个StackedPage，页面切换
- **FileExplorerWidget** (251行): QFileSystemModel文件树，右键菜单，双击打开
- **EditorManager** (491行): 多tab管理，脏文件检查，关闭操作，右键菜单
- **EditorWidget** (371行): QScintilla集成，多语言Lexer，VSCode Dark+配色
- **OutputPanel** (78行): 只读QTextEdit，彩色日志级别，自动滚动，5000行限制
- **PanelContainerWidget** (103行): QTabWidget容器，最大化/还原/关闭按钮
- **HardwareTreeWidget** (211行): 插件驱动设备树，状态轮询
- **NewProjectDialog** (189行): 项目创建对话框，路径验证
- **ConfigManager** (247行): QSettings INI存储，模板get/set，JSON导入导出
- **ProjectManager** (243行): .etproj JSON格式，目录结构，最近项目
- **PluginManager** (256行): QPluginLoader动态加载，元数据解析，依赖检查
- **Logger** (199行): spdlog异步，多sink（控制台/文件/QtUI），运行时级别切换
- **所有工具类**: FileUtil, StringUtil, ByteUtil, TimeUtil — 功能完整

### 仅存根的模块
- **ProblemsPanel** (28行): 空QTableWidget，4列定义，无数据填充逻辑
- **TerminalPanel** (31行): 仅"打开系统终端"按钮，启动外部cmd.exe

### 仅接口定义的插件
- IDevicePlugin, IADevicePlugin, IDADevicePlugin, ISerialDevicePlugin, IArinc429Plugin, ICANPlugin
- examples/plugins/下有mock实现可参考

## 关键技术约束

### QADS样式覆盖机制
- QADS在CDockManager构造函数中加载default.css，使用palette(window)颜色
- Qt样式级联：widget自身stylesheet优先于祖先 → MainWindow的QSS规则被覆盖
- 解决方案：将自定义dark样式追加到`dock_manager_->styleSheet()`
- 对应文件：`src/app/resources/styles/ads_dark.qss`

### QSS在QADS dock内不可靠
- ActivityBarWidget、SidebarWidget、PanelContainerWidget的QSS class选择器在QADS dock内不生效
- 解决方案：`setAutoFillBackground(true)` + `QPalette` 在C++代码中强制设置背景色

### QADS toggleView()重建标题栏
- `toggleView()`和`restoreState()`会重建dock area标题栏
- 需要在toggle/restore后重新隐藏标题栏或特定按钮

### QADS标题栏按钮结构
- CDockAreaTitleBar包含: CDockAreaTabBar(标签) → CSpacerWidget → 3个CTitleBarButton
- 三个按钮objectName: "tabsMenuButton", "detachGroupButton", "dockAreaCloseButton"
- 可通过`findChild<QToolButton*>(objectName)`单独隐藏

## ETest参考功能（来自docs/02-研究）

### 核心功能模块
1. **设备管理**: 串口/TCP/UDP/CAN/1553B/429/AI/AO/DI/DO
2. **ICD协议**: 可视化编辑、导入导出、打包解包
3. **测试用例**: 表格编辑、Lua/Python脚本、参数化
4. **测试执行**: 实时监控、断点调试、报告生成
5. **RUI可视化界面**: 基于Web的测试监控面板
6. **测试用例生成器**: 因果图/组合对/模糊测试/测试流程模型
