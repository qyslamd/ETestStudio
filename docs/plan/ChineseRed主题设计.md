# ChineseRed 中国红主题设计

## 问题陈述

当前系统有两个主题：`default`（浅色）和 `vscode`（深色）。需要新增一个"中国红"主题，以中国传统色彩为基调，提供有辨识度的深色界面风格。

## 现有主题系统架构

### 主题加载流程

```
SettingsDialog ComboBox → ConfigManager.set("appearance/theme")
    → ThemeManager.onConfigChanged()
    → ThemeManager.setTheme(themeId)
    → loadQss(themeId)          // 加载 :/resources/styles/<themeId>.qss
    → detectDarkFromQss(qss)    // 解析首个 background-color 的 luma < 0.4 → is_dark_
    → 若 is_dark_：追加 ads_dark.qss
    → qApp->setStyleSheet(qss)
    → applyEditorTheme()        // 按 is_dark_ 设 QScintilla 配色
    → emit themeChanged(is_dark_)
```

### 关键组件

| 组件 | 文件 | 职责 |
|------|------|------|
| ThemeManager | `src/core_ui/ThemeManager.cpp` | 单例，加载 QSS、检测明暗、切换主题 |
| QSS 文件 | `src/app/resources/styles/*.qss` | 样式定义，通过 qrc 嵌入 |
| resource.qrc | `src/app/resource.qrc` | 注册 QSS 文件到资源系统 |
| SettingsDialog | `src/app/dialogs/SettingsDialog.cpp` | 主题选择 ComboBox |
| AppIconProvider | `src/core_ui/AppIconProvider.cpp` | 按 isDarkTheme() 选 `_light.svg` / `_dark.svg` 图标 |

### 现有 QSS 文件

| 文件 | 行数 | 说明 |
|------|------|------|
| `default.qss` | 1208 | 浅色主题，大量使用 `palette()` 引用 |
| `vscode.qss` | 2173 | 深色 VS Code 风格，全硬编码颜色 |
| `ads_dark.qss` | 119 | QADS 停靠系统深色覆盖（dark 主题自动追加） |

### 暗色检测机制

`detectDarkFromQss()` 用正则提取 QSS 中首个 `background-color` 值，计算 luma（`0.2126R + 0.7152G + 0.0722B`），luma < 0.4 判定为暗色。

中国红主题背景为深色，luma 远低于 0.4，会被自动检测为暗色主题，无需额外处理。

### 编辑器配色

`applyEditorTheme()` 按 `is_dark_` 二分设置 QScintilla 的全部配色（paper、text、语法高亮等）。中国红主题作为暗色主题，会自动走 `is_dark_ = true` 分支，使用 VS Code Dark+ 配色。如果需要定制编辑器配色（如关键字用红色），需要扩展此函数。

## 设计决策

### 1. 色彩体系

以 `vscode.qss` 为基底，将蓝色系强调色替换为中国红/暖金色系：

| 语义 | vscode.qss | chinese_red.qss | 色名 |
|------|------------|-----------------|------|
| 主背景 | `#1E1E1E` | `#1A1A1A` | 墨黑 |
| 面板/侧栏背景 | `#252526` | `#241F20` | 暗赭 |
| 菜单栏/工具栏 | `#3C3C3C` | `#33292B` | 深褐红 |
| 悬停高亮 | `#505050` | `#4A3437` | 赭褐 |
| 选中/强调 | `#094771` | `#5D1A1A` | 绛红 |
| 边框/分隔线 | `#454545` | `#4A3A3C` | 暗褐 |
| 状态栏 | `#007ACC` | `#B71C1C` | 朱红 |
| 文字主色 | `#CCCCCC` | `#D4C5C5` | 暖白 |
| 次要文字 | `#858585` | `#9A8585` | 灰褐 |
| 禁用文字 | `#5A5A5A` | `#5A4A4A` | 暗灰褐 |
| 焦点边框 | `#007ACC` | `#C62828` | 中国红 |
| RunningMode 指示条 | `#ff8800` | `#D4AF37` | 金 |

### 2. 编辑器配色定制

在 `applyEditorTheme()` 中增加 `current_theme_` 判断，当主题为 `chinese_red` 时使用定制配色：

