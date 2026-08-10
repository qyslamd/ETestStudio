# Welcome 启动页重设计方案

## 问题陈述

当前 Welcome 页是一个可拖拽的网格仪表盘（自研 `grid/` 子系统，仅本页使用）：
渐变磁贴、眼睛小部件、时钟、背景图、网格叠加层、拖拽预览等装饰花哨但不专业，
且缺乏引导性——没有醒目主操作、没有快速新建入口、没有入门指引。

目标：改为 VS Code / JetBrains 风格的专业启动页。同时**保留旧版作为可切换版本**：
容器 `WelcomeWidget` 承载业务，旧网格仪表盘迁入 `welcome/v1`，新启动页在
`welcome/v2`，通过配置即时切换。

## 架构回顾

- `WelcomeWidget`（`src/app/WelcomeWidget.cpp`）：QWidget，paintEvent 画背景图 +
  网格叠加层；initUi 用 `grid::GridLayout`/`grid::GridTile` 铺磁贴（新建项目 1x1、
  打开项目 1x1、每日提示 1x2、最近项目 1x2、眼睛 2x2、时钟 2x2）；`showRandomTip()`
  随机提示；拖拽/丢弃事件。对外信号 `newProjectRequested()`/`openProjectRequested()`，
  MainWindow 已连接；另有 `refreshRecentProjects()`（MainWindow 可能调用）。
- `grid/` 子系统：`GridLayout`、`GridTile`、`layout_calculator_v1/v3/base`、
  `grid_global_def.hpp` 等，仅 Welcome 使用。
- `widgets/EyeWidget.*`、`widgets/PaintedClockWidget.*`：仅旧 Welcome 使用。
- 背景：`CONFIG_WELCOME_BG_DIR`/`CONFIG_WELCOME_BG_IMAGE`/`CONFIG_WELCOME_BG_MODE`
  配置驱动（目录随机选图或单图）。
- `RecentProjectCard`（`widgets/`）：最近项目卡片，带 `openRequested`/`removeRequested`。
- `NewFileGuideWizard` 已就绪；`ProjectStructureWidget::createNewFile` 已 public。
- 主题 QSS：WelcomeTile*/WelcomeAction*/WelcomeTip* 在 13 主题中。

## 方案选项及理由

### 保留旧版与容器架构
- **选项 A（选定）**：三层结构。`welcome/WelcomeWidget` 容器承载业务；旧网格仪表盘
  整体迁入 `welcome/v1`（`WelcomeV1Widget`）；新启动页在 `welcome/v2`
  （`WelcomeV2Widget`）；`CONFIG_WELCOME_VERSION`（默认 `v2`）经容器 QStackedWidget
  即时切换。
  理由：保留旧版可随时切回，容器统一对外契约（信号/刷新入口），v1 代码改动最小
  （仅挪目录+重命名+改 include）。
- 选项 B 直接删除旧版：不选（用户要保留）。

### grid 归属
- **选项 A（选定）**：`src/app/grid` 整体迁入 `src/app/welcome/v1/grid`（v1 自包含，
  可编译可切换回）。`src/app/grid` 从顶层消失。
- 理由：v1 网格仪表盘依赖 grid，迁入 v1 目录使其自包含，同时满足「顶层 grid 不要了」。

### v1/v2 切换
- **选项 A（选定）**：`CONFIG_WELCOME_VERSION`（`"v1"`/`"v2"`，默认 `"v2"`），
  容器 QStackedWidget 即时切换，改配置立即生效无需重启。

### 布局与引导区块
- **选项 A（选定）**：双栏启动页（头部品牌区 + 左栏「开始/最近项目」+ 右栏
  「快速新建/入门指南」+ 底部状态条）。快速新建三按钮直接派发到对应具体向导；
  入门指南 5 步静态流程。已由 HTML 原型确认视觉。

### 旧版装饰元素
- 眼睛、时钟、背景图、拖拽、网格叠加、每日提示磁贴——**保留在 v1**（v1 完整保留），
  但 v2 不采用。v2 的每日提示放底部状态条。

## 决策记录

