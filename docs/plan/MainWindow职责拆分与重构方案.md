# MainWindow 职责拆分与重构方案

> 目标：将 `MainWindow` 中过重的业务逻辑按功能域拆分到多个 Controller 类中，
> 让 MainWindow 退化为 Facade（外观），仅负责布局搭建和信号粘合。
> 遵循 SRP，不改现有行为，不加新功能。

---

## 一、现状问题

### 1.1 职责检视

`src/app/MainWindow.h` 当前承担 **至少 8 个独立领域** 的职责：

| 领域 | 方法数 | 成员变量数 | 评估 |
|------|--------|-----------|------|
| 窗口生命周期 & 布局 | 8 | ~10 | ✅ MainWindow 应保留 |
| **项目管理 UI 逻辑** | 9 | 4 | ❌ 可抽出 |
| **编辑器操作代理** | 9+3 | 5 | ❌ 可抽出 |
| **执行引擎控制** | 8 | 8 | ❌ 可抽出 |
| 状态栏管理 | 1 | 8 个 QLabel | ❌ 可抽出 |
| 面板可见性控制 | 7 个 QAction | 0 | ⚠️ 可简化 |
| 屏保 | 0 | 3 | ⚠️ 可抽出，价值较低 |
| 登录认证 | 0 | 3 | ⚠️ 可抽出，价值较低 |

**总成员变量：约 55 个（含 QAction \* 约 20 个）**
**总方法：约 50 个（含 private slot）**

### 1.2 违反 SRP 的具体表现

1. **项目逻辑和编辑器逻辑混合**：`onOpenFile()` 打开文件对话框后直接调用 `editor_manager_->openFile()`，但同时又管理最近文件列表、项目路径等——这些分属不同领域
2. **执行引擎在 MainWindow 中裸生存周期**：`engine_` 直接是 MainWindow 的成员，`createEngine()` / `destroyEngine()` 是 MainWindow 的私有方法——引擎的生命周期和 UI 控制应该封装在一起
3. **状态栏 8 个 QLabel 裸奔**：这些标签的更新分散在多个方法中，没有统一管理
4. **剪贴板**：`clipboard_` 成员只用于编辑操作中的 cut/copy/paste，属于编辑器子系统的内部细节

---

## 二、目标架构

### 2.1 设计原则

- **MainWindow 退化为 Facade**：只做布局搭建 + 信号粘合
- **每个 Controller 一个文件**：`QObject` 子类，通过信号/槽与 MainWindow 和其他模块通信
- **不改原有代码逻辑**：纯粹的方法搬移，不重构内部实现
- **保持向后兼容**：不改变任何公开接口签名

### 2.2 拆分后的类图

```
+-- MainWindow (Facade: ~51 members, ~12 business methods) ---------+
|                                                                   |
|  +-----------------+  +------------------+  +------------------+  |
|  | ProjectController|  | EditorPanelCtrl  |  | ExecPanelCtrl    |  |
|  |   (project UI)  |  |   (editor ops)   |  |   (engine ctrl)  |  |
|  +--------+--------+  +--------+---------+  +--------+---------+  |
|           |                    |                      |           |
|           +--------------------+----------------------+           |
|                                |                                  |
|  +-----------------------------v----------------------------+    |
|  |              AppStatusBarController                        |    |
|  |             (status bar management)                       |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  +----------------------------------------------------+    |
|  |    TuxSaverController                               |    |
|  |    (screensaver/idle detect)                        |    |
|  +----------------------------------------------------+    |
|                                                                   |
|  +----------------------------------------------------------+    |
|  |  initSignalsLate(): signal routing layer                   |    |
|  |  project_controller -> editor_controller                   |    |
|  |  exec_controller -> status_bar_ctrl                       |    |
|  +----------------------------------------------------------+    |
+------------------------------------------------------------------+
```

### 2.3 Facade 模式在本方案中的运用

#### 2.3.1 为什么用 Facade 而不是其他模式

面对"一个 God Class 拆成多个类"的重构，有几种常见策略。以下逐一对比，解释为何 Facade 为首选：

| 模式 | 做法 | 在本项目中的评价 | 为什么不采纳 |
|------|------|-----------------|-------------|
| **Facade ✅（本文采用）** | MainWindow 保留入口，逻辑委托给子系统 Controller | 外部通过 signal/槽耦合，Facade 可以零改动承接 | — |
| **ApplicationController / AppContext** | 一个统一控制器持有所有业务逻辑，MainWindow 只做视图 | 本质是在 MainWindow 之外再增加一个上帝类，只是把"胖 Window"变成"胖 Controller" | **将 God Class 从一个搬到另一个**，没有解决内聚性问题 |
| **SignalBridge / EventBus** | 用一个中介对象管理所有跨组件信号连接 | 增大了间接层，定位和调试信号流需要多跳转一次 | 引入不必要的间接性 → 详见下文分析 |
| **MVC / MVP** | 数据和视图完全分离 | MainWindow 现有非 Qt 业务逻辑少，不值得大动 | 成本高，收益低 |
| **Command** | 每个操作为一个对象 | undo/redo 已有 EditorManager 管理 | 不适用 |
| **Mediator** | 对象间通过中介通信 | 子系统交互以单向数据流为主（A→B→C），不需要中介仲裁 | 过度设计 |
| **QStateMachine** | 用状态机管理引擎/编辑器/项目状态 | 引擎状态机是 TestExecutionEngine 的内部细节，不应由 MainWindow 再包装一层 | 架空了引擎的状态管理 |
| **依赖注入容器** | 将依赖通过构造参数注入（signal_registry_、engine_ 等） | 值得局部采用，但不需要完整容器 → 见下文 | 有益但适用范围有限 |

##### 为什么不是 ApplicationController？

另一个 agent 建议"引入 ApplicationController，将所有业务逻辑迁移到独立的控制器中"。
这个方向的**问题**在于：

- 如果只做一个 `ApplicationController`，它本质上就是改了个名的 `MainWindow`——所有逻辑堆在一个类里，违反 SRP 的问题原封不动
- 如果拆成多个 `XxxController`，那就和本方案的 **5 个 Controller** 没有本质区别
- 把引擎、信号注册表、ICD 仓库、项目状态**全部塞进一个控制器**，反而破坏了本方案拆分出的 `ExecutionPanelController` 和 `ProjectController` 的边界

**结论**：本方案的 5 个 Controller 已经是对 ApplicationController 这一概念的**按领域拆分**。不需要再套一层。

##### 为什么不是 SignalBridge / EventBus？

同样建议提到"将 initSignalsLate() 中大量的跨组件连接集中到一个专门的管理类中"。

这个方向的**问题**在于：

- 在 Qt 中，`QObject::connect()` 本身就是信号/槽的"总线机制"
- 增加 `SignalBridge` 相当于在 Qt 的信号/槽之上再加一层路由——信号从 `A::signal` → `Bridge` → `Bridge::signal` → `B::slot`，多跳一次，调试时 `QObject::sender()` 追踪多一层
- 对于"项目打开 → 更新状态栏"这种单向简单数据流，多一层中介不增加可维护性，只增加间接性

**何时才需要 EventBus？**
- 当信号需要**动态路由**（运行时决定谁接收）
- 当信号需要**广播给 N 个接收者**（N 不固定，依赖订阅/取消订阅）
- 当源和目的跨越不共享 QObject* 指针的模块边界

本项目中的信号连接场景主要是 **1:1 固定连接**（Ribbon λ → Controller、Controller → StatusBar），Qt 内建的信号/槽已经足够。