| 语法元素 | Dark+ (现) | ChineseRed (新) |
|----------|-----------|-----------------|
| 关键字 | `#569CD6` | `#C62828` 中国红 |
| 函数 | `#DCDCAA` | `#D4AF37` 金 |
| 标签 | `#569CD6` | `#C62828` 中国红 |
| 其他 | 保持不变 | 保持不变 |

### 3. 图标系统

`AppIconProvider` 按 `isDarkTheme()` 选择 `_light.svg` / `_dark.svg`。中国红主题 is_dark_ = true，走现有暗色图标路径，无需新增图标。

### 4. 实现方式选择

| 方案 | 说明 | 优劣 |
|------|------|------|
| **A. 复制 vscode.qss 修改** | 拷贝 vscode.qss → chinese_red.qss，全局替换颜色 | 简单直接，但 2000+ 行副本，后续维护需同步结构变更 |
| **B. 以 vscode.qss 为基底只记录差异** | QSS 不支持变量/继承，无法实现 | 不可行 |

选择 **方案 A**。QSS 不支持变量或继承，只能整文件拷贝后修改颜色。这是 Qt 样式系统的固有限制。

## 实施步骤

### 步骤 0：修复构造函数时序 Bug

在 `src/core_ui/ThemeManager.cpp` 构造函数中，把 `current_theme_ = theme;` 移到 `applyEditorTheme()` 之前：

```cpp
ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
  auto& cfg = ConfigManager::instance();
  QString theme =
      cfg.get<QString>(CONFIG_APPEARANCE_THEME,
                       QString::fromLatin1(CONFIG_APPEARANCE_DEFAULT_THEME));

  loadQss(theme);
  current_theme_ = theme;       // ← 移到此处
  applyEditorTheme();

  connect(&cfg, &ConfigManager::configChanged, this,
          &ThemeManager::onConfigChanged);
}
```

### 步骤 1：定义语义色板 API

在 `ThemeManager.h` 中增加语义颜色 getter 声明，在 `ThemeManager.cpp` 中按主题返回对应色值。色板定义见上方"语义色板设计"表。

### 步骤 2：创建 QSS 文件

- 复制 `vscode.qss` -> `src/app/resources/styles/chinese_red.qss`
- 按色彩体系表替换所有颜色值
- 保持所有选择器结构不变，只改颜色

### 步骤 3：注册资源

在 `src/app/resource.qrc` 中添加：

```xml
<file>resources/styles/chinese_red.qss</file>
```

### 步骤 4：设置对话框添加选项

在 `src/app/dialogs/SettingsDialog.cpp` 主题 ComboBox 添加：

```cpp
combo->addItem(QStringLiteral("中国红"), QStringLiteral("chinese_red"));
```

### 步骤 5：编辑器配色定制

在 `src/core_ui/ThemeManager.cpp` 的 `applyEditorTheme()` 中，增加 `chinese_red` 分支。同时清理浅色分支中 7 处重复设值（见注意事项 8）：

```cpp
void ThemeManager::applyEditorTheme() {
  auto& cfg = ConfigManager::instance();
  if (current_theme_ == QStringLiteral("chinese_red")) {
    // 基于 Dark+ 配色，替换关键字/函数/标签为红金色调
    // ... 完整配色设置
  } else if (is_dark_) {
    // 现有 Dark+ 配色
  } else {
    // 现有 Light+ 配色（清理重复行）
  }
}
```

### 步骤 6：迁移 QPainter 组件

逐个将 20+ 组件中的 `isDark ? #xxx : #yyy` 替换为 `ThemeManager::instance().xxxColor()`。可渐进式迁移，未迁移的组件暂时走旧逻辑不影响编译。

### 步骤 7：编译验证

```bash
scripts/build_ninja.bat -t debug -m ETestStudio
```

## 改动文件清单

| 文件 | 改动类型 | 说明 |
|------|----------|------|
| `src/core_ui/ThemeManager.h` | 修改 | 增加语义色板 getter 声明 |
| `src/core_ui/ThemeManager.cpp` | 修改 | 修复构造函数时序 + 实现色板 + `applyEditorTheme()` 增加分支 |
| `src/app/resources/styles/chinese_red.qss` | 新增 | 中国红主题样式（~2173 行） |
| `src/app/resource.qrc` | 修改 | 注册新 QSS 文件 |
| `src/app/dialogs/SettingsDialog.cpp` | 修改 | ComboBox 添加选项 |
| 20+ QPainter 组件文件 | 修改 | 逐步迁移到语义色板 API |