1. 目录结构：
   ```
   src/app/welcome/
     WelcomeWidget.h/.cpp          // 容器：版本切换 + 对外信号 + 刷新入口
     v1/
       WelcomeV1Widget.h/.cpp      // 旧网格仪表盘（原 WelcomeWidget 整体搬移）
       grid/                       // grid 子系统迁入（自包含）
     v2/
       WelcomeV2Widget.h/.cpp      // 新双栏启动页
   ```
   `src/app/WelcomeWidget.*` 消失；`src/app/grid/` 消失（迁入 v1/grid）。
2. 容器 `WelcomeWidget` 职责：
   - 读 `CONFIG_WELCOME_VERSION`（默认 `v2`），QStackedWidget 内建 v1/v2，按配置
     setCurrentIndex 即时切换；监听 `ConfigManager::configChanged` 的
     `CONFIG_WELCOME_VERSION` 键热切换（审查 C1，与 v1 背景配置监听同模式）。
   - 对外信号：`newProjectRequested()`、`openProjectRequested()`、
     `createFileRequested(categoryId, extension, baseName)`、
     `projectOpenRequested(const QString& projectPath)`——全部转发自当前激活版本
     （审查 🔴B1：`projectOpenRequested` 为最近项目打开信号，MainWindow:759 已连接，
     遗漏会静默失效）。
   - 对外方法：`refreshRecentProjects()` 转发给**当前激活版本**（审查 🟡S1）；
     切换版本时对新激活版本补一次 refresh，保证数据新鲜。
   - MainWindow 仅依赖容器（改 include 为 `welcome/WelcomeWidget.h`，信号连接不变）。
3. v1 `WelcomeV1Widget`：原 WelcomeWidget 代码整体搬移，类名改 `WelcomeV1Widget`，
   保留自身业务（网格、背景、提示、最近磁贴、信号）；include 改
   `welcome/v1/...`；grid 迁入 `v1/grid` 后内部 include 不变，外部 include 改
   `welcome/v1/grid/grid_tile.h`。
4. v2 `WelcomeV2Widget`：新双栏启动页。结构：
   - 头部品牌区：logo + 「ETest 测试系统」+ 副标语 + 版本 + 背景切换
   - 左栏：开始（新建项目/打开项目 大按钮）；最近项目（复用 `RecentProjectCard`
     竖向列表，空态提示）
   - 右栏：快速新建（协议/拓扑/测试程序 三按钮）；入门指南（5 步静态文本）
   - 底部状态条：每日提示（点击换一条）+ 设置入口
   - 信号：`newProjectRequested`/`openProjectRequested`/`createFileRequested`
   - 背景：沿用 `CONFIG_WELCOME_BG_*` 配置（与 v1 同源）
5. 快速新建：三按钮 emit `createFileRequested`，MainWindow 连接后检查项目状态，
   未打开则提示「请先新建或打开项目」，已打开则 `project_structure_widget_->createNewFile`
   （映射：protocol→eprotox、topology→etopo、testprog→etprog）。
6. `EyeWidget`/`PaintedClockWidget`：v1 使用，**保留**在 `src/app/widgets/` 不删除。
7. 配置项：`src/core/config/ConfigDefs.h` 新增 `CONFIG_WELCOME_VERSION`
   （`"welcome/version"`）与默认值常量 `CONFIG_WELCOME_DEFAULT_VERSION`（`"v2"`），
   并在 ConfigManager 默认值注册（审查 🔴B2；现有 Welcome 背景三配置同文件，比照处理）。
8. QSS：v2 启动页新样式源以 `vscode.qss`（暗）/ `default.qss`（亮）为准，注入 13
   主题；5 个由 gen_themes.py 自动生成的主题（avocado/mocha/hermes/bright_yellow/cyan）
   改 vscode.qss 后重跑脚本传播，其余 6 个手动主题（chinese_red/gold/indigo/klein_blue/
   lime/rose_pink）同步补；v1 的 WelcomeTile* 样式保留（v1 仍用）。（审查 🟡S2）
