# SignalTreePanel 迁出 ExecutionDashboard 方案

## 问题

ExecutionDashboard 三列布局中，SignalTreePanel 固定占据 250px 左列，但仅在通道选择时短暂使用，大部分时间闲置，压缩了可视化区空间。

## 方案

移除 ExecutionDashboard 中的左列 SignalTreePanel，改为 Ribbon「执行」页「运行配置」panel 中的「通道选择」按钮，点击弹出 Modal QDialog 展示树。

## 改动清单

### 1. ExecutionPanelController — 接管 SignalTreePanel 所有权

- 新增成员：`SignalTreePanel* signal_tree_`、`QAction* act_select_channels_`
- 构造时创建 signal_tree_，parent = parent_widget_（MainWindow），确保不泄漏；Dialog 首次 layout->addWidget() 时自动 reparent 到 Dialog
- Dialog 关闭后不迁回，signal_tree_ 始终在 Dialog 父子树中；Controller 持 raw pointer 仅用于接口调用
- 构造时创建 act_select_channels_，文本「通道选择」，带图标
- 新增访问器：`signalTreePanel()`、`selectChannelsAction()`
- `setDashboard()` 中：所有 `dashboard_->signalTreePanel()->xxx` 改为 `signal_tree_->xxx`
- `refreshMonitorTree()`：`signal_tree_->setMonitorTree(...)`
- `clearProjectState()`：`signal_tree_->clearTree()`

### 2. ExecutionDashboard — 移除 SignalTreePanel

- 删除 `signal_tree_` 成员和 `signalTreePanel()` 访问器
- 删除 SignalTreePanel 创建和 addWidget
- 布局从 3 列变 2 列：VisualizationArea | ExecutionDebugWidget

### 3. MainWindow — Ribbon 按钮 + Dialog

-「程序选择」panel 改名 →「运行配置」
- act_select_channels_ 放入该 panel（programPopup 之后）
- connect：创建 modal QDialog，内含 signalTreePanel()
- Dialog 创建一次，反复 exec()

### 4. QSS / CMakeLists

- 不动（#SignalTree / #SignalTreeSearch 基于 objectName，无视 parent）

## 时序

| 时机 | 行为 |
|------|------|
| Controller 构造 | signal_tree_ 创建（空树） |
| 项目打开 → 拓扑加载 | syncProjectTopologies → signal_tree_->setMonitorTree(tree, activeChannels) — 数据就绪 |
| 用户点 ribbon「通道选择」 | dialog->exec() modal 打开，树已填好 |
| dialog 中勾选 checkbox | checkStateChanged → controller → subscribe / addVisualizer（dialog 背后实时更新） |
| dialog 关闭 | visualizer 已就绪 |
| 再次打开 dialog | exec() 重新 modal 打开，树保留上次勾选 |
| 项目关闭 | clearProjectState → signal_tree_->clearTree() |

## 决策记录

- [x] Dialog 尺寸：380×480
- [x]「运行配置」panel：仅放程序选择 popup + 通道选择 action
- [x] signal_tree_ parent 策略：初始 parent = nullptr，由 Dialog 自动 reparent（选项 A）
- [x] visualizerClosed lambda：改为 `signal_tree_->uncheckChannel(mi, ci)`，去掉 dashboard_ guard
- [x] Dialog 宿主：由 ExecutionPanelController 管理，新增 showChannelSelectionDialog()
- [x] refreshMonitorTree guard：`if (!mm || !signal_tree_) return;`，第二参数 `dashboard_` 三元兜底

## 待讨论

- [ ] 通道选择是否需要搜索框默认获得焦点
- [ ] 是否支持多选提示（如标题显示「已选 N 个通道」）
