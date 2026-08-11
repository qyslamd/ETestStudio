# ProjectStructureWidget Fluent 化设计方案

## 1. 问题陈述

侧边栏「项目概述」页（ProjectStructureWidget）沿用旧式 QGroupBox + QListView 平铺
样式，与 WelcomeV2 / SettingsDialog 已确立的 Fluent 设计语言不一致。具体问题：

- 无项目占位页（模式 0）卡片 1 含 3×2 快捷按钮网格 `btn_grid`（新建项目 / 打开项目 /
  4 个快速新建文件按钮）。新建项目/打开项目/新建文件入口已由 Ribbon 文件菜单与
  WelcomeV2 启动页承载，侧边栏重复冗余。
- 最近项目 / 最近文件为 QListView 平铺，无层级，hover 反馈弱；且 default 主题缺少
  `QListView::item:hover` 规则，明暗主题 hover 表现不对称。
- 有项目页（模式 1）的项目树与「已打开」列表容器无统一卡片视觉。

### 目标

- 移除 `btn_grid`，卡片 1 简化为一行提示，告诉用户未打开项目及入口。
- 最近项目 / 最近文件保留，改为 Fluent **紧凑列表行**（Win11 侧边栏惯例：图标 +
  名称 + 次要信息一行内排，hover 浅灰圆角条、选中 accent 色，无卡片间隙）。
- 模式 0 / 模式 1 容器统一 Fluent 视觉，树逻辑不变。

## 2. 架构回顾（现状）

`src/app/ProjectStructureWidget.cpp`：

- `QStackedWidget` 双页：index 0 = 无项目占位页（page_default_），index 1 = 项目页
  （page_project_）。
- 模式 0 结构（可滚动）：
  - 卡片 1（QGroupBox「没有打开的项目」）：`ph_desc` 两行描述 + `btn_grid`（3×2：
    `new_proj_btn_`/`open_proj_btn_` + 最多 4 个 `PhQuickBtn`，复用 `defaultCategories()`）。
  - 卡片 3（QGroupBox「最近项目」）：QListView + `RecentProjOrFileDelegate`。
  - 卡片 4（QGroupBox「最近文件」）：QListView + `RecentProjOrFileDelegate`，固定高 200。
- 模式 1：垂直分割器 = 项目树（QTreeView + `ProjectTreeDelegate`）+ 「已打开」区
  （QListView + `RecentProjOrFileDelegate`）。
- `initSignals` 中 `PhQuickBtn` 走 `createStandaloneFile`/`createNewFile`；
  `new_proj_btn_`/`open_proj_btn_` 发射 `newProjectRequested`/`openProjectRequested`。
  `createStandaloneFile` 唯一调用点是 PhQuickBtn lambda，随按钮删除成为死方法。
- `RecentProjOrFileDelegate`（`src/app/widgets/RecentProjOrFileDelegate.cpp`）：paint 先
  `drawControl(CE_ItemViewItem)`（QSS ::item 生效），随后自绘一层半透明 hover 填充
  （约 42-49 行）。text 布局用 option.rect + 自有 8/4px 边距，与 QSS padding 不耦合。
- 样式：objectName 选择器（PhPlaceholder/PhDesc/PhProjectBtn/PhQuickBtn/PhSectionLabel
  等）。`PhSectionLabel` 在两主题 QSS 中均无样式（objectName 存在、规则全无）。
  `PhProjectBtn`/`PhQuickBtn` 选择器配的是 QPushButton（类型不匹配，规则从未生效）。

## 3. 目标设计

### 3.1 模式 0 占位页

| 组件 | 现状 | 目标 |
|---|---|---|
| 卡片 1 | QGroupBox「没有打开的项目」+ 两行描述 + btn_grid 3×2 | QFrame 空态卡（`PhEmptyCard`）：居中图标（folder SVG）+ 主文案「没有打开任何项目」+ 副文案「通过新建项目或欢迎页开始」 |
| 最近项目 | QGroupBox + QListView 平铺 | 分区标题（`PhSectionLabel`，细分隔线）+ Fluent 紧凑列表行 |
| 最近文件 | QGroupBox + QListView 平铺 | 同上 |