9. v2 最近项目卡片点击 → 转发 `projectOpenRequested(path)`（非通用 `openProjectRequested`，
   审查 🔵C3）；快速新建三按钮的类型映射（protocol→eprotox 等）放 v2 内静态映射
   （审查 🔵C2）。

## 详细设计

### 文件与类

| 文件 | 说明 |
|------|------|
| 新增 `src/app/welcome/WelcomeWidget.h/.cpp` | 容器（版本切换 + 信号转发 + 刷新入口） |
| 新增 `src/app/welcome/v2/WelcomeV2Widget.h/.cpp` | 新双栏启动页 |
| 搬迁 `src/app/WelcomeWidget.*` → `welcome/v1/WelcomeV1Widget.*` | 旧仪表盘，类名改名 |
| 搬迁 `src/app/grid/` → `src/app/welcome/v1/grid/` | grid 子系统自包含 |
| 修改 `src/app/MainWindow.cpp` | include `welcome/WelcomeWidget.h`；连接 `createFileRequested`（项目门控） |
| 修改 `src/app/CMakeLists.txt` | 摘除旧路径，注册 welcome/ 下文件 |
| 修改主题 QSS（13 文件，经 inject 脚本） | v2 启动页样式 |

### 容器实现要点

```cpp
class WelcomeWidget : public QWidget {
  Q_OBJECT
 public:
  explicit WelcomeWidget(QWidget* parent = nullptr);
  void refreshRecentProjects();
 signals:
  void newProjectRequested();
  void openProjectRequested();
  void createFileRequested(const QString& categoryId, const QString& extension,
                           const QString& baseName);
 private:
  QStackedWidget* stack_;
  WelcomeV1Widget* v1_;
  WelcomeV2Widget* v2_;
};
```
- 构造时读 `CONFIG_WELCOME_VERSION`，`setCurrentIndex(v2 ? 1 : 0)`；
  监听配置变化（或提供一个 setter）即时切换。
- 转发：`connect(v1_, &WelcomeV1Widget::newProjectRequested, ...)` 等，统一到容器信号。

### 信号派发

```
v2 快速新建[协议] → createFileRequested("protocol","eprotox","新建协议文件")
  └─ MainWindow::onQuickCreateFile
       ├─ 项目未打开 → QMessageBox「请先新建或打开项目」
       └─ 已打开 → project_structure_widget_->createNewFile(cat, ext, base)
```

## 实施计划

按序执行，每步编译验证通过再进下一步。

- [ ] **步骤 1：新增配置项**
  - `src/core/config/ConfigDefs.h`：在 Welcome 背景三配置旁新增
    `CONFIG_WELCOME_VERSION = "welcome/version"`、
    `CONFIG_WELCOME_DEFAULT_VERSION = "v2"`；ConfigManager 默认值注册（比照
    `CONFIG_WELCOME_BG_*` 的注册方式）。

- [ ] **步骤 2：搬迁 v1（旧 Welcome + grid，纯搬移零行为改动）**
  - `git mv src/app/WelcomeWidget.h/.cpp → src/app/welcome/v1/WelcomeV1Widget.h/.cpp`
  - `git mv src/app/grid/ → src/app/welcome/v1/grid/`
  - 类名 `WelcomeWidget` → `WelcomeV1Widget`（头/源文件内全部引用）
  - include 改写：自 include `"WelcomeWidget.h"` → `"WelcomeV1Widget.h"`；
    3 处 `"grid/*.h"` → `"welcome/v1/grid/*.h"`（grid 内部裸 include 无需动）
  - `src/app/CMakeLists.txt`：移除 `WelcomeWidget.*` 与 `grid/` 8 条目；新增
    `welcome/v1/WelcomeV1Widget.*` 与 `welcome/v1/grid/` 8 条目
  - 编译验证：v1 编译通过（此时 MainWindow 仍 include 旧路径——临时让
    `welcome/v1` 可被 include，或先改 MainWindow include 指向 v1 过渡）

