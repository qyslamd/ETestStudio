# ETest Demo — 研究发现

## 代码架构现状

### 已完整实现的模块
- **MainWindow** (1100+行): QADS dock布局、菜单栏/工具栏/状态栏、项目生命周期、编辑器操作、窗口状态持久化、Ctrl+Shift+F全局搜索快捷键
- **ActivityBarWidget** (110行): 7个SVG图标按钮（资源管理器/搜索/Git/调试/扩展/硬件/设置），切换侧边栏页面，设置按钮独立信号
- **SidebarWidget** (120行): 6个StackedPage，页面切换，SearchWidget替代占位符
- **FileExplorerWidget** (251行): QFileSystemModel文件树，右键菜单，双击打开，**FileTypeIconProvider注入**
- **EditorManager** (500+行): 多tab管理，脏文件检查，关闭操作，右键菜单，**openFileAtLine()**
- **EditorWidget** (371行): QScintilla集成，多语言Lexer（C++/Lua/JSON/XML/Python/YAML/MD/CMake/JS），VSCode Dark+配色
- **SearchWidget** (160行): 项目范围文本搜索，按文件分组结果树，点击跳转行，大小写开关，1000结果上限
- **SettingsWidget** (QDialog): 独立设置对话框，左侧分类树+右侧表单，ConfigManager双向绑定
- **FileTypeIconProvider**: QFileIconProvider子类，11类Seti风格彩色图标（C++/Lua/JSON/XML/Python/YAML/MD/CMake/JS/文件夹/通用）
- **OutputPanel** (78行): 只读QTextEdit，彩色日志级别，自动滚动，5000行限制
- **TerminalPanel** (800+行): 嵌入式PTY终端，VT100解析，scrollback缓冲(10000行)，Shell退出重启
- **PanelContainerWidget** (103行): QTabWidget容器，最大化/还原/关闭按钮
- **HardwareTreeWidget** (211行): 插件驱动设备树，状态轮询
- **ProblemsPanel** (28行): 空QTableWidget，4列定义，无数据填充逻辑
- **NewProjectDialog** (189行): 项目创建对话框，路径验证
- **ConfigManager** (247行): QSettings INI存储，模板get/set，JSON导入导出
- **ProjectManager** (243行): .etproj JSON格式，目录结构，最近项目
- **PluginManager** (256行): QPluginLoader动态加载，元数据解析，依赖检查
- **Logger** (199行): spdlog异步，多sink（控制台/文件/QtUI），运行时级别切换
- **所有工具类**: FileUtil, StringUtil, ByteUtil, TimeUtil — 功能完整

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

### QFileIconProvider注意事项
- QFileIconProvider不是QObject子类，不能传parent参数
- QFileSystemModel内部缓存图标，setIconProvider()必须在setRootPath()之前调用
- SVG图标用QIcon(path)直接构造比addFile()更可靠
- addFile()的QSize()参数在某些Qt版本下可能导致空图标

### TerminalPanel架构
- PtyProcess封装Windows ConPTY API
- VtParser解析VT100转义序列
- screen_为二维Line数组，scrollback_保存滚出顶部的行
- flushToDisplay()合并scrollback+screen显示，检查用户滚动位置决定是否自动滚底
- setMaximumBlockCount()与手动scrollback管理冲突，必须移除

## ETest参考功能（来自docs/02-研究）

### 核心功能模块
1. **设备管理**: 串口/TCP/UDP/CAN/1553B/429/AI/AO/DI/DO
2. **ICD协议**: 可视化编辑、导入导出、打包解包
3. **测试用例**: 表格编辑、Lua/Python脚本、参数化
4. **测试执行**: 实时监控、断点调试、报告生成
5. **RUI可视化界面**: 基于Web的测试监控面板
6. **测试用例生成器**: 因果图/组合对/模糊测试/测试流程模型

## Windows崩溃处理机制

### SetUnhandledExceptionFilter vs AddVectoredExceptionHandler
- `SetUnhandledExceptionFilter` 只在异常链最后才被调用，gtest/CRT的SEH handler会先拦截
- `AddVectoredExceptionHandler(1, handler)` 注册在VEH链最前端，优先级最高
- 实际使用中两者都注册：VEH负责优先捕获，SetUnhandledExceptionFilter作为兜底
- VEH handler需过滤异常类型，只处理真正的崩溃（ACCESS_VIOLATION等），放行调试和控制流异常

### MiniDumpWriteDump要点
- 需要dbghelp.lib（已在CMake中链接）
- MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo 为推荐级别
- MiniDumpWithFullMemory会包含全部进程内存，文件可能数百MB
- 生成的.dmp配合PDB可在任意机器用WinDbg/VS调试

### QStandardPaths在Windows上的路径
| 枚举值 | 实际路径 |
|--------|----------|
| AppLocalDataLocation | C:/Users/<user>/AppData/Local/etest_demo/ |
| AppConfigLocation | C:/Users/<user>/AppData/Local/etest_demo/ |
| AppDataLocation | C:/Users/<user>/AppData/Roaming/etest_demo/ |
| DocumentsLocation | C:/Users/<user>/Documents/ |

### gtest崩溃测试
- gtest默认用SEH __try/__except包裹测试体，崩溃异常到不了SetUnhandledExceptionFilter
- `--gtest_catch_exceptions=0` 可禁用gtest的SEH拦截
- 即使加了此参数，gtest进程内的其他SEH handler仍可能拦截，VEH才能保证优先
- DISABLED_前缀测试不会自动运行，需`--gtest_also_run_disabled_tests`