移除项：`btn_grid`、`new_proj_btn_`、`open_proj_btn_`、`PhQuickBtn` 及对应信号连接、
`createStandaloneFile` 死方法、`PhDesc`/`PhProjectBtn`/`PhQuickBtn` QSS 块。

### 3.2 模式 1 项目页

- 项目树与「已打开」区统一 Fluent 视觉（白底圆角容器、行级 hover/选中圆角条）。
- **树结构、ProjectTreeDelegate 绘制、右键菜单、拖拽、根节点操作按钮逻辑均不变**。

### 3.3 样式（QSS，default.qss / vscode.qss 对称）

- 新增（类型限定选择器，与现状约定一致）：
  - `QFrame#PhEmptyCard`、`QLabel#PhEmptyTitle`、`QLabel#PhEmptyDesc`：空态卡。
  - `QLabel#PhSectionLabel`：分区标题（全新规则，两主题均无现状）。
- 全局 QListView 样式（各主题一套，不特殊处理）：default（白）/ vscode（深）各自
  新增统一 `QListView` + `QListView::item`/`:hover`/`:selected`（padding 5px 8px、
  圆角 6px、hover/选中色随主题），覆盖最近项目/最近文件/已打开及所有 QListView；
  不再使用 `QListView#PhRecentList` 特殊选择器。
- 删除：`PhDesc`、`PhProjectBtn`、`PhQuickBtn` 相关块。
- 列表 hover 统一走 QSS；**删除 RecentProjOrFileDelegate 自绘半透明 hover 填充**（约
  RecentProjOrFileDelegate.cpp:42-49），避免双重高亮。
- 列表项图标：model 项 `setIcon`（最近项目 folder、最近文件/已打开 `file_generic`，
  经 AppIconProvider）；RecentProjOrFileDelegate 文本起点按 `initStyleOption` 后的
  `opt.features` 的 `HasDecoration` 偏移 `decorationSize.width() + spacing` 避开图标
  （仅此一处文本布局改动，关闭按钮与其余绘制不动）。
- 列表项为两行布局（第 1 行名称粗体、第 2 行路径小一号），
  `RecentProjOrFileDelegate` 经 `setShowTime` 控制右侧时间显示（最近项目开、
  其余关）、`setCloseButtonVisible` 控制 hover 关闭按钮（已打开开、其余关）。
- 占位页容器 `QWidget#PhPlaceholder` 加 `border-radius: 8px`，与 PhEmptyCard 圆角呼应。
- 项目树 hover/选中沿用 ProjectTreeDelegate 现状（不新增 QTreeView::item 规则）。
- 空态图标经 `AppIconProvider` 加载 SVG，不硬编码颜色。

## 4. 方案选项对比

### 方案 A：保留 QListView + RecentProjOrFileDelegate，紧凑列表行走 QSS + delegate 微调（推荐）

- 数据与 delegate 绘制逻辑基本不动，外观走 QSS（行高、hover、选中）+ 删 delegate
  自绘 hover。
- 优点：改动小、风险低，delegate 的关闭按钮/右键/拖拽全部保留；hover 统一由对称
  QSS 提供。
- 缺点：行高需在 delegate `sizeHint` 微调，非纯 QSS。

### 方案 B：换用自绘卡片控件

- 每项独立 QWidget 卡片堆叠。视觉最重但改动大，与 RecentProjOrFileDelegate 职责重叠，
  且偏离 Win11 紧凑列表惯例。

### 决策

- 采用**方案 A**，列表形态为紧凑列表行（用户确认）。
- 空态卡为独立 QFrame；btn_grid 移除；分区标题与列表 hover 为全新对称 QSS。