## 风险与注意事项

1. **QSS 维护成本**：chinese_red.qss 是 vscode.qss 的完整副本，后续 QSS 结构变更（新增选择器）需同步到两个文件
2. **暗色检测**：首个 `background-color` 必须是深色值，否则 `detectDarkFromQss` 误判
3. **ads_dark.qss**：暗色主题自动追加，中国红无需额外处理 QADS 样式
4. **RunningMode**：ribbon 执行页底部指示条颜色从橙色改为金色，需确认视觉效果

## 审查问题记录

> 以下为 subagent 审查发现的问题，需在实施前逐项确认解决方案。

### 🔴 阻塞问题 1：构造函数 `current_theme_` 时序 Bug

`ThemeManager` 构造函数 (`src/core_ui/ThemeManager.cpp:22-30`) 调用顺序为：

```cpp
loadQss(theme);         // 设置 is_dark_
applyEditorTheme();     // ← current_theme_ 此时仍为空字符串 ""
current_theme_ = theme; // 赋值在 applyEditorTheme() 之后
```

文档步骤 4 的 `applyEditorTheme()` 依赖 `current_theme_ == "chinese_red"` 判断。当用户配置中保存了 chinese_red 并重启程序时，构造函数中 `applyEditorTheme()` 被调用时 `current_theme_` 还是 `""`，会落入 `is_dark_` 分支，应用 VS Code Dark+ 配色而非中国红配色。

对比 `setTheme()` (`:64-76`) 中 `current_theme_ = themeId` 在 `loadQss` 之前，运行时切换没问题。仅构造函数有此 Bug。

**修复选项**：

| 选项 | 说明 |
|------|------|
| A. 调整构造函数赋值顺序 | 把 `current_theme_ = theme;` 移到 `applyEditorTheme()` 之前，与 `setTheme()` 中的顺序一致 |
| B. 给 `applyEditorTheme()` 加参数 | 改签名为 `applyEditorTheme(const QString& themeId)`，构造时传 theme 进去 |

**决策：选项 A**。理由：改动最小（移一行），与 `setTheme()` 中已有的顺序（先赋值 `current_theme_` 再调 `applyEditorTheme`）保持一致，不引入新接口。

### 🔴 阻塞问题 2：QPainter 硬编码颜色全面遗漏

文档改动清单只列了 4 个文件，但代码库中有 20+ 个组件通过 QPainter + `isDarkTheme()` 二分法直接绘制颜色，不受 QSS 控制。中国红 `is_dark_ = true` 会走 dark 分支，但这些 dark 分支全是 VS Code 色值（`#1E1E1E`、`#252526`、`#858585`、`#CCCCCC` 等），不是中国红色值（`#1A1A1A`、`#241F20`、`#9A8585`、`#D4C5C5` 等）。

**后果**：QSS 控制的元素显示中国红配色，QPainter 绘制的元素显示 VS Code 暗色配色，同一界面两套色系并存。

**修复选项**：

| 选项 | 说明 | 优缺点 |
|------|------|--------|
| A. 全量适配 | 在每个组件中加 `currentTheme() == "chinese_red"` 第三分支 | 精确控制，但 20+ 文件改动，且每加一个新主题就要改一遍所有组件，不可扩展 |
| B. 分阶段实施 | 本期只做 QSS + 编辑器配色，QPainter 组件留第二阶段 | 快速出成果，但两套色系并存期间视觉割裂 |
| C. 引入颜色间接层 | 在 ThemeManager 中增加语义色板 API（`panelBg()`、`accentColor()`、`textColor()` 等），组件查询而非硬编码 | 架构最优，一次重构永久受益；但工作量大，需定义色板 + 重构 20+ 组件 + 为现有主题提供默认值 |

**决策：选项 C**。理由：
1. **可扩展性**：以后再加新主题只需在 ThemeManager 加一套色板，不用动任何组件代码
2. **根治问题**：选项 A 不可扩展（每加主题改 20+ 文件），选项 B 只是推迟问题
3. **改动虽多但模式统一**：每个组件的改法相同--把 `isDark ? #xxx : #yyy` 替换为 `ThemeManager::instance().xxxColor()`，机械且可验证
4. **渐进式可行**：可以先定义色板 API + 中国红色板 + QSS，然后逐个组件迁移，未迁移的组件暂时走旧逻辑不影响编译

