# Ideas / 想法清单

记录开发过程中发现的待办想法、技术债、跨平台坊点、参考材料。
合并自原 `my_ideas.md` 与 `ideas.md`（2026-07-27 合并）。

---

## 一、待实现功能想法

- [ ] **评估集成 QtNodes（paceholder/nodeeditor）**
  用途：测试流程可视化编辑（阶段4-5）、ICD 信号映射可视化
  注意：不能替代现有拓扑编辑器（范式不同）
  -> https://github.com/paceholder/nodeeditor
  参考案例：BehaviorTree/Groot 就是用 QtNodes 搭建领域特定节点编辑器的实际例子
  -> https://github.com/BehaviorTree/Groot
  评估时机：做到阶段 4（用例管理层）时再决定是否集成

- [ ] **拓扑编辑器 - 连线增强**
  手动调整连线路径/控制点、连线样式（颜色/粗细）、从端口拖出创建连线

- [ ] **拓扑编辑器 - 连接验证**
  方向不匹配等合法性检查，连线时实时提示

- [ ] **拓扑编辑器 - 自动布局**
  一键整理拓扑图布局算法

- [ ] **拓扑编辑器 - 设备模板**
  保存/加载/管理设备模板（当前只有另存为，没有加载入口）

- [ ] **拓扑编辑器 - 网格吸附**
  元素拖动时吸附到网格对齐

- [ ] **帧协议编辑器改进思路（参考 Protocol Designer）**
  参考：https://github.com/filipskrabak/protocol-designer
  可以借鉴的点：
  - 位图换行算法：bitsPerRow × pixelsPerBit 控制行宽，字段超出自动换行，变长字段支持填充/截断
  - Hover 联动高亮：位图悬停时高亮同字段所有跨行分块，同时联动树节点
  - 右键快捷操作：位图上直接右键 Edit/AddBefore/AddAfter/Delete
  - 分组着色：字段按功能分组（header/payload/checksum），自动分配底色
  - 双视图模式：位图可视化 + 列表拖拽排序并存

- [ ] **引入 Step Result 路由机制**
  参考 TestStand 的 Pass/Fail/Error 跳转，在 JSON 用例中增加 `onPass`/`onFail`/`onError` 字段
  -> [NI 分析报告](docs/02-研究/NI_VeriStand_TestStand_分析与对比.md) 7.1 节

- [ ] **引入 Process Model 概念**
  参考 TestStand 的流程模板，在用例管理层增加顺序/循环等执行模式模板
  -> [NI 分析报告](docs/02-研究/NI_VeriStand_TestStand_分析与对比.md) 7.3 节

- [ ] **引入 Callback 钩子机制**
  PreStep/PostStep/onSetup/onCleanup 生命周期管理
  -> [NI 分析报告](docs/02-研究/NI_VeriStand_TestStand_分析与对比.md) 7.3 节

---

## 二、技术债与风险记录

> 每条注明发现日期、来源、现状与建议，便于后续按优先级安排。

### 安装包单实例 mutex 与架构后缀

- **发现日期**: 2026-07-02
- **来源**: make_install_package.iss 双架构改造（commit 56d928a 后续）
- **现状**:
  - `src/app/make_install_package.iss` 中 `AppMutex=ETestStudioAppMutex` 和 `CheckForMutexes('{#MyAppMutex}')` 是**死代码**。
  - app 实际单实例机制是 `src/core/common/SingleInstance.cpp` 里的 `QLocalServer` + `QSharedMemory`（key 为 `"ETestStudio"`），从不创建名为 `ETestStudioAppMutex` 的 Windows mutex。
  - 因此安装时的 mutex 检测永远不触发，`AppMutex` 指令也永不生效。
- **风险**:
  - 若以后 app 改用 Windows mutex 做单实例（或补充 mutex 检测），且 mutex 名固定为 `ETestStudioAppMutex`，则 x86 和 x64 版本会互相误判对方在运行--本架构包安装时把另一架构的运行实例当成自己，提示关闭。
