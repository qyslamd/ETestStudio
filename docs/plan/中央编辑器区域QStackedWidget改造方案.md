# 中央编辑器区域 QStackedWidget 改造方案

## 背景

ETest Studio 目前采用 Qt-Advanced-Docking-System（QADS）管理编辑器区域，运行测试时编辑器控件仍然活跃在视图树中。当前做法是通过禁用 ribbon 按钮和菜单项来"阻止"运行时的编辑操作——但这只是行为层面的限制，编辑器控件本身并未从渲染树中移除。

## 动机

现有方案有几点不足：

- **防不住**：禁用 action 只拦截了 ribbon 和菜单入口，其他路径（快捷键、拖拽、程序化调用）仍可能触及编辑器
- **不干净**：运行态下编辑器背景渲染、信号处理仍在进行，造成不必要的 CPU 开销
- **无法复用**：运行态页面（执行仪表盘）没有独立的插槽，要跟编辑器抢 central_area 的空间，布局代码互相牵扯
- **扩展性差**：未来如果要加"全屏报表"、"只读审查"等模式，没有现成的机制

改用 `QStackedWidget` 后，编辑态和运行态是两套独立的 `QWidget`，切换时 Qt 自动从视图树中移除另一页，事件根本发不到编辑器控件上。

## 目标

将 `initUi()` 中的 `central_area` 从 `QWidget` 替换为 `QStackedWidget`，运行/停止时自动切换页面，实现编辑态/运行态的物理隔离。

## 阶段划分

分两期实施，互不阻塞：

| 阶段 | 内容 | 依赖 |
|------|------|------|
| **Phase 1** | QStackedWidget 中央容器 + 运行态自动切页 | 无 |
| **Phase 2** | IEditor 统一 `setReadOnly` 接口 + 各编辑器实现 | 建议在 Phase 1 之后做，但不是硬依赖 |

## 结构对比

### 改造前

```
v_splitter_
├── central_area (QWidget, QVBoxLayout)      ← 单页面
│   ├── hint_bar_
│   └── dock_manager_ (中央编辑器)
└── bottom_container_
```

运行态：所有控件仍在，仅靠 action enable/disable 限制交互。

### 改造后

```
v_splitter_
├── central_stack (QStackedWidget)            ← 双页面
│   ├── [0] page_editor (QWidget, QVBoxLayout)  ← 编辑态
│   │   ├── hint_bar_
│   │   └── dock_manager_
│   └── [1] page_exec (QWidget, QVBoxLayout)    ← 运行态
│       └── exec_dashboard_widget_ (占位)
└── bottom_container_
```

运行态：page_editor 不在视图树中，Qt 不渲染、不派发事件给它。

## 运行时行为

```
编辑态 (page 0)  →  用户点击「运行」
         │
         ▼
  run() 切到 page 1  ←  执行仪表盘全屏显示
         │
    ┌────┴────┐
    │         │
 执行完成   用户点「停止」
    │         │
    └────┬────┘
         ▼
  engineFinished/stop() 切回 page 0
```

切换时机由 `ExecutionPanelController` 控制：
- `run()` 末尾 → page 1（执行开始，编辑器消失）
- `stop()` / `engineFinished` → page 0（执行结束，编辑器恢复）

## 需要改动的文件

| 文件 | 改动 |
|------|------|
| `src/app/MainWindow.h` | 新增 `QStackedWidget* central_stack_`、`QWidget* exec_dashboard_page_` |
| `src/app/MainWindow.cpp` | `initUi()` 中将 `central_area` 改为 `QStackedWidget`，拆两页；加 `#include <QStackedWidget>`、显式 `#include <QLabel>` |
| `src/app/ExecutionPanelController.h` | 新增 `setCentralStack()` 声明和 `central_stack_` 成员；加 `class QStackedWidget;` 前向声明 |
| `src/app/ExecutionPanelController.cpp` | 实现 `setCentralStack()`，run/stop/engineFinished/destroyEngine 时切页 |

## 改造步骤

### step 1：MainWindow.h 新增成员

```cpp
// 在现有成员区（central_dock_ 附近）添加：
QStackedWidget* central_stack_ = nullptr;
QWidget* exec_dashboard_page_ = nullptr;
```