## 5. 决策记录

| 编号 | 决策 | 理由 |
|---|---|---|
| D1 | 移除 btn_grid（新建项目/打开项目/快速新建按钮） | 入口已由 Ribbon 文件菜单与 WelcomeV2 承载，侧边栏冗余 |
| D2 | 卡片 1 简化为空态提示（QFrame：图标 + 主文案 + 副文案） | 占位页职责从"操作入口"降为"状态提示" |
| D3 | 最近项目/最近文件保留，Fluent 紧凑列表行 | 侧边栏常驻可快速打开最近项；紧凑行符合 Win11 惯例、窄栏省空间 |
| D4 | 模式 1 一并 Fluent 化，仅容器/视觉，树逻辑不动 | 统一视觉语言，降低回归风险 |
| D5 | 保留 QListView + RecentProjOrFileDelegate，外观走 QSS + 删 delegate 自绘 hover | 改动最小；QSS ::item:hover 经 drawControl 生效，统一由 QSS 提供 hover 避免双重 |
| D6 | 空态图标经 AppIconProvider 加载 SVG | 主题自适应，禁 emoji/QSS 硬编码 |
| D7 | `newProjectRequested`/`openProjectRequested` 信号保留声明，但移除占位页按钮发射源后 MainWindow 对它们的连接为死连接，一并删除 | 功能入口在 Ribbon/Welcome，无缺失；删除死连接防误导 |
| D8 | `createStandaloneFile` 死方法、`PhDesc`/`PhProjectBtn`/`PhQuickBtn` QSS 块一并删除 | 随 btn_grid 移除成为死代码 |
| D9 | 最近项目/文件为空时整节隐藏（分区标题 + 列表） | 空分节显示空框观感差 |

## 6. 影响范围与实现要点

- `src/app/ProjectStructureWidget.cpp/.h`：
  - 移除 btn_grid 相关（new_proj_btn_/open_proj_btn_ 成员与声明、PhQuickBtn 遍历连接、
    `<QGridLayout>`/`<QPushButton>` include、QPushButton 前置声明）。
  - 删除 `createStandaloneFile` 声明与定义及其专属 include。
  - 卡片 1 重构为空态卡（`PhEmptyCard`/`PhEmptyTitle`/`PhEmptyDesc`）。
  - 列表容器不设特殊 objectName（全局 `QListView` 规则统一覆盖）。
  - model 项 `setIcon`（最近项目 folder、最近文件/已打开 `file_generic`）。
  - 空分节整节隐藏（refactor refreshRecentProjects/refreshRecentFiles 的显隐逻辑）。
- `src/app/widgets/RecentProjOrFileDelegate.cpp`：删除自绘半透明 hover 填充（约 42-49 行）；
  文本起点按 `HasDecoration` 偏移图标宽度；如行高不足则 `sizeHint` 微调。
- `src/app/MainWindow.cpp`：删除对 `newProjectRequested`/`openProjectRequested` 的死
  connect（约 539-542），确认 Welcome/Ribbon 入口不受影响。
- `src/app/resources/styles/default.qss` + `vscode.qss`：新增/删除上述选择器块。
- 无新依赖，无模块边界变化。

## 7. 验收标准

1. 未打开项目时，侧边栏占位页仅显示：空态提示（图标 + 「没有打开任何项目」）+ 最近
   项目紧凑列表 + 最近文件紧凑列表，无任何按钮网格。
2. 最近项目/最近文件点击、右键菜单（打开/复制路径/打开所在目录/移除）功能不变；
   列表为空时整节隐藏。
3. 打开项目后，项目树与「已打开」列表 Fluent 容器显示正常，树操作不变。
4. hover/选中高亮在明暗主题（default + vscode）下均可见且一致，无双重高亮。
5. 空态图标随主题切换正确显示，无 emoji。
6. Windows 与 WSL Linux 编译通过。