- **建议**:
  - 现状可保留死代码不动（不影响功能）。
  - 若以后引入 mutex：让 mutex 名带架构后缀，例如 `ETestStudioAppMutex-x64` / `ETestStudioAppMutex-x86`。.iss 里通过已有的 `MyAppArch` 宏拼接：`#define MyAppMutex "ETestStudioAppMutex-" + MyAppArch`，app 侧 `CreateMutex` 时用同名。
  - 同步评估 `QLocalServer`/`QSharedMemory` 的 key 是否也需要架构后缀--目前 x86/x64 共用 `"ETestStudio"`，意味着两个架构版本当前就会互相当成单实例（同一台机器先开 x64 再开 x86，x86 会被 x64 拦下）。这是另一个待评估的问题，见下条。

### SignalRegistry 四本内部索引的一致性风险

- **发现日期**: 2026-07-08
- **来源**: UUID V1.1 实现代码审查
- **现状**:
  - `SignalRegistry` 内部维护四个索引（`uuid_index_`、`port_to_frames_`、`device_names_`、`node_to_uuids_`），mutations 散布在 6 个方法中，无显式不变式约束。
  - 代码审查已发现 `unbindPort` 错误删除整个 `node_to_uuids_` key 的 bug（多设备共享同一节点路径时其他设备的索引被连带清除）。
- **风险**: 后续新增 `removeDevice`、增量绑定更新等方法时，大概率又会引入类似的不一致。
- **建议**:
  - 考虑将四个索引的写入收敛到少数内部方法，每个方法执行前后用 `assert`/`checkInvariants()` 校验一致性（如 `node_to_uuids_` 中每个 UUID 必在 `uuid_index_` 中存在）。
  - 后续若出现第三个索引不一致的 bug，则重构为单一数据源 + 派生视图的模式。

### SignalSelectionDialog 存储裸 Node* 指针

- **发现日期**: 2026-07-08
- **来源**: UUID V1.1 实现代码审查
- **现状**:【2026-07-27 核实仍存在】`src/app/dialogs/SignalSelectionDialog.cpp:194/206/262` 仍用 `reinterpret_cast<quintptr>(node)` 存 Node*。
  - 方案 3.7.3 明确反对此做法："Node* 的生命周期由 Repository 管理，Repository 重建后裸指针立即 dangling"，但当前 dialog 是 modal `exec()` 同步使用，生命周期安全。
- **风险**: 若未来将 dialog 改为非模态、缓存选中结果、或跨异步操作复用，裸指针将成为段错误的定时炸弹。
- **建议**:
  - 保持现状（modal 短生命周期没问题）。
  - 若以后改为非模态，需移除裸指针存储，改为存 nodePath(QString)，用时通过 `findNodeByPath` 定位。

### IcdSignalSelection / synchronizeRegistry 未经运行验证

- **发现日期**: 2026-07-08
- **来源**: UUID V1.1 实现代码审查
- **现状**:【待核实】原记录称 `MainWindow` 的 `icd_repository_` 始终为 null。2026-07-27 核实发现 `EditorManager.h:66` 已有 `icdRepository`，`MainWindow.cpp` 与 `SignalSyncHelper.cpp` 均调用 `synchronizeRegistry`，**很可能已接入运行时，但需亲自运行验证以下场景**：
  1. 打开含 ICD 协议的项目 -> `synchronizeRegistry` 正确建立索引
  2. 信号选择对话框显示实际 ICD 帧树
  3. 拓扑端口绑帧后 registry 同步更新
- **风险**: 这些代码在实际 ICD 数据下的行为未经验证，未来接入时可能发现隐藏问题。
- **建议**:
  - 接入 ICD Repository 后，按上述三个场景手动验收。
  - 补充集成测试覆盖完整链路。

### x86 / x64 版本互相视为同一实例（QLocalServer/SharedMemory key 共用）

- **发现日期**: 2026-07-02
- **来源**: 同上
- **现状**: `SingleInstance("ETestStudio")` 在 x86 和 x64 构建里用同一个 key。
  - `QLocalServer` 监听名 = `"ETestStudio"`
  - `QSharedMemory` key = `"ETestStudio"`