**本方案的做法**：`initSignalsLate()` 仍然集中在一个方法里管理所有跨域连接，但没有增加额外中间层。这样定位信号时只需要看这一个函数，调试时 `QObject::sender()` 直接指向源对象。

##### 为什么不是 QStateMachine？

另一个建议提到"使用 QStateMachine 将引擎状态、编辑器状态、项目状态统一管理"。

本项目的引擎**已有自己的状态机**（`TestExecutionEngine::EngineState`），编辑器状态由 `EditorManager` 管理，项目状态由 `ProjectManager` 管理。MainWindow 的职责不是"管理这些状态"，而是"监听这些状态的变化并同步 UI"。

如果在 MainWindow 层再引入 `QStateMachine`：
- 需要在 MainWindow 中维护一个与引擎状态机**平行**的状态副本
- 引擎状态变化 → 发射信号 → MainWindow 的 QStateMachine 收到 → 转换到对应状态 → 更新 UI
- 比直接 `connect(engine_, &Engine::stateChanged, this, [this]() { syncControls(); })` 多了一整层冗余

**结论**：状态机在引擎内部（控制执行流程的转换图）是有价值的，但在 MainWindow 层（只是响应状态变化更新 UI 控件）是过度设计。

##### 关于依赖注入（局部采用）

另一个 agent 提到"将 signal_registry_、icd_repository_、engine_ 等作为构造参数传入，便于单元测试"。

**本方案的兼容度**：

| 依赖 | 当前方式 | 是否可以改为构造注入 | 影响 |
|------|---------|---------------------|------|
| `signal_registry_` | MainWindow 成员，由 onProjectOpened 初始化 | ✅ 可以传入 `ExecutionPanelController::postInit()` | 低 |
| `icd_repository_` | 同上 | ✅ 同上 | 低 |
| `engine_` | MainWindow 成员，createEngine 中创建 | ✅ 所有权已归 `ExecutionPanelController`（本方案已做） | 低 |
| `editor_manager_` | MainWindow 成员，lazyInit 中创建 | ✅ 已通过构造参数传入 EditorPanelController（本方案已做） | 低 |

本方案的 Controller 构造函数已经采用了**手动依赖注入**（Manual DI）：
- `EditorPanelController(EditorManager*, QClipboard*, AppStatusBarController*)` — 依赖通过参数传入
- `ExecutionPanelController::postInit(ExecutionDebugWidget*, ExecutionOutputPanel*, SignalRegistry*, Repository*)` — 依赖通过方法参数传入
- `ProjectController(QWidget*, EditorManager*)` — 依赖通过参数传入

不引入完整 DI 容器的原因是 C++ 的 DI 容器（如 Boost.DI）会增加构建复杂度和模板错误信息，而本项目的 Controller 数量少（5 个），依赖关系简单（1~4 个参数），手动注入已经足够清晰。

##### 关于进一步拆分 lazyInit()

另一个 agent 建议将 `lazyInit()` 拆分为多个私有方法或委托给初始化器类。

**本方案的做法**：lazyInit() 中与布局相关的部分（创建面板、连接信号）仍保留在 MainWindow 中（因为它们是 Facade 的搭建职责），但**与业务逻辑相关的部分已委托给 Controller 的构造函数**：

```cpp
void MainWindow::lazyInit() {
    // 保留：布局搭建（Facade 职责）
    createPanels();       // 拆分出：创建 sidebar 面板、底部面板
    restoreWindowState();
    loadPlugins();
    checkFirstRun();

    // 委托：业务逻辑初始化（Controller 职责）
    project_controller_ = new ProjectController(this, editor_manager_, this);
    editor_controller_ = new EditorPanelController(editor_manager_, clipboard_,
                                                    status_bar_ctrl_, this);
    execution_controller_->postInit(
        execution_debug_widget_, execution_output_panel_,
        signal_registry_, icd_repository_);

    // 粘合
    initSignalsLate();
}
```

`lazyInit()` 被拆分成了：
- `createPanels()` — 面板创建（原 step 1~5 中与布局相关的逻辑）
- `restoreWindowState()` — 窗口状态恢复（原已独立）
- `loadPlugins()` — 插件加载（原已独立，但不显式命名）
- `checkFirstRun()` — 首次运行处理（原已独立）

但这些是**重构过程中的可选项**，不是核心改动，根据实际代码量和复杂度决定是否拆分。

##### Facade 模式的局限性（必须承认）

| 局限性 | 在本方案中的体现 | 缓解措施 |
|--------|----------------|---------|
| **Facade 可能变成"上帝门面"** | MainWindow 仍保有 ~51 个成员 | 约 30 个 QAction 仅做"路由中转"不负责逻辑；真正的业务状态下沉到 5 个 Controller |
| **Facade 隐藏了子系统的复杂性** | 调用者只看到 MainWindow 的方法，不知道背后委托给了哪个 Controller | 通过 λ 连接的命名（`execution_controller_->run()`）显式暴露委托目标 |
| **Facade 和子系统耦合** | MainWindow 持有所有 Controller 的指针 | 这是 Qt 对象树的正常模式（parent-child），并非病态耦合 |
| **Facade 不适合动态行为** | 如果未来需要动态切换 Controller 实现，Facade 模式比较僵化 | 当前业务场景（硬件测试工具）没有运行时多态 Controller 的需求 |

**结论**：Facade 是"分步拆散上帝类"过程中性价比最高的起点。它承认"MainWindow 还是会比较胖"，但确保了"胖的部分只做路由，不做决策"。5 个 Controller 可以独立测试、独立修改，MainWindow 只是它们的"接线板"。

#### 2.3.2 何时用 Facade、何时走 facade→子系统 直接通信

```cpp
// ─── 场景 A：需要通过 Facade 中转 ───
// Ribbon QAction 的 triggered 直接绑定 λ，λ 调用 Controller 方法
// Facade 提供一条"命名路径"，让 setupRibbon 里的连接看着清晰

void MainWindow::setupRibbon() {
    // 以前：直接连接 MainWindow::onRunClicked
    // 现在：MainWindow 做中转，但逻辑在 ExecutionPanelController
    connect(act_run_, &QAction::triggered,
            this, [this]() { execution_controller_->run(); });
    //     ↑ Facade 中转       ↑ 实际逻辑在子系统
    // 好处：setupRibbon 里一眼能看出"点运行按钮 → 执行引擎"
}

// ─── 场景 B：不需要 Facade 介入 ───
// 两个 Controller 之间的数据流，直接信号/槽连接
// Facade 在 initSignalsLate 里只负责"拉线"

void MainWindow::initSignalsLate() {
    connect(execution_controller_, &ExecutionPanelController::execStatsUpdated,
            status_bar_ctrl_, &AppStatusBarController::setExecStats);
    //     ↑ Facade 不参与逻辑，只接线
}
```

**判断标准**：

| 通信方向 | 举例 | 走 Facade？ | 原因 |
|----------|------|-------------|------|
| **外部 → 子系统** | Ribbon 按钮 → run() | ✅ Facade 中转 | 外部事件需要一条"路由"进入子系统，Facade 的 λ 连接可读性高 |
| **外部 → 子系统** | MenuBar → openProject() | ✅ Facade 中转 | 同上 |
| **子系统 ↔ 子系统** | Engine stats → StatusBar | ❌ 直接信号/槽 | 两个 Controller 已在 MainWindow 中注册，直接 `connect(A, B)` 最简洁 |
| **子系统 → 外部** | 项目打开 → 更新窗口标题 | ✅ Facade 中转 | 需要操作 QWidget（窗口标题），Controller 不应该持有窗口指针以外的东西 |
| **子系统内部** | Engine 状态变化 → syncControlStates | ❌ 不经过 Facade | 纯内部逻辑，Controller 内自己连接 |