### step 2：initUi() 改造

**改造前**（约 184~198 行）：

```cpp
// 中央编辑器区域（提示栏 + DockManager）
auto* central_area = new QWidget(v_splitter_);
auto* central_area_layout = new QVBoxLayout(central_area);
central_area_layout->setContentsMargins(0, 0, 0, 0);
central_area_layout->setSpacing(0);

hint_bar_ = new HintBarWidget(central_area);
central_area_layout->addWidget(hint_bar_);

dock_manager_ = new ads::CDockManager(central_area);
central_area_layout->addWidget(dock_manager_, 1);
```

**改造后**：

```cpp
// 中央堆叠容器：编辑态 / 运行态
central_stack_ = new QStackedWidget(v_splitter_);

// ── 页 0：编辑态（提示栏 + DockManager） ──
auto* page_editor = new QWidget(central_stack_);
auto* page_editor_layout = new QVBoxLayout(page_editor);
page_editor_layout->setContentsMargins(0, 0, 0, 0);
page_editor_layout->setSpacing(0);

hint_bar_ = new HintBarWidget(page_editor);
page_editor_layout->addWidget(hint_bar_);

dock_manager_ = new ads::CDockManager(page_editor);
page_editor_layout->addWidget(dock_manager_, 1);

central_stack_->addWidget(page_editor);  // index 0

// ── 页 1：运行态（占位，后续替换为执行仪表盘） ──
exec_dashboard_page_ = new QWidget(central_stack_);
exec_dashboard_page_->setObjectName("ExecDashboardPage");
auto* exec_layout = new QVBoxLayout(exec_dashboard_page_);
auto* exec_placeholder = new QLabel(
    QStringLiteral("执行仪表盘（待实现）"), exec_dashboard_page_);
exec_placeholder->setAlignment(Qt::AlignCenter);
exec_layout->addWidget(exec_placeholder);
central_stack_->addWidget(exec_dashboard_page_);  // index 1

central_stack_->setCurrentIndex(0);  // 默认编辑态
```

`v_splitter_` 的 splitter children 从 `[central_area, bottom_container_]` 变为 `[central_stack_, bottom_container_]`。`addWidget` 和 `setSizes` 调用不变。

**新增 include**：`MainWindow.cpp` 需要加 `#include <QStackedWidget>`，以及显式 `#include <QLabel>`（page 1 占位的 QLabel 依赖。当前文件通过其他头文件间接包含 QLabel，但应显式声明依赖）。

### step 3：ExecutionPanelController 注入 + 切页

**.h** 新增（需在顶部加 `class QStackedWidget;` 前向声明）：

```cpp
// public 方法
void setCentralStack(QStackedWidget* stack);

// private 成员
QStackedWidget* central_stack_ = nullptr;
```

**.cpp** 实现：

```cpp
void ExecutionPanelController::setCentralStack(QStackedWidget* stack) {
  central_stack_ = stack;
}
```

**run() 末尾**（`engine_->start()` 之后）：

```cpp
if (central_stack_) {
  central_stack_->setCurrentIndex(1);
}
```

**stop() 中**（`engine_->stop()` 之后）：

```cpp
if (central_stack_) {
  central_stack_->setCurrentIndex(0);
}
```

**destroyEngine() 中**（安全兜底：如果引擎异常退出未发射 engineFinished，项目关闭时强制切回）：

```cpp
void ExecutionPanelController::destroyEngine() {
  if (central_stack_) {
    central_stack_->setCurrentIndex(0);
  }
  if (!engine_) {
    return;
  }
  // ... 现有 stop + deleteLater 逻辑 ...
}
```

**connectEngineSignals() 中 engineFinished 连接**：`engineFinished` 已有成功连接（保存 .etlog 报告），切页追加到**同一个 lambda 末尾**，不另起连接：

```cpp
connect(engine_, &etest::engine::TestExecutionEngine::engineFinished, this,
        [this]() {
          // 已有：保存 .etlog 报告
          if (!current_program_name_.isEmpty()) {
            auto& proj_mgr = etest::core::project::ProjectManager::instance();
            // ... save report ...
          }
          // 新增：切回编辑态
          if (central_stack_) {
            central_stack_->setCurrentIndex(0);
          }
        });
```