- **影响**: 同一台机器同时装了 x86 和 x64 两个版本时，先启动的版本会拦下后启动的另一架构版本（被当成"已有实例运行"并转发参数）。对于"共存"目标（见 make_install_package.iss 双架构改造）是矛盾的。
- **建议**:
  - 若希望 x86/x64 真正独立共存，`SingleInstance` 的 key 应带架构标识，例如 `"ETestStudio-x64"` / `"ETestStudio-x86"`。可在编译期通过宏（如 `CMAKE_SIZEOF_VOID_P` -> `-DAPP_ARCH_SUFFIX`）注入，或运行期检测 `sizeof(void*)`。
  - 若不希望共存运行（只允许装一个），则当前行为符合预期，可忽略本条；但需与安装包的"共存"策略对齐--目前 .iss 已改为共存（不同 AppId），与单实例 key 共用存在语义冲突，需明确取舍。

### 测试执行引擎 Phase 1 裁剪的调试功能（后续可加）

- **发现日期**: 2026-07-08
- **来源**: 硬件接入与测试执行引擎设计头脑风暴
- **现状**:【待核实】原记录称 Phase 1 只做核心功能。**当前状态需核实**--ExecutionDebugWidget 已落地（见 `docs/plan/测试执行功能重新设计决策记录.md`），但以下裁剪项是否已实现需确认：
  1. **信号值监视**（对标 lua-debugger-demo 的 Variable Watch）-- 执行过程中实时显示指定信号的当前工程值
  2. **步骤级端点**（对标 Breakpoints）-- 执行到标记的步骤时自动暂停，便于观察当前硬件状态
  3. **Step Into/Over/Out** -- 不适用测试执行场景，不需要
  - lua-debugger-demo 中 `std::thread` + `QWaitCondition` 的暂停/继续机制设计成熟，Phase 1 的 StepRunner 线程控制应参考其实现
- **风险**: 若用户后续有调试测试程序的需求，缺乏这些功能可能影响调试效率
- **建议**:
  - Phase 1 完成后，根据实际使用反馈再决定是否追加
  - 如果走 Lua 执行器路线，lua-debugger-demo 的 LuaDebugger 类几乎可直接复用（含 breakpoints + 变量 inspect + 调用栈）

### ~~StepValidation 校验器与 StepRunner 引擎命令列表不一致~~（已修复 CHECK）

- **发现日期**: 2026-07-27
- **来源**: 编写「命令演示」测试程序时发现
- **状态**: CHECK 已补入 `validCommands` 并与 VERIFY 合并校验分支（2026-07-27 修复）
- **剩余问题**: ACTION / LOG / INJECT_FAULT / CLEAR_FAULT / PHOTO / RECORD 这 6 个命令在校验器合法但引擎会报 ERROR，待后续决策是补引擎实现还是从校验器删除

### ~~测试程序编辑器的布局存在一些小问题~~（已修复）

**状态**：2026-07-27 修复

**原因**：QADS 的 `CDockWidget::setWidget()` 对非 `QAbstractScrollArea` 子类的 widget 会自动用 `QScrollArea` 包裹。`TestProgramEditorWidget` 继承 `QMainWindow`（不是 `QAbstractScrollArea`），导致整个 QMainWindow（含 toolbar + dockWidgets）被塞进 QScrollArea，toolbar 随内容一起滚动。

**修复**：`EditorManager.cpp` 两处 `dock->setWidget()` 调用加 `ads::CDockWidget::ForceNoScrollArea` 参数，跳过自动包裹。

---

## 三、待讨论的开放问题

> 还在酝酿中的想法，未到具体方案阶段，先记下来。

- **Mock 所有的插件**（已出方案，见 `docs/plan/Mock模式推断与配置方案.md`）
  原想法：mock 模式需要一个全局开关来开启，提供界面入口，放在执行页面，在开始执行之前，弄一个 checkbox，勾选上就走 Mock，否则就真实硬件，不靠拓扑或测试程序中编写字段。
  讨论后结论：
  - 「不靠拓扑中编写 mock 字段」采纳 -- 移除 `mock` 字段
  - 「全局 checkbox 切换模式」放弃 -- 模式由拓扑选用的插件类型（pluginId -> is_mock）自动决定，设备在拓扑加载时实例化，运行时无法切换
  - 「UI 配置 Mock 交互数据」保留 -- 新增 MockConfigDialog，入口在拓扑编辑器
  - 业务流程梳理见 `docs/01-规划/测试执行业务流程.md`

- **测试报告还未实现**
  状态记录，待后续设计