#### 语义色板设计

在 `ThemeManager` 中定义语义颜色角色，每个角色按当前主题返回对应色值：

| 角色 | getter | default (浅色) | vscode (暗色) | chinese_red |
|------|--------|----------------|---------------|-------------|
| 主背景 | `windowBackground()` | `palette(window)` | `#1E1E1E` | `#1A1A1A` |
| 面板背景 | `panelBackground()` | `palette(window)` | `#252526` | `#241F20` |
| 工具栏背景 | `toolbarBackground()` | `palette(window)` | `#3C3C3C` | `#33292B` |
| 悬停高亮 | `hoverBackground()` | `palette(midlight)` | `#505050` | `#4A3437` |
| 选中/强调 | `selectionBackground()` | `palette(highlight)` | `#094771` | `#5D1A1A` |
| 边框 | `borderColor()` | `palette(mid)` | `#454545` | `#4A3A3C` |
| 文字主色 | `textColor()` | `palette(text)` | `#CCCCCC` | `#D4C5C5` |
| 次要文字 | `secondaryTextColor()` | `palette(text)` | `#858585` | `#9A8585` |
| 禁用文字 | `disabledTextColor()` | `palette(text)` | `#5A5A5A` | `#5A4A4A` |
| 强调色 | `accentColor()` | `#007ACC` | `#007ACC` | `#C62828` |
| 状态栏背景 | `statusBarBackground()` | `palette(window)` | `#007ACC` | `#B71C1C` |
| 时钟表盘背景 | `clockFaceBackground()` | `#F6F6F6` | `#252526` | `#241F20` |
| 时钟主色 | `clockHandColor()` | `#333333` | `#CCCCCC` | `#D4C5C5` |
| 时钟次要色 | `clockSecondaryColor()` | `#555555` | `#858585` | `#9A8585` |
| 时钟强调色 | `clockAccentColor()` | `#FF6600` | `#FF6600` | `#D4AF37` |

组件迁移模式：

```cpp
// 旧代码
bool dark = ThemeManager::instance().isDarkTheme();
painter->setPen(dark ? QColor("#CCCCCC") : QColor("#333333"));

// 新代码
painter->setPen(ThemeManager::instance().textColor());
```

#### 受影响组件清单