### step 4：MainWindow 注入

在 `lazyInit()` 中 `postInit()` 调用之后添加：

```cpp
execution_controller_->setCentralStack(central_stack_);
```

## 得失

### 所得

| 方面 | 说明 |
|------|------|
| **安全性** | 运行态编辑器物理不可达，不存在"运行时误改"的路径 |
| **性能** | 编辑器控件不在视图树中，Qt 不处理其渲染和事件，减少运行态开销 |
| **职责清晰** | 编辑态/运行态的 UI 组件完全独立，不会相互干扰 |
| **扩展性** | page 1 后续可替换为完整的执行仪表盘；page 2/3 可扩展为全屏报表、只读审查等模式 |

### 所失

| 方面 | 说明 |
|------|------|
| **复杂度+1** | 多出一个 `QStackedWidget` 层和切页逻辑，追踪布局时多一层间接 |
| **page 1 占位** | 首次部署后 page 1 是一个空占位控件，直到执行仪表盘真正实现前无明显价值 |
| **切页闪烁** | QStackedWidget 默认不带过渡动画，切换瞬间可能肉眼可见的闪烁（可后续通过 QPropertyAnimation 弥补） |

### 风险

- **最小**：改动集中在 `central_area` → `QStackedWidget` 的局部替换，不涉及活动栏、侧边栏、底部面板的任何逻辑
- **回归点**：切页后 `bottom_container_` 仍然在 `v_splitter_` 中，不受 `central_stack_` 影响，日志面板正常运行
- **回滚**：还原 `initUi()` 中两行关键代码即可回到 QWidget 模式

## 不涉及的文件

以下文件无需改动：
- 所有 Controller 类（除 ExecutionPanelController）
- 所有 Widget 类
- CMake 配置
- QSS 样式文件（QStackedWidget 默认无样式，`exec_dashboard_page_` 样式由内部子控件决定）

---

## Phase 2：IEditor readOnly 接口（概要）

### 动机

QStackedWidget 切页后编辑器物理不可达，但有些场景不需要切页——比如"边看 ICD 协议边跑测试"，这时编辑器需要留在视图内但不可编辑。需要一个统一的下发机制。

### 接口

**IEditor.h**：

```cpp
virtual void setReadOnly(bool readOnly) = 0;
```

### 各编辑器实现方向

| 编辑器 | 实现方式 |
|--------|---------|
| `TextEditorWidget` | `sci_editor_->setReadOnly(readOnly)`，QsciScintilla 原生支持 |
| `TopologyEditorWidget` | `view_->setInteractive(!readOnly)` + 禁用撤销栈 + 隐藏调色板 |
| `ProtocolEditorWidget` | `tree_->setEnabled(!readOnly)` + 位图视图设为只读 + 禁用属性面板编辑 |
| `TestProgramEditorWidget` | 禁用表格编辑、拖拽排序、添加/删除行按钮 |
| `ImageViewerWidget` | 本身只读，空实现即可 |
| `EtlogViewerWidget` | 本身只读，空实现即可 |

### 下发时机

在 ExecutionPanelController 的 `run()` / `stop()` 中，遍历 `editor_mgr_` 所有打开的编辑器调用 `setReadOnly`：

```cpp
// run() 中
editor_mgr_->forEachEditor([](IEditor* ed) { ed->setReadOnly(true); });

// stop() / engineFinished 中
editor_mgr_->forEachEditor([](IEditor* ed) { ed->setReadOnly(false); });
```

### 与 Phase 1 的关系

- Phase 1 是**物理隔离**（视图树级别），QStackedWidget 切页后编辑器不渲染
- Phase 2 是**逻辑隔离**（行为级别），编辑器在视图内但不可操作
- 两者共存时：切到 page 1 时编辑器既不可见也不可编辑；Page 1 执行仪表盘本身没有编辑器，所以 Phase 2 不会发生在 page 1 上
- 实际生效场景是 **page 0 内的编辑器**——如果未来在 page 0 内运行时（比如轻量预览），Phase 2 提供保护