- **UUID 四元组似乎不是很好啊**
  存疑，需要重新审视 UUID 四元组设计是否最优

- **ICD 协议规范还未弄好啊**
  状态记录，icd 遗留问题 V1.0 中有 5 项未修复

- **core 完全不依赖 Qt**
  想让 core 完全不依赖 Qt，能做到吗？或者说我们先来讨论清楚 core 的核心职责有哪些？
  -> 见 `docs/plan/Qt剥离计划Phase1.md`（设计阶段）

- **engine 放入 core**
  想把 engine 放入 core 行不行？

- **监听器要不要专门做一个编辑器？**
  有的使用场景下，用户的拓扑和协议其实很简单，它主要是想要控制数据的收发，对于发送来说，他编写测试程序的时候可能就已经固化下来了参数，对于接收，他希望用监听器的界面来呈现数据并显示，然后他不需要整个IDE，他只需要一个独立运行程序，主要显示监听器界面

---

## 四、跨平台坑点记录

### 函数局部静态单例 + QSqlDatabase 的析构顺序问题

**现象**：`WisdomDatabase::instance()` 使用函数局部 `static WisdomDatabase db`，程序退出时析构函数访问 `QSqlDatabase::contains()` 导致 SIGSEGV。
**仅在 GCC（Linux）上复现**，MSVC（Windows）上安全。

**原因**：
- 函数局部静态变量在首次调用时构造，在程序退出时以**构造的逆序**析构（§[basic.start.term]）
- QSqlDatabase 内部维护一个全局连接注册表（也是静态存储期），其析构时机由 Qt 内部决定
- GCC 和 MSVC 对**不同编译单元**（Qt 库 vs 用户代码）中静态变量的析构顺序有不同的实现策略
- GCC 倾向于先析构用户代码中的静态对象，此时 QSqlDatabase 的注册表可能已被析构 -> `QSqlDatabase::contains()` 访问已销毁内存 -> SIGSEGV
- MSVC 的析构顺序恰好相反，因此从未暴露

**教训**：**凡是持有 QSqlDatabase 连接的单例，不应依赖析构函数来清理连接。** 因为无法保证你的析构函数在 QSqlDatabase 内部静态数据销毁之前执行。

**修复方案**：改为堆分配单例，永不析构。这与 `QCoreApplication` 的典型模式一致--让 Qt 在 `QCoreApplication` 析构时自行清理 SQL 连接注册表。

```cpp
WisdomDatabase& WisdomDatabase::instance() {
  static WisdomDatabase* db = new WisdomDatabase();
  return *db;
}
```

**适用场景**：任何在全局/静态生命周期中使用 Qt 资源（`QSqlDatabase`、`QNetworkAccessManager`、`QThread`、`QTimer` 等）的单例或静态对象。

**一般性建议**：
1. 函数局部静态变量适用于**纯数据**或**不依赖 Qt 内部静态数据的对象**
2. 如果对象依赖 Qt 类（特别是涉及全局注册表的类如 `QSqlDatabase`），优先考虑堆分配（`new` + 永不 `delete`）
3. 或者将初始化/清理托管给 `QCoreApplication` 的生命周期（如 `aboutToQuit` 信号中手动清理）
4. 跨平台代码**必须**在 GCC 和 MSVC 下都测试退出路径--析构顺序是未定义行为，标准不保证一致性

### 遮罩类浮层窗口定位：Wayland 无全局坐标，必须退化为父窗口子覆盖层

**现象**：`AnimationDialog`（登录/关于/新建项目/用户管理对话框的遮罩）在 Windows 正常覆盖主窗口，WSL2 Ubuntu 下遮罩落在屏幕左上角。
同项目的 `TuxSaverOverlay`（屏保）在 WSL2 下却完全正常。

**原因**：
- Wayland 协议下，客户端**无法查询自身在屏幕上的全局坐标**，窗口位置由 compositor 管理。`QWidget::geometry()` / `position()` 的位置部分只能返回客户端"认为"的值，通常为 `(0,0)`。
- `AnimationDialog` 原来是独立顶层 QDialog + `setGeometry(parentWidget()->window()->geometry())`，全局坐标在 Wayland 下拿到的是 `(0,0)`，遮罩就钉在左上角。
- `TuxSaverOverlay` 正常，是因为它是**普通 QWidget 子覆盖层**，用 `setGeometry(p->rect())`（父窗口本地坐标）定位，从不碰全局坐标——Wayland 下 child widget 本地定位 100% 可靠，且随父窗口移动/缩放自动跟随。