#### 2.3.3 Facade 的边界：MainWindow 保留什么、委托什么

```
                            MainWindow (Facade)
                            ┌─────────────────────────┐
                            │                         │
  Ribbon Action ──────────→ │  setupRibbon():          │
  triggered                 │   连接 action → λ       │
                            │   λ 调用 Controller      │
                            │                         │
  closeEvent ──────────────→│  saveWindowState():      │
  showEvent                 │   保留（窗口生命周期）    │
                            │                         │
  eventFilter ─────────────→│  保留（全局事件拦截）     │
                            │                         │
                            │  initSignalsLate():      │ ← "拉线中心"
                            │   A_controller → B_ctrl  │
                            │   B_ctrl → status_bar    │
                            │   C_ctrl → 窗口标题      │
                            └─────────────────────────┘
                                        │
              ┌─────────────────────────┼──────────────────────────┐
              │                         │                          │
              ▼                         ▼                          ▼
   ┌──────────────────┐   ┌──────────────────────┐   ┌───────────────────────┐
   │ ProjectController │   │ EditorPanelController │   │ ExecPanelController   │
   │                   │   │                       │   │                       │
   │ • newProject()    │   │ • saveCurrent()       │   │ • createEngine()      │
   │ • openProject()   │   │ • undo() / redo()     │   │ • run() / pause()     │
   │ • onXXX信号 ──────┼──→│ • find() / replace()  │   │ • stop() / verify()   │
   │   → EditorPanel   │   │                     │   │ • engineState信号 ────→│
   └──────────────────┘   └──────────────────────┘   │   → StatusBarMgr        │
                                                     └───────────────────────┘
```

此时 MainWindow 处于一个微妙的角色：
- **它是入口但不是实现** — 调用者（Ribbon、MenuBar）仍然只和 MainWindow 交互，但 MainWindow 马上把调用委托给子系统
- **它是拉线层但不是调度层** — `initSignalsLate()` 只连接 signal/槽，不参与任何决策逻辑
- **它是布局所有者但所有者** — `setupRibbon()`、splitter、dock_manager_ 这些控件仍归 MainWindow 管，因为它们直接关系窗口的物理外观

#### 2.3.4 对比重构前后的调用链差异

**重构前（God Class 直接调用）**：

```
Ribbon `act_run_──→ MainWindow::onRunClicked()
                      │
                      ├── execution_debug_widget_->refreshPreconditions()
                      ├── execution_debug_widget_->canRun()
                      ├── engine_->setProgram(...)
                      ├── engine_->start()
                      └── statusBar()->showMessage("运行中...")
                          ↑ 操作状态栏
```

**重构后（Facade 委托 + 信号驱动）**：

```
Ribbon `act_run_──→ MainWindow (λ 中转)
                      │
                      └── execution_controller_->run()
                            │
                            ├── debug_widget_->refreshPreconditions()
                            ├── debug_widget_->canRun()
                            ├── engine_->setProgram(...)
                            ├── engine_->start()
                            └── emit engineStateChanged(Running)
                                  │
                                  └── → status_bar_ctrl_->setEngineState("运行中...")
```

差异化点：
1. **MainWindow 不做任何业务判断**，只是一个 3 行的 λ 转发
2. **engine_ 不是 MainWindow 的成员**，它是 `ExecutionPanelController` 的内部细节
3. **状态栏不通过 MainWindow 的裸 QLabel 操作**，而是通过 signal → `AppStatusBarController` 更新

### 2.4 信号/槽数据流

```
Ribbon QAction::triggered
  │
  ├──→ ProjectController::openProject()
  │       ├──→ signal: fileRequested(path)
  │       │       └──→ EditorPanelController::openFile(path)
  │       └──→ signal: projectChanged(name)
  │               └──→ AppStatusBarController::setProject()
  │
  ├──→ EditorPanelController::saveCurrent()
  │       └──→ signal: editorStateChanged()
  │               └──→ AppStatusBarController::setCursorPos()
  │
  └──→ ExecutionPanelController::run()
          ├──→ TestExecutionEngine (内部)
          └──→ signal: execStatsUpdated(pass, fail, elapsed)
                  └──→ AppStatusBarController::setExecStats()
```

---

## 三、新类详细设计

### 3.1 ProjectController

**文件**：新增 `src/app/ProjectController.h` / `.cpp`

**职责**：管理项目相关的所有 UI 操作逻辑

```cpp
namespace etest::app {

class ProjectController : public QObject {
    Q_OBJECT
public:
    explicit ProjectController(QWidget* parentWidget,  // 用于对话框父窗口
                               EditorManager* editorMgr,
                               QObject* parent = nullptr);

    // 替代 MainWindow 的 onNewProject / onOpenProject / onOpenFile / onCloseProject
    void newProject();
    void openProject();
    void openFile();
    void closeProject();
    void openRecent(const QString& path);

    // 辅助方法
    void updateWindowTitle(QWidget* window);
    void updateRecentProjectsMenu(QMenu* menu);
    void updateRecentFilesMenu(QMenu* menu);

signals:
    void projectOpened(const QString& projectPath);
    void projectClosed();
    void fileRequested(const QString& filePath);

private:
    static QString findProjectFile(const QString& dirPath);

    QWidget* parent_widget_;       // 用于 QFileDialog 等模态对话框
    EditorManager* editor_mgr_;
    QString current_project_path_;
};

}  // namespace etest::app
```

**从 MainWindow 搬来的方法清单**：

| 原方法 | 新位置 | 改动 |
|--------|--------|------|
| `findProjectFile()` | → `ProjectController::findProjectFile()` | 纯搬移 |
| `onNewProject()` | → `ProjectController::newProject()` | 纯搬移 |
| `onOpenProject()` | → `ProjectController::openProject()` | 纯搬移 |
| `onOpenFile()` | → `ProjectController::openFile()` | `editor_manager_` 访问改为 `editor_mgr_` |
| `onCloseProject()` | → `ProjectController::closeProject()` | 纯搬移 |
| `onProjectClosed()` | → `ProjectController::closeProject()` 内部，通过 signal `projectClosed` 通知 | 其内部 `destroyEngine()` 调用通过 signal 转发给 ExecutionPanelController |
| `updateWindowTitle()` | → `ProjectController::updateWindowTitle()` | 纯搬移 |
| `updateRecentProjectsMenu()` | → `ProjectController::updateRecentProjectsMenu()` | 纯搬移 |
| `updateRecentFilesMenu()` | → `ProjectController::updateRecentFilesMenu()` | 纯搬移 |
| `onProjectOpened()` | → `ProjectController` 内部，通过 signal `projectOpened` 通知 | 部分逻辑保留在 MainWindow（初始化 signal_registry_ 等）|
| `tryCloseCurrentProject()` | → `ProjectController` 内部 | 纯搬移 |
| `openRecentProject()` | → `ProjectController::openRecent()` | 纯搬移 |

### 3.2 EditorPanelController

**文件**：新增 `src/app/EditorPanelController.h` / `.cpp`

**职责**：编辑器操作命令的代理层