- [ ] **步骤 3：新增容器 WelcomeWidget**
  - 新建 `src/app/welcome/WelcomeWidget.h/.cpp`
    - QStackedWidget 内建 v1/v2 实例；读 `CONFIG_WELCOME_VERSION` setCurrentIndex
    - `connect(ConfigManager::configChanged)` 监听 `CONFIG_WELCOME_VERSION` 热切换，
      切换时对新激活版本补 `refreshRecentProjects()`
    - 转发信号：`newProjectRequested` / `openProjectRequested` /
      `createFileRequested(cat,ext,base)` / `projectOpenRequested(path)`
    - `refreshRecentProjects()` 转发当前激活版本
  - CMakeLists 注册 `welcome/WelcomeWidget.*`
  - MainWindow include 改 `"welcome/WelcomeWidget.h"`；编译验证容器 + v1 可用

- [ ] **步骤 4：实现 v2 启动页**
  - 新建 `src/app/welcome/v2/WelcomeV2Widget.h/.cpp`，按 HTML 原型
    （`docs/prototype/Welcome启动页设计.html`）：
    - 头部品牌区 + 版本 + 背景切换（`CONFIG_WELCOME_BG_*`）
    - 左栏：开始（新建项目/打开项目 大按钮）+ 最近项目（`RecentProjectCard`
      竖向列表，空态提示；卡片点击转发 `projectOpenRequested(path)`；× 移除在
      v2 内部处理——读写 `CONFIG_RECENT_PROJECT_LIST` 后刷新本列表，与 v1 现状一致，
      不占对外信号）
    - 右栏：快速新建三按钮（静态映射 protocol→eprotox 等，emit
      `createFileRequested`）+ 入门指南 5 步
    - 底部状态条：每日提示（换条）+ 设置入口
    - 对外信号：`newProjectRequested`/`openProjectRequested`/`createFileRequested`/
      `projectOpenRequested(path)`
  - CMakeLists 注册 `welcome/v2/WelcomeV2Widget.*`；编译验证

- [ ] **步骤 5：MainWindow 集成**
  - 连接容器 `createFileRequested` → `onQuickCreateFile`：项目未打开 →
    `QMessageBox` 提示；已打开 → `project_structure_widget_->createNewFile(cat, ext, base)`
  - 核对既有连接（`newProjectRequested`/`openProjectRequested`/
    `projectOpenRequested`/`refreshRecentProjects`）改走容器后仍生效
  - 编译验证

- [ ] **步骤 6：QSS（v2 启动页样式）**
  - 源：`src/app/resources/styles/vscode.qss`（暗）、`default.qss`（亮）加入 v2 选择器
    （`welcomeStartPage`、`welcomeHeader`、`welcomeSectionTitle`、
    `welcomePrimaryBtn`/`welcomeSecondaryBtn`、`welcomeQuickBtn`、`welcomeGuideStep`、
    `welcomeStatusBar` 等）
  - 重跑 `gen_themes.py` 传播 5 个自动生成主题；手动同步 6 个手动主题
    （chinese_red/gold/indigo/klein_blue/lime/rose_pink）
  - v1 的 WelcomeTile* 样式保留不动

- [ ] **步骤 7：全量编译 + 代码审查 + 手动验证**
  - `scripts/build_ninja.bat -t debug -m ETestStudio` 全量编译通过
  - 派 subagent 代码审查，修复至无阻塞
  - 手动（GUI）：默认 v2 显示正确；改 `CONFIG_WELCOME_VERSION=v1` 立即切回旧仪表盘
    且功能正常；v2 主操作/最近项目/快速新建（含未开项目提示）/入门指南/每日提示/
    背景切换全部验证；File 菜单「新建文件」不受影响

## 验证

1. `scripts/build_ninja.bat -t debug -m ETestStudio` 编译通过（无残留旧路径引用）。
2. 手动（GUI 需人工确认）：
   - 默认显示 v2 双栏启动页；`CONFIG_WELCOME_VERSION` 设为 `v1` 后立即切回旧仪表盘
   - v2：主操作可用、最近项目列表/空态正确、快速新建三按钮按项目状态派发、
     入门指南 5 步、每日提示换条、背景切换生效
   - v1：旧网格仪表盘功能正常（网格/眼睛/时钟/拖拽/背景）
   - File 菜单「新建文件」引导向导不受影响