**教训**：**浮层/遮罩窗口的定位不要依赖"顶层窗口 + 全局坐标"，这是 Windows 思维。** Wayland 下独立顶层窗口无法被精确覆盖到另一窗口之上（协议级限制）。可靠路径是让遮罩退化为父窗口的 child widget，用父窗口本地坐标（`parentWidget()->rect()`）定位。

**修复方案**（commit b3c458a）：
- Linux 分支构造时清掉窗口类型位（`setWindowFlags((windowFlags() & ~Qt::WindowType_Mask) | Qt::Widget)`），退化为子覆盖层
- `showEvent` 双平台：Windows 用 `parentWidget()->window()->geometry()`（全局坐标可用），Linux 用 `parentWidget()->rect()`（本地坐标）
- 同步移除不再需要的 `WindowMover` 拖拽机制
- 注意事项：child QDialog 上 `exec()` 的模态行为（WindowModal 是否完整禁父窗口）未实测；`LoginDialog` 用 `show()` 不受影响，若 exec() 失效需改调用点为 `show()` + 完成信号

### Linux Debug 比 Windows Debug 快--Debug CRT 与 Qt 编译模式不对称

**现象**：WSL2 Ubuntu 上 `-g` 编译的 Debug 程序比 Windows 本机 Debug 程序流畅很多。
UI 响应、文件操作均有明显差距，实际逻辑相同。

**原因**：
- Windows Debug 模式下，MSVC 的 `/MDd`（Debug CRT）自动启用堆验证（填充 `0xDD`/
  `0xFD`/`0xCC`）、每次 malloc/free 做完整边界检查、STL checked iterators 跑全量
  范围校验。这些是**运行时开销**，与是否启动 VS 调试器无关
- Qt 官方 Windows 发布包包含 `Qt5Widgetsd.dll`（Debug 版），全链路走 Debug 路径；
  Linux 上 `apt` 装的 Qt 是 Release 版（`libqt5widgets.so`），即使 CMake 传
  `-DCMAKE_BUILD_TYPE=Debug` 也只影响你自己的代码
- GCC 的 `-g` 只生成 DWARF 调试符号（按需加载，零运行时开销），不开任何额外的
  运行时检查

**教训**：
1. 跨平台对比性能时，必须意识到底层运行时库的差异--Windows Debug CRT 的开销不是
   "调试符号"问题，而是**代码路径根本不同**
2. 如果要在 Windows 上获得接近 Linux Debug 的响应速度，要么用 Release 编译，要么
   用 `/MD` 替代 `/MDd`（但会失去堆检查和 checked iterators）
3. `set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")` + `/MD`（不加 `d`）可以
   生成带调试信息的 Release 运行时模式，适合需要调试符号但又不想牺牲性能的场景
4. 这个差异不影响逻辑正确性，只影响交互体验--适合在需要高频 UI 调试（布局、动画
   调参）时临时切到 Linux 环境

---

## 五、开发模型选型参考

> 下次开新功能时，先对照下表判断适合哪种开发模型，再决定是否 TDD。

| 模型          | 核心                                          | 适合场景                   |
| ------------- | --------------------------------------------- | -------------------------- |
| **TDD**       | 红-绿-重构，测试先行                          | 纯逻辑、可自动化验证       |
| **BDD**       | 用自然语言描述行为（Given/When/Then）驱动测试 | 业务行为清晰、需与产品对齐 |
| **DDD**       | 领域模型驱动，先建领域语言与模型              | 业务复杂、领域知识密集     |
| **FDD**       | 按特性切分，每个特性短周期交付                | 特性相对独立               |
| **瀑布**      | 需求->设计->编码->测试->部署，顺序流转        | 需求稳定、一次做对         |
| **V 模型**    | 瀑布的变体，每阶段对应一个测试层              | 嵌入式/强验证要求          |
| **增量**      | 大设计拆模块，逐模块交付并集成                | 架构清晰、模块边界明确     |
| **迭代/敏捷** | 小周期循环，每轮加一点可工作软件              | 需求演进、探索性           |
| **螺旋**      | 每轮带风险分析，逐步细化                      | 高风险、需求模糊           |
| **原型**      | 先做可抛弃原型探路，再正式实现                | 需求不明、技术验证         |
| **XP**        | TDD+结对+持续集成+简单设计                    | 小团队、变化快             |
| **RAD**       | 快速搭+可视化工具，压缩周期                   | 内部工具、UI 为主          |