```cpp
namespace etest::app {

class EditorPanelController : public QObject {
    Q_OBJECT
public:
    explicit EditorPanelController(EditorManager* editorMgr,
                                   QClipboard* clipboard,
                                   AppStatusBarController* statusBarCtrl,
                                   QObject* parent = nullptr);

    // 文件操作
    void saveCurrent();
    void saveCurrentAs();

    // 编辑操作
    void undo();
    void redo();
    // ...

    // 连接当前编辑器的状态变化信号
    void connectCurrentEditor();

signals:
    void modificationChanged(bool modified);
    void cursorPositionChanged(int line, int col);

private:
    void disconnectCurrentEditor();
    void updateEditorStatus();  // 从当前编辑器读取语言/编码/换行符 → status_bar_ctrl_

    EditorManager* editor_mgr_;
    QClipboard* clipboard_;
    AppStatusBarController* status_bar_ctrl_;  // 直接调用 setLanguage/setEncoding/setEol

    QMetaObject::Connection mod_connection_;
    QMetaObject::Connection sel_connection_;
    QMetaObject::Connection state_connection_;
};

}  // namespace etest::app
```

**从 MainWindow 搬来的方法清单**：

| 原方法 | 新位置 | 改动 |
|--------|--------|------|
| `onSaveFile()` | → `EditorPanelController::saveCurrent()` | 纯搬移 |
| `onSaveFileAs()` | → `EditorPanelController::saveCurrentAs()` | 纯搬移 |
| `onSaveAllFiles()` | → `EditorPanelController::saveAll()` | 纯搬移 |
| `onCloseCurrentFile()` | → `EditorPanelController::closeCurrent()` | 纯搬移 |
| `onCloseAllFiles()` | → `EditorPanelController::closeAll()` | 纯搬移 |
| `onUndo()` | → `EditorPanelController::undo()` | 纯搬移 |
| `onRedo()` | → `EditorPanelController::redo()` | 纯搬移 |
| `onCut()` | → `EditorPanelController::cut()` | 纯搬移 |
| `onCopy()` | → `EditorPanelController::copy()` | 纯搬移 |
| `onPaste()` | → `EditorPanelController::paste()` | 纯搬移 |
| `onFind()` | → `EditorPanelController::find()` | 纯搬移 |
| `onReplace()` | → `EditorPanelController::replace()` | 纯搬移 |
| `onGoToLine()` | → `EditorPanelController::goToLine()` | 纯搬移 |
| `current_editor_modification_connection_` | → `EditorPanelController::mod_connection_` | 纯搬移 |
| `current_editor_selection_connection_` | → `EditorPanelController::sel_connection_` | 纯搬移 |
| `current_editor_state_connection_` | → `EditorPanelController::state_connection_` | 纯搬移 |
| `clipboard_` | → `EditorPanelController::clipboard_` | 纯搬移 |

### 3.3 ExecutionPanelController

**文件**：新增 `src/app/ExecutionPanelController.h` / `.cpp`

**职责**：测试执行引擎的生存期管理和 UI 控制

```cpp
namespace etest::app {

class ExecutionPanelController : public QObject {
    Q_OBJECT
public:
    explicit ExecutionPanelController(QWidget* parentWidget,
                                      ExecutionDebugWidget* debugWidget,
                                      ExecutionOutputPanel* outputPanel,
                                      QObject* parent = nullptr);

    // 引擎生存期
    void createEngine(etest::core::SignalRegistry* registry,
                      std::shared_ptr<icd::Repository> repo);
    void destroyEngine();

    // 执行控制
    void run();
    void pause();
    void stop();
    void verify();
    void runAll();

    // 状态
    void syncControlStates();
    bool canRun() const;

    // Ribbon 动作（供 MainWindow setupRibbon 获取）
    QAction* runAction() const { return act_run_; }
    QAction* pauseAction() const { return act_pause_; }
    QAction* stopAction() const { return act_stop_; }
    QAction* verifyAction() const { return act_verify_; }
    QAction* runAllAction() const { return act_run_all_; }

    // Ribbon 统计标签（供 MainWindow 布局用）
    QLabel* ribbonStatsLabel() const { return label_ribbon_stats_; }

signals:
    void engineStateChanged(etest::engine::EngineState state);
    void execStatsUpdated(int pass, int fail, int elapsed);
    void preconditionResult(bool canRun);

private:
    // 引擎
    etest::engine::TestExecutionEngine* engine_ = nullptr;

    // Ribbon 运行动作（原 MainWindow 创建，搬移到此处）
    QAction* act_run_ = nullptr;
    QAction* act_pause_ = nullptr;
    QAction* act_stop_ = nullptr;
    QAction* act_verify_ = nullptr;
    QAction* act_run_all_ = nullptr;
    QLabel*  label_ribbon_stats_ = nullptr;

    // 引擎状态（原 MainWindow）
    int pass_count_ = 0;
    int fail_count_ = 0;
    QString current_program_name_;

    // 引用外部组件
    QWidget* parent_widget_;               // 用于对话框
    ExecutionDebugWidget* debug_widget_;   // 刷新前提条件
    ExecutionOutputPanel* output_panel_;   // 输出执行日志

    // 引擎状态标签（已在 MainWindow 中创建为 statusBar permanent widget，
    // 但 label_engine_state_ 和 label_exec_stats_ 可能保留在 MainWindow，
    // 通过 signal → AppStatusBarController 驱动）
};

}  // namespace etest::app
```

**从 MainWindow 搬来的方法清单**：

| 原方法 | 新位置 | 改动 |
|--------|--------|------|
| `createEngine()` | → `ExecutionPanelController::createEngine()` | 纯搬移 |
| `destroyEngine()` | → `ExecutionPanelController::destroyEngine()` | 纯搬移 |
| `syncControlStates()` | → `ExecutionPanelController::syncControlStates()` | 纯搬移 |
| `onRunClicked()` | → `ExecutionPanelController::run()` | 纯搬移 |
| `onPauseClicked()` | → `ExecutionPanelController::pause()` | 纯搬移 |
| `onStopClicked()` | → `ExecutionPanelController::stop()` | 纯搬移 |
| `onVerifyClicked()` | → `ExecutionPanelController::verify()` | 纯搬移 |
| `onRunAllClicked()` | → `ExecutionPanelController::runAll()` | 纯搬移 |
| `onProgramSaved()` | → `ExecutionPanelController` 内部 | 纯搬移（通过 signal 转发） |
| `act_run_` / `act_pause_` / `act_stop_` / `act_verify_` / `act_run_all_` | → `ExecutionPanelController` | 所有权转移 |
| `label_ribbon_stats_` | → `ExecutionPanelController` | 所有权转移 |
| `engine_` | → `ExecutionPanelController` | 所有权转移 |
| `pass_count_` / `fail_count_` | → `ExecutionPanelController` | 所有权转移 |
| `current_program_name_` | → `ExecutionPanelController` | 所有权转移 |

### 3.4 AppStatusBarController

**文件**：新增 `src/app/AppStatusBarController.h` / `.cpp`

**职责**：统一管理 `QStatusBar` 上的所有标签