| # | 组件 | 文件 | 问题详情 |
|---|------|------|----------|
| 1 | `HintMessageDelegate::paint` | `src/app/widgets/HintMessageDelegate.cpp:52-128` | 9 处 `isDark?` 硬编码颜色，dark 分支全为 VS Code 色值 |
| 2 | `WaveformWidget::applyTheme` | `src/app/visualizers/WaveformWidget.cpp:215-239` | 6 处 dark 分支硬编码 (`#252526`/`#1E1E1E`/`#AAAAAA` 等) |
| 3 | `TopologyTheme::topologyColors` | `src/topology/TopologyTheme.cpp:38-58` | dark 配色集为 VS Code 风格，拓扑编辑器整体配色不匹配 |
| 4 | `ImageViewerWidget` | `src/app/editors/ImageViewerWidget.cpp:27-29,93-95` | dark 背景硬编码 `QColor(60,60,60)`，非中国红 `#1A1A1A` |
| 5 | `EtlogViewerWidget::colorForStatus` | `src/app/editors/EtlogViewerWidget.cpp:662-669` | 6 处 dark 分支状态色 |
| 6 | `LoadingOverlay::paint` | `src/app/widgets/LoadingOverlay.cpp:90-126` | dark 背景硬编码 `QColor(30,30,46)`，蓝色圆圈 `(100,140,255)` |
| 7 | `WelcomeWidget` | `src/app/WelcomeWidget.cpp:97-111,247-254` | 网格高亮硬编码蓝色 `QColor(0,120,215)`，EyeWidget 颜色 dark 分支 |
| 8 | `OpenFileDelegate::paint` | `src/app/widgets/OpenFileDelegate.cpp:43-117` | dark 分支硬编码 hover 色 |
| 9 | `IcdBitLayoutView` | `src/protocol/IcdBitLayoutView.cpp:329,596,805,935,1056,1075,1111` | 7 处 `isDarkTheme()` 分支 |
| 10 | `TabBarStyle` / `DockAreaTabBarStyle` | `src/libui/styles/TabBarStyle.cpp` | dark 分支硬编码 |
| 11 | `MainWindow` | `src/app/MainWindow.cpp:1446,2226-2228,2547` | 多处 `isDark?` 硬编码颜色 |
| 12 | `ProjectStructureWidget` | `src/app/ProjectStructureWidget.cpp:575,630,1370-1380` | 多处硬编码前景色 |
| 13 | `ProtocolManagerWidget` | `src/app/ProtocolManagerWidget.cpp:612,646` | 硬编码灰色 |
| 14 | `ExecutionPanelController` | `src/app/ExecutionPanelController.cpp:1041-1045` | 8 色通道色谱（**功能性颜色，不迁移**） |
| 15 | `LogFilterBar` | `src/app/widgets/LogFilterBar.cpp:73-75,163` | dark 分支硬编码 `#858585` |
| 16 | `DockTitleBar` | `src/libui/dock_title_bar/DockTitleBar.cpp` | dark 分支硬编码 |
| 17 | `SignalTreePanel` | `src/app/SignalTreePanel.cpp` | dark 分支硬编码 |
| 18 | `StateLEDWidget` | `src/app/visualizers/StateLEDWidget.cpp` | dark 分支硬编码 |
| 19 | `DigitalMeterWidget` | `src/app/visualizers/DigitalMeterWidget.cpp` | dark 分支硬编码 |
| 20 | `VisualizationArea` | `src/app/VisualizationArea.cpp` | dark 分支硬编码 |
| 21 | `WisdomWidget` | `src/app/widgets/WisdomWidget.cpp:34-42` | 已有暗色配色含朱砂红 accent，需对齐色值 |

#### 完全无主题感知的组件（不区分明暗，固定颜色）

| # | 组件 | 文件 | 问题详情 |
|---|------|------|----------|
| 21 | `modern_clock_renderer` | `src/app/widgets/modern_clock_renderer.cpp` | 固定浅色绘制 (`#F6F6F6` 背景 / `#333333` 文字)，完全不感知主题 |
| 22 | `minimal_clock_renderer` | `src/app/widgets/minimal_clock_renderer.cpp` | 固定白色绘制，完全不感知主题 |
| 23 | `EphQtSwitchButton::paintButton` | `src/app/utils/eph_qt_switch_button.cpp:125-204` | 7 处硬编码渐变色，完全不感知主题 |

### 🟡 注意事项 1：`detectDarkFromQss` 依赖首个 `background-color`

`detectDarkFromQss()` 用正则提取 QSS 中**首个** `background-color` 值计算 luma。`chinese_red.qss` 必须确保首个 `background-color` 出现在 `QMainWindow` 选择器中且为深色值，否则会误判为浅色主题，导致不追加 `ads_dark.qss`、图标走浅色路径、编辑器配色走 Light+ 分支。

**修复选项**：

| 选项 | 说明 |
|------|------|
| A. 确保 QSS 文件以 QMainWindow 开头 | 复制 vscode.qss 后保持首个选择器为 `QMainWindow { background-color: #1A1A1A; }`，与 vscode.qss 结构一致 |
| B. 改用更可靠的检测方式 | 在 QSS 文件头加注释标记 `/* @theme: dark */`，改 detectDarkFromQss 解析标记 |

**决策：选项 A**。理由：vscode.qss 已经是 QMainWindow 开头，复制后保持即可，零额外工作量。选项 B 需改 ThemeManager 检测逻辑，且需回溯现有 QSS 文件加标记，投入产出不成比例。

### 🟡 注意事项 2：`modern_clock_renderer` / `minimal_clock_renderer` 完全无主题感知

这两个时钟渲染器固定使用浅色绘制（`#F6F6F6` 背景、`#333333` 文字），不调用 `isDarkTheme()`。在中国红深色主题下会显示为浅色方块，与周围深色界面不协调。需要增加主题感知逻辑。

**修复选项**：