### 选型原则

- **GUI 项目不强套 TDD**：视觉、布局、滚动、防抖这些靠人眼+日志验证更直接，单元测试覆盖意义有限。本项目 CLAUDE.md 已点明"Agent 无法看运行状态，唯一手段是加日志"--正是增量+手动验证的语境。
- **纯逻辑层可摘出来 TDD**：如过滤器 `matches()`、判断函数（输入输出明确、边界清晰）这类纯函数，先写测试能省掉"加日志-编译-跑-看"的循环。
- **本项目实际在用的是增量式**：设计文档驱动 + 分阶段实施 + 每步 subagent 审查 + 手动验证，保持即可。
- **混合最实际**：纯逻辑 TDD + UI 层增量推进，别为了方法论而方法论。

---

## 六、参考资源

### 谷歌 2026 新图标风格提示词

- 谷歌2026风格图标，极简几何图形，流畅的彩色渐变（蓝红黄绿），柔和投影，毛玻璃质感，圆角，干净白底，等轴测3D倾斜，现代扁平设计，高清矢量。图标无背景。
- Google 2026 style icon, minimalist geometric shape, smooth colorful gradient (blue, red, yellow, green), soft shadow, frosted glass texture, rounded corners, clean white background, 3D isometric tilt, modern flat design, high quality, 8k, vector illustration.

---

## 七、已废弃想法

- [x] **评估 Taskflow（已评估，不适用）**
  用途评估：测试执行引擎的并行调度
  结论：不适用。引擎是顺序执行 + 控制流 + 交互式调试模型，与 Taskflow 的 DAG 并行范式不匹配。
  保留价值：后期多个独立用例并行执行时可能参考，但 MVP 不需要。
  -> https://github.com/taskflow/taskflow

- [x] **信号别名机制**（2026-07 放弃）
  原想法：在 ICD 帧上绑定多个别名，让用户在测试程序中 `verify 温度 50℃` 而非 `verify 电压采集 50?`
  放弃原因：改动太大，与 UUID 设计初衷不符。UUID 的核心是确定性派生，别名引入用户自定义映射层会破坏这一前提。
  详见桌面文档 `信号别名设计.md`（未入库）

---

## 八、已完成（历史索引）

- [x] **全局 IconProvider + ThemeManager**
  统一图标加载和主题管理，带信号机制和缓存
  落地：`src/core_ui/AppIconProvider.cpp` + `src/core_ui/ThemeManager.cpp`
  -> [设计文档](docs/01-规划/全局IconProvider和ThemeManager设计.md)

- [x] **CentralWidget -> QStackedWidget 改造**
  主窗口 centralWidget 替换为 QStackedWidget，编辑态/运行态双页面
  -> [设计文档](docs/plan/中央编辑器区域QStackedWidget改造方案.md)

- [x] **NI VeriStand + TestStand 拆解分析**
  完成 NI 两大产品的模块拆解和与 IATP 的逐层对比报告
  -> [分析报告](docs/02-研究/NI_VeriStand_TestStand_分析与对比.md)

- [x] **UI 布局重构**
  全 QADS -> QSplitter 混合布局，活动栏/侧边栏/底部面板独立为普通 QWidget
  -> [设计文档](docs/01-规划/UI布局重构方案.md)
  (commit 3b526e1)

- [x] **SARibbon 主窗口改造**
  QMainWindow -> SARibbonMainWindow，Ribbon 功能区替代传统菜单栏/工具栏
  (commit 7e15c75)

- [x] **core -> etest_core 重命名**
  统一 CMake 目标命名规范，涉及 12 个 CMakeLists.txt
  (commit 466f6ce)

- [x] **topology-demo 浅色主题 + SVG 图标修复**
  添加 resource.qrc、AUTORCC、setDarkTheme(false)
  (commit 7e15c75)