```cpp
namespace etest::app {

class AppStatusBarController : public QObject {
    Q_OBJECT
public:
    explicit AppStatusBarController(QObject* parent = nullptr);

    void setup(QStatusBar* statusBar);

public slots:
    void setProject(const QString& name);
    void setEngineState(const QString& text);
    void setExecStats(int pass, int fail, int elapsed);
    void setCursorPos(int line, int col);
    void setEncoding(const QString& enc);
    void setEol(const QString& eol);
    void setLanguage(const QString& lang);
    void setErrorsWarnings(int errors, int warnings);

private:
    QStatusBar* status_bar_ = nullptr;

    // 原 MainWindow 中 8 个 status_xxx_label_ 的整合
    QLabel* label_message_ = nullptr;    // 左侧临时消息
    QLabel* label_project_ = nullptr;    // 项目名
    QLabel* label_errors_ = nullptr;     // 错误/警告计数
    QLabel* label_cursor_ = nullptr;     // 行:列
    QLabel* label_encoding_ = nullptr;   // 编码
    QLabel* label_eol_ = nullptr;        // 换行符
    QLabel* label_language_ = nullptr;   // 语言
    QLabel* label_engine_state_ = nullptr;  // 引擎状态（右侧）
    QLabel* label_exec_stats_ = nullptr;    // 执行统计（右侧）
};

}  // namespace etest::app
```

**从 MainWindow 搬来的内容**：

| 原内容 | 新位置 | 改动 |
|--------|--------|------|
| `status_message_label_` | → `AppStatusBarController::label_message_` | 纯搬移 |
| `status_project_label_` | → `AppStatusBarController::label_project_` | 纯搬移 |
| `status_errors_label_` | → `AppStatusBarController::label_errors_` | 纯搬移 |
| `status_cursor_label_` | → `AppStatusBarController::label_cursor_` | 纯搬移 |
| `status_encoding_label_` | → `AppStatusBarController::label_encoding_` | 纯搬移 |
| `status_eol_label_` | → `AppStatusBarController::label_eol_` | 纯搬移 |
| `status_language_label_` | → `AppStatusBarController::label_language_` | 纯搬移 |
| `label_engine_state_` | → `AppStatusBarController::label_engine_state_` | 纯搬移 |
| `label_exec_stats_` | → `AppStatusBarController::label_exec_stats_` | 纯搬移 |
| `createStatusBar()` | → `AppStatusBarController::setup()` | 搬移 + 参数化 |

### 3.5 TuxSaverController

**文件**：新增 `src/app/TuxSaverController.h` / `.cpp`

**职责**：屏保空闲检测和 TuxSaverOverlay 的生命周期管理

```cpp
namespace etest::app {

class TuxSaverController : public QObject {
    Q_OBJECT
public:
    explicit TuxSaverController(QWidget* parentWidget,
                                QObject* parent = nullptr);

    void start();   // 启动空闲检测计时器（lazyInit 中调用）
    void stop();    // 停止计时器

    void onUserActivity();  // 由 MainWindow::eventFilter 调用

signals:
    void saverActivated();
    void saverDeactivated();

private:
    void showSaver();
    void hideSaver();

    QWidget* parent_widget_;
    TuxSaverOverlay* overlay_ = nullptr;
    QElapsedTimer idle_timer_;
    QTimer* check_timer_ = nullptr;
    int idle_timeout_ms_ = 60000;
};

}  // namespace etest::app
```

**从 MainWindow 搬来的内容**：

| 原成员 | 新位置 | 改动 |
|--------|--------|------|
| `tux_overlay_` | → `TuxSaverController::overlay_` | 纯搬移 |
| `tux_idle_timer_` | → `TuxSaverController::idle_timer_` | 纯搬移 |
| `tux_idle_check_timer_` | → `TuxSaverController::check_timer_` | 纯搬移 |
| `eventFilter` 中 `QEvent::User`/`QEvent::KeyPress`/`QEvent::MouseMove` → `onUserActivity()` | → `TuxSaverController::onUserActivity()` | 封装为公开方法 |

**与 MainWindow 的协作方式**：

```cpp
// MainWindow::lazyInit() 中
tux_controller_ = new TuxSaverController(this, this);
tux_controller_->start();

// MainWindow::eventFilter() 中 — 原屏保检测代码缩减为一行的委托
bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress ||
        event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonPress) {
        tux_controller_->onUserActivity();
    }
    // ... 其他 eventFilter 逻辑（由父类或其他处理） ...
}
```

屏保逻辑完全自包含（计时器 → 检测空闲 → 显示/隐藏 Overlay），不与任何其他 Controller 产生信号/槽连接。

---

## 四、Controller 创建时序与初始化策略

### 4.1 依赖关系图

```
创建时机                     依赖链
─────────                   ──────
                            editor_manager_ (lazyInit step 4)
constructor 中创建 ──────→  ├── EditorPanelController(editor_manager_)
setupRibbon()               │
需要 act_run_ 等 QAction    ├── ExecutionPanelController(QAction*)
已存在                      │     依赖 debug widget 和 output panel，
                            │     但 QAction 创建不依赖它们
                            │
lazyInit() 中创建 ────────→  ├── ProjectController(parentWidget, editor_manager_)
(step 2: debug widget)      │
(step 3: output panel)      └── ExecutionPanelController::postInit(
(step 4: editor_manager_)        debug_widget_, output_panel_)
```

### 4.2 解决"鸡生蛋"问题：分步初始化

`ExecutionPanelController` 的 QAction 需要早（constructor 时创建，供 `setupRibbon()` 放入 Ribbon Panel），
但它的依赖项（`ExecutionDebugWidget*`、`ExecutionOutputPanel*`）在 `lazyInit()` 中才创建。

解决方案：**两步初始化**。

```cpp
// ─── 第一步：构造函数中创建 QAction ───
// ExecutionPanelController 的构造只创建 ribbon action 对象
// QAction parent 设为 MainWindow（保持 Qt 对象树一致性）
ExecutionPanelController::ExecutionPanelController(QWidget* parentWidget,
                                                   QObject* parent)
    : QObject(parent) {
    act_run_ = new QAction(QIcon(":/icons/run.svg"), "运行", parentWidget);
    act_pause_ = new QAction(QIcon(":/icons/pause.svg"), "暂停", parentWidget);
    // ... 其他 action
    // 此时 engine_, debug_widget_ 等都是 nullptr
}

// ─── 第二步：依赖就绪后初始化（在 MainWindow::lazyInit() 末尾调用） ───
void ExecutionPanelController::postInit(
    ExecutionDebugWidget* debugWidget,
    ExecutionOutputPanel* outputPanel,
    etest::core::SignalRegistry* registry,
    std::shared_ptr<icd::Repository> repo) {
    debug_widget_ = debugWidget;
    output_panel_ = outputPanel;
    // 此时才创建引擎、连接信号
    createEngine(registry, std::move(repo));
}
```

### 4.3 各 Controller 创建时序总表

| Controller | 创建时机 | 构造参数 | 依赖就绪条件 |
|-----------|---------|----------|-------------|
| `AppStatusBarController` | **构造函数中** | `(QObject* parent = this)` | 无依赖；`setup(QStatusBar*)` 在 `initUi()` 中调用 |
| `ExecutionPanelController` | **构造函数中**（仅 QAction） | `(parentWidget, parent)` | QAction parent 设为 MainWindow，不依赖 lazyInit 产物 |
| `ProjectController` | **lazyInit() 末尾** | `(parentWidget, editor_manager_, this)` | 需要 `editor_manager_` 就绪（lazyInit step 4） |
| `EditorPanelController` | **lazyInit() 末尾** | `(editor_manager_, clipboard_, status_bar_ctrl_, this)` | 需要 `editor_manager_` 就绪（lazyInit step 4） |
| `TuxSaverController` | **lazyInit() 末尾** | `(parentWidget, this)` | 无依赖；`start()` 在 lazyInit 末尾调用 |