| 选项 | 说明 |
|------|------|
| A. 本期增加主题感知 | 用语义色板 API 替换硬编码颜色 |
| B. 留到后续 | 本期不处理 |

**决策：选项 A**。理由：既然已经引入语义色板 API，迁移成本极低（每个渲染器就几行颜色替换）。不修的话深色主题下时钟是浅色方块，视觉割裂非常明显。

### 🟡 注意事项 3：`EphQtSwitchButton` 完全无主题感知

使能/禁用开关按钮固定使用绿色/红色渐变，不感知主题。虽然功能上可接受，但在视觉上可能与中国红色调不协调。

**修复选项**：

| 选项 | 说明 |
|------|------|
| A. 本期增加主题感知 | 按主题调整按钮配色 |
| B. 留到后续 | 保持现有语义色不变 |

**决策：选项 B**。理由：绿色=使能、红色=禁用是功能语义色，类似交通灯--绿色始终表示"开"，跨主题保持一致反而更利于辨识。强行跟随主题变色会降低可用性。

### 🟡 注意事项 4：状态栏子控件颜色

`vscode.qss` 状态栏为蓝色 `#007ACC`，中国红改为朱红 `#B71C1C`。但 `AppStatusBarController` 管理的状态栏子控件（如进度条、状态标签等）是否有硬编码颜色需要检查。

**修复选项**：

| 选项 | 说明 |
|------|------|
| A. 本期检查并迁移 | 排查 AppStatusBarController 中的硬编码颜色，用语义色板替换 |
| B. 留到后续，先看实际效果 | 先不处理，上线后根据视觉反馈决定 |

**决策：选项 A**。理由：状态栏是视觉焦点区域，颜色不匹配很显眼。AppStatusBarController 颜色点不多，迁移成本低，顺手做了避免返工。

### 🟡 注意事项 5：`ads_dark.qss` 是否足够

中国红主题自动追加 `ads_dark.qss`，但该文件是为 VS Code 暗色主题设计的。其中硬编码的颜色（如 `#252526`、`#3C3C3C`）与中国红色系（`#241F20`、`#33292B`）不完全匹配。是否需要定制 `ads_chinese_red.qss`？

**修复选项**：

| 选项 | 说明 |
|------|------|
| A. 定制 `ads_chinese_red.qss` | 为中国红单独创建 QADS 样式文件 |
| B. 接受 `ads_dark.qss` 的色值差异 | 共用现有文件 |

**决策：选项 B**。理由：色差极小（`#252526` vs `#241F20`，`#3C3C3C` vs `#33292B`），QADS 停靠标题栏等区域面积小，实际视觉差异肉眼难辨。新增文件增加维护成本，性价比低。如果后续实际效果不满意再定制也不迟。

## 二次审查发现

### 🟡 注意事项 6：`AppStatusBarController` 无硬编码颜色，无需迁移

经读取 `src/app/AppStatusBarController.cpp` 全文（116 行），该控制器只创建 QLabel 并设置文本，**无任何硬编码颜色**，所有样式由 QSS 控制。注意事项 4 的决策"本期检查并迁移"实际无需改动任何代码。状态栏子控件颜色完全由 QSS 驱动，切换主题后自动生效。

**决策：无需改动**。注意事项 4 的迁移目标已满足，零代码变更。

### 🟡 注意事项 7：`ExecutionPanelController` 8 色通道色谱为功能性颜色，不应迁移

经读取 `src/app/ExecutionPanelController.cpp:1041-1046`，8 色通道色谱定义如下：

```cpp
static const QColor kColors[] = {
    QColor(0, 120, 215),   QColor(229, 57, 53),
    QColor(67, 160, 71),   QColor(255, 152, 0),
    QColor(156, 39, 176),  QColor(0, 151, 167),
    QColor(121, 85, 72),   QColor(158, 158, 158)};
```

这是波形图中区分不同监控通道的**数据可视化颜色**（类似 chart.js 的调色板），不是主题语义色。跨主题保持一致才能确保通道辨识度。组件清单中 #14 应从 QPainter 迁移范围中**排除**。

**决策：不迁移**。通道色谱为功能性数据可视化颜色，跨主题保持一致。

### 🟡 注意事项 8：`applyEditorTheme()` 浅色分支有重复设值

