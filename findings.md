# IATP — 研究发现

## Lua + sol2 技术调研（2026-05-11）

### sol2绑定Qt类型
- **QString不原生支持**：sol2不直接识别QString，需要通过`.toStdString()`和`QString::fromStdString()`转换。在Lua侧统一使用Lua string，C++端做转换
- **QVariant不原生支持**：sol2不直接识别QVariant。建议在Lua侧使用table，C++端做QVariant ↔ sol::table的双向转换
- **自定义Struct绑定**：通过`new_usertype`绑定，对QString字段需要用`sol::property`包装getter/setter做std::string转换，对QVariant字段同理转换为基础类型
- **枚举绑定**：两种方案——(1)全局常量`DeviceStatus_Online`(简单但Lua不友好) (2)table方式`DeviceStatus.Online`(推荐，符合Lua习惯)
- **safe_script**：可以捕获运行时错误和语法错误，错误后VM仍然可用。`sol::protected_function`也可单独包装某个函数做保护调用

### Lua Debug Library
- **lua_sethook + sol2**：兼容。通过`lua.lua_state()`获取底层`lua_State*`后设置hook，sol2 API正常工作
- **LUA_MASKLINE**：每行Lua代码触发一次，C++注册的函数内部不触发（source为"C"）
- **断点实现**：在hook回调中检查`ar->currentline`是否在断点集合中，命中则记录/暂停
- **变量监视**：`lua_getlocal(L, ar, index)`可读取当前函数的所有局部变量，返回变量名和值。以`(`开头的name是内部临时变量，跳过
- **调用栈**：`lua_getstack(L, level, &stackEntry)` + `lua_getinfo(L, "nSl", &stackEntry)`逐层获取调用信息
- **暂停机制**：在hook中不能直接暂停（会阻塞），需要配合协程的`lua_yield`或在协程中手动`coroutine.yield()`
- **条件断点**：在hook中通过`lua_getlocal`读取条件变量值，与阈值比较后决定是否命中
- **hook上下文传递**：C API的`lua_sethook`不支持upvalue，需要通过全局lightuserdata传递调试上下文指针

### VM隔离
- **多个sol::state实例**：完全隔离，各自的全局变量、注册函数、标准库打开状态互不影响
- **沙箱化**：通过只打开`base/math/string/table/coroutine`库，不打开`io/os/debug`，防止脚本执行危险操作
- **销毁重建**：sol::state析构后所有状态清除，新建的VM是完全干净的环境
- **IATP应用**：每个测试用例在独立VM中执行，用例间状态完全隔离

### 协程执行控制
- **coroutine.yield/resume**：C++端通过`sol::coroutine`的`operator()`驱动resume，Lua端用`coroutine.yield()`暂停
- **逐步骤执行**：每个测试步骤后yield，C++端逐步resume，实现暂停/恢复/单步
- **resume传值**：`co(value)`传值到协程内部，作为`coroutine.yield()`的返回值，可用于从C++端向脚本传递信号值
- **协程错误处理**：协程内`error()`不会导致C++崩溃，`result.valid()`为false，协程状态变为dead
- **协程内hook**：Lua 5.4中子协程继承主线程的hook设置，`lua_sethook`在协程内也生效
- **lua_yield在hook中**：理论上可以在hook回调中调用`lua_yield`暂停协程，但必须确保当前在协程上下文中，否则会crash。IATP推荐方案：**在协程中执行脚本，每步yield，C++端控制节奏**

### IATP引擎层实现策略（基于调研结论）
1. 每个用例创建独立的`sol::state`，只打开安全库
2. 用例脚本在`sol::coroutine`中执行，每个步骤后yield
3. C++端逐步骤resume，实现暂停/恢复/终止
4. 用`lua_sethook(LUA_MASKLINE)`实现断点和变量监视
5. 用`lua_getlocal`在断点处读取变量值
6. 用`safe_script`或`protected_function`确保错误不崩溃
7. 枚举用table方式绑定（`DeviceStatus.Online`），Struct用`new_usertype`
8. QString/QVariant在C++/Lua边界做转换，Lua侧只用基础类型

---

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