```cpp
MainWindow::MainWindow(QWidget* parent)
    : SARibbonMainWindow(parent) {

    // 1) 创建不依赖 lazyInit 的 Controller
    status_bar_ctrl_ = new AppStatusBarController(this);
    execution_controller_ = new ExecutionPanelController(this, this);

    initUi();  // → setupRibbon() 中通过 execution_controller_->runAction()
              //   获取 QAction 放入 Ribbon Panel
              // → createStatusBar() 改为 status_bar_ctrl_->setup(statusBar())

    initSignalsEarly();

    // 2) 安排懒加载
    QTimer::singleShot(0, this, &MainWindow::lazyInit);
}

void MainWindow::lazyInit() {
    // ... 现有步骤创建 editor_manager_, debug_widget_, output_panel_ ...

    // 3) 创建依赖 lazyInit 的 Controller
    project_controller_ = new ProjectController(this, editor_manager_, this);
    editor_controller_ = new EditorPanelController(editor_manager_, clipboard_,
                                                    status_bar_ctrl_, this);

    // 4) 补全 ExecutionPanelController 的依赖
    execution_controller_->postInit(
        execution_debug_widget_, execution_output_panel_,
        signal_registry_, icd_repository_);

    // 5) 统一粘合信号/槽
    initSignalsLate();
}
```

### 4.4 QAction 所有权策略

| QAction 归属 | parent 对象 | 被谁持有指针 | 析构安全 |
|-------------|-----------|-------------|---------|
| 执行按钮（act_run_ 等，~5 个） | `MainWindow`（`parentWidget` 参数） | `ExecutionPanelController` 原始指针 | MainWindow 析构 → Qt 自动删除 → Controller 无 dangling ptr（因为 shared/weak 或在析构中主动置空） |
| 文件/编辑/视图 QAction（~22 个） | `MainWindow`（`this`） | `MainWindow`（Facade 存根） | Qt 对象树自动清理，不涉及跨对象所有权 |
| Demo 启动 QAction（~4 个） | `MainWindow`（`this`） | `MainWindow` | 同上 |

> **原则**：QAction 的 Qt 父对象统一为 `MainWindow`，确保对象树一致。
> Controller 仅持有原始指针（raw pointer），不在其析构中 delete action。

---

## 五、拆分后的 MainWindow 骨架

### 5.1 头文件（瘦身后）

> ⚠️ **真实盘点**：瘦身后的 MainWindow 仍有约 **51 个** 成员变量，而不是"~25 个"。
> 原因：大量 QAction（~22 个）和子控件指针（底部面板 4 个、侧边栏组件等）仍保留在 MainWindow 中。
> 但关键区别是——**这些成员不再负责业务逻辑**，QAction 仅作为 Ribbon/Menu 的"存根"保留，
> triggered 信号通过 λ 直接委托给 Controller。MainWindow 从"执行者"退化为"路由表"。
>
> 以下骨架**完整列出所有留存成员**，不做选择性省略：

```cpp
class MainWindow : public SARibbonMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void initUi();
    void initSignalsEarly();
    void initSignalsLate();
    void lazyInit();
    void onThemeChanged(bool isDark);
    void saveWindowState();
    void restoreWindowState();
    void setupRibbon();
    static void hideDockTitleBarButtons(ads::CDockAreaWidget* area);

    // ── 布局组成员（保留） ──
    ads::CDockManager* dock_manager_;
    ActivityBarWidget* activity_bar_;
    SidebarWidget* sidebar_;
    HintBarWidget* hint_bar_;
    QSplitter* h_splitter_;
    QSplitter* v_splitter_;
    BottomContainerWidget* bottom_container_;
    int bottom_container_height_ = 200;

    // ── 侧边栏组件（保留指针，因为它们是 SidebarWidget 的子控件，MainWindow 持有引用） ──
    TestProgramManagerWidget* test_program_mgr_ = nullptr;
    ExecutionDebugWidget* execution_debug_widget_ = nullptr;

    // ── 底部面板（保留指针，用于面板可见性切换） ──
    LogOutputPanel* log_panel_;
    ExecutionOutputPanel* execution_output_panel_;
    ProblemsPanel* problems_panel_;
    TerminalPanel* terminal_panel_;

    // ── 辅助侧边栏（保留） ──
    QWidget* aux_sidebar_widget_ = nullptr;
    int aux_sidebar_width_ = 280;
    int sidebar_expanded_width_ = 280;

    // ── 核心组件（保留指针） ──
    EditorManager* editor_manager_;
    WelcomeWidget* welcome_widget_ = nullptr;
    ads::CDockWidget* central_dock_ = nullptr;

    // ── ICD 数据（由 ProjectController 填充，MainWindow 保留供 ExecutionPanelController 使用） ──
    etest::core::SignalRegistry* signal_registry_ = nullptr;
    std::shared_ptr<icd::Repository> icd_repository_;

    // ── 子系统 Controller（新增） ──
    ProjectController* project_controller_ = nullptr;
    EditorPanelController* editor_controller_ = nullptr;
    ExecutionPanelController* execution_controller_ = nullptr;
    AppStatusBarController* status_bar_ctrl_ = nullptr;

    // ── 设置对话框 ──
    SettingsDialog* settings_dialog_ = nullptr;

    // ── 文件/视图菜单 QAction 存根 ──
    QMenu* recent_projects_menu_ = nullptr;
    QMenu* recent_files_menu_ = nullptr;
    QAction* view_output_action_ = nullptr;
    QAction* view_execution_output_action_ = nullptr;
    QAction* view_problems_action_ = nullptr;
    QAction* view_terminal_action_ = nullptr;
    QAction* view_aux_sidebar_action_ = nullptr;
    QAction* new_project_action_ = nullptr;
    QAction* open_project_action_ = nullptr;
    QAction* open_file_action_ = nullptr;
    QAction* close_project_action_ = nullptr;
    QAction* save_action_ = nullptr;
    QAction* save_as_action_ = nullptr;
    QAction* save_all_action_ = nullptr;
    QAction* close_file_action_ = nullptr;
    QAction* close_all_files_action_ = nullptr;

    // ── 编辑菜单 QAction 存根 ──
    QAction* edit_undo_action_ = nullptr;
    QAction* edit_redo_action_ = nullptr;
    QAction* edit_cut_action_ = nullptr;
    QAction* edit_copy_action_ = nullptr;
    QAction* edit_paste_action_ = nullptr;
    QAction* edit_find_action_ = nullptr;
    QAction* edit_replace_action_ = nullptr;
    QAction* edit_go_to_line_action_ = nullptr;

    // ── Demo 启动动作（保留，Ribbon 需要） ──
    QAction* demo_topology_action_ = nullptr;
    QAction* demo_protocol_action_ = nullptr;
    QAction* demo_testprogram_action_ = nullptr;
    QAction* demo_testexecutor_action_ = nullptr;

    // ── 屏保（委托给 TuxSaverController） ──
    TuxSaverController* tux_controller_ = nullptr;

    // ── 保留（懒加载覆盖层、登录认证） ──
    LoadingOverlay* loading_overlay_ = nullptr;
    QMenu* login_menu_ = nullptr;
    QAction* login_user_info_action_ = nullptr;
    QAction* login_manage_users_action_ = nullptr;

    // ── 首次显示 ──
    bool first_show_ = true;
};
```

**效果对比**：