经读取 `src/core_ui/ThemeManager.cpp:176-242`，浅色分支（`else`）中以下配置项被重复设置了两次：

- `CONFIG_EDITOR_THEME_LINE_NUMBER`（行 192 和行 206）
- `CONFIG_EDITOR_THEME_INDENT_GUIDE`（行 194 和行 208）
- `CONFIG_EDITOR_THEME_BRACE_LIGHT_BG`（行 196 和行 210）
- `CONFIG_EDITOR_THEME_BRACE_LIGHT_FG`（行 198 和行 212）
- `CONFIG_EDITOR_THEME_BRACE_BAD_BG`（行 200 和行 214）
- `CONFIG_EDITOR_THEME_BRACE_BAD_FG`（行 202 和行 216）
- `CONFIG_EDITOR_THEME_FOLD_MARGIN`（行 204 和行 218）

这是预存 Bug，不影响功能但应清理。在步骤 5 改 `applyEditorTheme()` 时顺带删除重复行。

**决策：本期清理**。步骤 5 修改 `applyEditorTheme()` 时删除浅色分支中 7 处重复设值。

### 🟡 注意事项 9：语义色板需补充时钟专用角色

经读取 `modern_clock_renderer.cpp` 全文（173 行），该渲染器使用以下颜色：

| 现有硬编码 | 用途 | 建议语义角色 |
|-----------|------|-------------|
| `QColor(0xF6, 0xF6, 0xF6)` | 表盘背景 | `clockFaceBackground()` |
| `QColor(0x33, 0x33, 0x33)` | 边框/刻度/指针/中心点（5 处） | `clockHandColor()` |
| `QColor(0x55, 0x55, 0x55)` | 次要刻度/数字/日期（5 处） | `clockSecondaryColor()` |
| `QColor(0xFF, 0x66, 0x00)` | 秒针 | `clockAccentColor()` |
| `Qt::white` | 数字时间文字 | 复用 `clockFaceBackground()` 或 `textColor()` |

`minimal_clock_renderer.cpp`（84 行）全部使用半透明白色（`QColor(255, 255, 255, 200/220)`），设计为深色背景上的白色时钟。迁移时可复用 `clockHandColor()` + 透明度。

**决策：补充 4 个时钟角色到语义色板**。色板表已更新（见上方"语义色板设计"）。数字时间文字复用 `textColor()`，不单独设角色。

### 🔵 建议优化：`WisdomWidget` 已有类似中国红的配色

`src/app/widgets/WisdomWidget.cpp:34-42` 中，暗色模式已使用 `QColor(0xC4, 0x3D, 0x3D)`（朱砂红）作为 accent 色。中国红主题的 accent 色 `#C62828` 与之接近但不同。建议统一两者色值，或确认差异是有意为之。组件清单中未列出 WisdomWidget，应补充。

**决策：本期对齐**。将 WisdomWidget 暗色模式的 accent 从 `#C43D3D` 统一为 `#C62828`，与中国红主题 accent 一致。组件清单已补充为 #21。

## 待决策项

### 决策 1：QPainter 组件适配策略

**已决策：选项 C--引入颜色间接层**。在 ThemeManager 中增加语义色板 API，组件查询而非硬编码。详见上方"语义色板设计"。

### 决策 2：无主题感知组件处理

**已决策**：
- `modern_clock_renderer` / `minimal_clock_renderer`：**本期增加主题感知**（注意事项 2，选项 A），用语义色板 API 替换硬编码颜色。
- `EphQtSwitchButton`：**留到后续**（注意事项 3，选项 B），绿色/红色为功能语义色，跨主题保持一致。

### 决策 3：`ads_dark.qss` 定制

**已决策：选项 B--接受 `ads_dark.qss` 的色值差异**（注意事项 5）。色差极小，共用现有文件，后续不满意再定制。

### 决策 4：二次审查注意事项

**全部已决策**：
- 注意事项 6（AppStatusBarController 无硬编码颜色）：无需改动
- 注意事项 7（8 色通道色谱）：不迁移，功能性颜色
- 注意事项 8（applyEditorTheme 重复设值）：本期清理
- 注意事项 9（时钟专用角色）：补充 4 个角色到语义色板
- 建议优化（WisdomWidget 配色对齐）：本期统一 accent 色值为 `#C62828`