| 指标 | 重构前 | 重构后 | 实际变化 |
|------|--------|--------|----------|
| 成员变量总数 | ~55 个 | ~51 个 | -4（3 个 tux 成员 → 1 个 Controller 指针） |
| **承载业务逻辑的成员** | ~55 个 | **~9 个**（5 个 Controller + signal_registry_/icd_repository_） | **-84%** |
| QAction/QLabel 存根 | ~30 个 | ~30 个（但只做路由，不参与逻辑） | 职责转移，数量不变 |
| 私有方法（业务逻辑） | ~50 个 | ~12 个 | -76% |
| 子系统职责承担者 | 1 个类（MainWindow） | **6 个类**（MainWindow + 5 Controller） | +500% 内聚性 |

> **关键认知**：重构的核心收益不是"成员变量减少了"，而是**每个成员承担的职责变轻了**。
> 以前 55 个成员全部由 MainWindow 直接管理——开关面板要操作 QAction、更新状态栏要逐个设 QLabel、
> 保存文件要找 EditorManager——所有逻辑耦合在 ~5000 行的 cpp 里。
> 重构后，MainWindow 的 ~51 个成员中，~22 个 QAction 只负责"被点击 → emit triggered → λ 路由到 Controller"，
> 4 个子控件指针只用于布局管理，真正的业务数据/状态分布在 5 个 Controller 内部（对 Facade 不可见）。

### 5.2 initSignalsLate 示意

```cpp
void MainWindow::initSignalsLate() {
    // ── Project → Editor ──
    connect(project_controller_, &ProjectController::fileRequested,
            editor_controller_, &EditorPanelController::openFile);

    // ── Project → StatusBar ──
    connect(project_controller_, &ProjectController::projectOpened,
            status_bar_ctrl_, &AppStatusBarController::setProject);
    connect(project_controller_, &ProjectController::projectClosed,
            status_bar_ctrl_, [this]() {
                status_bar_ctrl_->setProject("");
                status_bar_ctrl_->setEngineState(QStringLiteral("空闲"));
            });

    // ── Execution → StatusBar ──
    connect(execution_controller_, &ExecutionPanelController::engineStateChanged,
            status_bar_ctrl_, &AppStatusBarController::setEngineState);
    connect(execution_controller_, &ExecutionPanelController::execStatsUpdated,
            status_bar_ctrl_, &AppStatusBarController::setExecStats);

    // ── Editor → StatusBar ──
    // cursorPosition 通过信号中转（因为需要跨 Controller）
    connect(editor_controller_, &EditorPanelController::cursorPositionChanged,
            status_bar_ctrl_, &AppStatusBarController::setCursorPos);
    // 语言/编码/换行符等状态：由 EditorPanelController 内部在 connectCurrentEditor()
    // 中直接调用 status_bar_ctrl_ 的公开槽（不经过 Facade 中转）

    // ── 引擎状态 → ribbon 按钮同步 ──
    connect(execution_controller_, &ExecutionPanelController::engineStateChanged,
            this, [this]() { execution_controller_->syncControlStates(); });

    // ── 编辑器切换 → 重新连接编辑器信号 + 同步 Ribbon ──
    connect(editor_manager_, &EditorManager::currentEditorChanged,
            editor_controller_, &EditorPanelController::connectCurrentEditor);
    connect(editor_manager_, &EditorManager::currentEditorChanged,
            execution_controller_, &ExecutionPanelController::syncControlStates);

    // ── 面板可见性 ──
    connect(view_output_action_, &QAction::triggered,
            this, [this]() {
                bool vis = !log_panel_->isVisible();
                log_panel_->setVisible(vis);
                view_output_action_->setChecked(vis);
            });
    connect(view_execution_output_action_, &QAction::triggered,
            this, [this]() {
                bool vis = !execution_output_panel_->isVisible();
                execution_output_panel_->setVisible(vis);
                view_execution_output_action_->setChecked(vis);
            });
    connect(view_problems_action_, &QAction::triggered,
            this, [this]() { problems_panel_->toggleView(); });
    connect(view_terminal_action_, &QAction::triggered,
            this, [this]() { terminal_panel_->toggleView(); });
}
```

> **语言/编码/换行符状态更新**：这部分逻辑不经过 `initSignalsLate()` 中转，
> 而是在 `EditorPanelController::connectCurrentEditor()` 内部直接调用
> `AppStatusBarController` 的公开槽方法。因为 EditorPanelController 持有
> `AppStatusBarController*` 指针（通过构造函数注入），编辑器切换时即可直接更新状态栏，
> 不需要在 Facade 层额外拉线。

---

## 六、CMake 变更

```cmake
# src/app/CMakeLists.txt
set(SOURCES
    ...
    ProjectController.cpp
    EditorPanelController.cpp
    ExecutionPanelController.cpp
    AppStatusBarController.cpp
    TuxSaverController.cpp
    ...
)

set(HEADERS
    ...
    ProjectController.h
    EditorPanelController.h
    ExecutionPanelController.h
    AppStatusBarController.h
    TuxSaverController.h
    ...
)
```

---

## 七、执行计划（2 阶段方案）

> ⚠️ **评审发现**：原 5 Phase 方案中 Phase 1（AppStatusBarController）删除了 `label_engine_state_` 等成员，
> 但 Phase 4 搬迁前的 `createEngine()` 仍直接引用它们——导致 Phase 1 和 Phase 4 之间存在**编译耦合**，
> 无法独立回滚。
>
> **修正方案**：改为 2 阶段，Phase A 可以独立提交，Phase B 四个 Controller 在同一个分支/提交中搬迁。

```
Phase A: AppStatusBarController（最独立，可单独合并，1~2 小时）
Phase B: 四个 Controller 同时搬迁（在同一个 feature 分支完成，5~7 小时）
  ├── ProjectController
  ├── EditorPanelController
  ├── ExecutionPanelController
  └── TuxSaverController
Phase C: 收尾清理（删除过渡代码，1 小时）
```

### Phase A: AppStatusBarController（可独立合并）

1. 新建 `AppStatusBarController.h/.cpp`
2. 将所有 `status_*_label_` 成员搬入
3. 将 `createStatusBar()` 搬入改为 `setup(QStatusBar*)`
4. MainWindow 中删除对应的裸 QLabel 成员，改为 `AppStatusBarController*` 指针
5. `initUi()` 中调用 `status_bar_ctrl_->setup(statusBar())`

6. **关键过渡策略：MainWindow 旧代码保留两份路径**
   ```cpp
   // 重构前：直接操作裸成员
   // status_message_label_->setText("就绪");

   // 过渡期：通过 AppStatusBarController 的公开槽
   // 旧代码路径保持可用，因为 AppStatusBarController::setEngineState()
   // 内部做实际 setText()。MainWindow 原方法（如 createEngine() 中
   // 的 statusBar()->showMessage()）仍然可调用。
   // 等到 Phase B 执行引擎逻辑搬走后，旧调用点自然消失。

   // 不会出现编译断裂 —— status message 的原始 setText 调用替换为
   // status_bar_ctrl_->setXxx()，接口等价。
   ```

7. 扫描所有直接操作 status label 的 `setText()`/`showMessage()` 调用点，替换为对应的 `status_bar_ctrl_->setXxx()` 方法
8. 构建验证 + **冒烟验证**：启动主窗口，检查状态栏项目名、引擎状态、光标位置显示正常

### Phase B: 四个 Controller 同时搬迁（同一分支完成）

在 `feat/mainwindow-controller-refactor` 分支上完成，一次性提交：

1. **前置准备**（分支创建后立即做）：
   - 一次性将 5 组 .h/.cpp 文件加入 CMakeLists.txt
   - 创建空的 `ProjectController`、`EditorPanelController`、`ExecutionPanelController`、`TuxSaverController` 类声明

2. **ProjectController 搬移**：
   - 将 `onNewProject`、`onOpenProject`、`onOpenFile`、`onCloseProject`、`onProjectClosed`、`openRecentProject`、`findProjectFile`、`updateWindowTitle`、`updateRecent*Menu`、`tryCloseCurrentProject` 搬入
   - `onProjectClosed` 中的 `destroyEngine()` 调用改为 emit signal → Facade 接收 → 调用 `execution_controller_->destroyEngine()`
   - 打开/关闭项目时初始化的 `signal_registry_` 和 `icd_repository_` 仍保留在 MainWindow（因为 ExecutionPanelController 也需要它们）
   - 构建验证

3. **EditorPanelController 搬移**：
   - 将 `onSaveFile/As/All`、`onCloseCurrentFile/All`、`onUndo/Redo/Cut/Copy/Paste`、`onFind/Replace/GoToLine` 搬入
   - `clipboard_` 所有权转移
   - `current_editor_*_connection_` 搬入，封装 `connectCurrentEditor()`
   - `EditorPanelController` 构造函数注入 `AppStatusBarController*`，在 `connectCurrentEditor()` 中直接更新语言/编码/换行符
   - 构建验证

4. **ExecutionPanelController 搬移**：
   - 将 `createEngine()`、`destroyEngine()`、`syncControlStates()`、5 个 run 方法搬入
   - Ribbon QAction（`act_run_` 等）创建代码从 `setupRibbon()` 搬到此类构造函数
   - `engine_`、`pass_count_` / `fail_count_` 所有权转移
   - `setupRibbon()` 改为从 `execution_controller_->runAction()` 等 getter 获取 QAction
   - 构建验证

5. **TuxSaverController 搬移**：
   - 将 `tux_overlay_`、`tux_idle_timer_`、`tux_idle_check_timer_` 搬入
   - `eventFilter` 中的空闲检测代码改为调用 `tux_controller_->onUserActivity()`
   - 屏保的 `#include "widgets/TuxSaverOverlay.h"` 从 MainWindow 头文件中移除（由 TuxSaverController 内部包含）
   - 构建验证

6. **Integration 验证**：
   - 全量编译
   - 启动主窗口 → 验证 Ribbon 按钮可用
   - 打开项目 → 验证状态栏更新
   - 打开编辑器 → 验证行/列显示
   - 点击运行 → 验证引擎启动和控制
   - 关闭项目 → 验证清理
   - **屏保**：等待 60 秒 → 验证 TuxSaverOverlay 出现 → 移动鼠标 → 验证消失

### Phase C: 收尾清理

1. 删除不再需要的 `#include`（`<QClipboard>` 等）
2. 删除 `MainWindow` 构造函数中的 `QElapsedTimer` 计时代码（构造阶段不再有重量级操作，统计构造函数耗时已无意义）
3. 确认所有信号连接在 `initSignalsLate()` 中都有体现
4. 运行全量构建 + 功能冒烟

### 每个 Phase 的功能验证点

| Phase | 必须通过的冒烟测试 |
|-------|-------------------|
| A | 主窗口启动 → 状态栏显示"就绪"/项目名/光标位置；关闭窗口 → 无崩溃 |
| B | 打开项目 → 编辑器可编辑 → 保存 → 关闭项目 → 重新打开 → 运行测试程序 → 暂停/停止 |
| C | 全量功能回归：文件/编辑菜单、引擎控制、面板切换、状态栏所有标签、窗口状态恢复 |

---

## 八、影响范围验证

### 在本模块内（src/app/）

| 文件 | 影响 | 处理方式 |
|------|------|----------|
| `MainWindow.h` | 大量成员和声明删除 | Phase A~C 逐步完成 |
| `MainWindow.cpp` | 大量方法搬移 | Phase A~C 逐步完成 |
| `CMakeLists.txt` | 新增 5 组源文件 | Phase B 前一次性添加 |
| `resource.qrc` | 无影响 | — |

### 外部模块

**外部模块完全不受影响**，原因如下：
- `MainWindow` 的所有 `onXxx()` 方法均为 `private` 或 `private slots`，无外部模块直接调用它们
- 外部模块仅通过信号/槽或公共方法与 MainWindow 交互（如 `EditorManager`、`TestExecutionEngine` 的信号），这些接口签名**保持不变**
- Ribbon QAction 的 `triggered` 信号接收方从 MainWindow 自身方法改为 λ 路由到 Controller，但 QAction 的公开接口（icon/text/tooltip）不变，Ribbon Panel 布局不受影响
- Controller 类对外部模块完全不可见

### 无影响范围

- `src/engine/` — engine 接口不改变
- `src/core/` — core 模块不引用 MainWindow
- `src/topology/`、`src/protocol/` — 不依赖 MainWindow
- `src/tools/` — 有各自的 MainWindow 实现（独立的 `QMainWindow` 子类）
- `tests/` — 不直接测试 MainWindow

---

## 九、回滚方案

### 方案局限性

⚠️ **Phase A 可独立回滚，Phase B 的四个 Controller 必须整体回滚或保留。**

原因：Phase A（AppStatusBarController）只搬动了状态栏标签——它们与其他 Controller 无实质耦合，
旧代码路径通过公开槽方法保持兼容，回滚 Phase A 不会影响其他代码。

Phase B 的四个 Controller 共享 `editor_manager_`、`signal_registry_`、`icd_repository_` 等公共依赖，
且 `initSignalsLate()` 中的连接变更交织在一起，无法独立回滚其中一个。

### 回滚操作

```bash
# 回滚 Phase A（安全，可独立执行）
git revert <phase-A-commit-hash>
git commit -m "revert: 回退 Phase A: AppStatusBarController"

# 回滚 Phase B（必须整体回滚，含 Phase C）
git revert <phase-B-commit-hash> <phase-C-commit-hash>
# 或者使用 git rebase -i 删除整个 feature 分支的合并提交
```

### 预防措施

1. Phase A 完成后提交并测试，确保可独立运行
2. Phase B 在 `feat/mainwindow-controller-refactor` 分支上开发，不直接提交到 master
3. Phase B 全量构建 + 冒烟测试通过后，以 squash merge 合入 master
4. 合入后若发现问题，直接 `git revert` 整个 merge commit，而不是逐个文件回滚

---

## 附录：状态栏标签归属变迁

| 标签 | 重构前 | 重构后 |
|------|--------|--------|
| `status_message_label_` | MainWindow 裸成员 | AppStatusBarController |
| `status_project_label_` | MainWindow 裸成员 | AppStatusBarController |
| `status_errors_label_` | MainWindow 裸成员 | AppStatusBarController |
| `status_cursor_label_` | MainWindow 裸成员 | AppStatusBarController |
| `status_encoding_label_` | MainWindow 裸成员 | AppStatusBarController |
| `status_eol_label_` | MainWindow 裸成员 | AppStatusBarController |
| `status_language_label_` | MainWindow 裸成员 | AppStatusBarController |
| `label_engine_state_` | MainWindow 裸成员 | AppStatusBarController |
| `label_exec_stats_` | MainWindow 裸成员 | AppStatusBarController |
