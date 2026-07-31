# 主题 QADS 样式每主题定制方案与实施计划

- 日期：2026-07-31
- 状态：已审查（无 🔴 阻塞），可进入实施
- 范围：`src/app/resources/styles/`、`src/core_ui/ThemeManager.cpp`、`src/app/MainWindow.cpp`、`src/app/resource.qrc`、`src/app/resources/scripts/gen_themes.py`、`CLAUDE.md`

## 1. 问题陈述

当前 QADS（Qt-Advanced-Docking-System）的样式覆盖集中在 `src/app/resources/styles/ads_dark.qss` 一个文件中，被**所有暗色主题共享**：

- vscode、klein_blue、indigo、chinese_red 四个暗色主题共用同一份暗色 dock 配色；
- 亮色主题（default、lime、gold、rose_pink 及脚本生成的 4 个）完全没有 QADS 定制，依赖 QADS 内置 `default.css` 的 palette 外观。

期望：**每个主题对 QADS 有独立的观感**。暗色主题各有各的 dock 配色，亮色主题也能定制 dock 细节。

## 2. 架构回顾

### 2.1 QADS 内置样式的作用机制

- QADS 的 `DockManagerPrivate::loadStylesheet()`（`3rdparty/Qt-Advanced-Docking-System-3.8.3/src/DockManager.cpp:183`）在 **CDockManager 构造函数**里读取内置 `:ads/stylesheets/default.css`（Linux 下追加 `_linux` 后缀；启用 FocusHighlighting 时用 `focus_highlighting.css`），`setStyleSheet()` 到 CDockManager 自身。
- Qt 样式表级联：**widget 级样式表压过应用级（qApp）样式表**。因此写在各主题 QSS（全局 qApp）里的 QADS 样式对 dock 子树无效，**必须设置到 CDockManager 自身**。
- 浮动窗口（`CFloatingDockContainer`）是独立顶层窗口，**不在 CDockManager 子树内**，无法吃到 CDockManager 的样式表，只能走全局 qApp。

### 2.2 现有 QADS 覆盖的两个加载点

| 加载点 | 代码位置 | 作用 |
|--------|----------|------|
| `MainWindow::onThemeChanged()` | `src/app/MainWindow.cpp:1424-1439` | `dock_manager_->setStyleSheet(default.css + (暗色 ? ads_dark.qss : ""))`，覆盖 QADS 构造函数设的内置样式 |
| `ThemeManager::loadQss()` | `src/core_ui/ThemeManager.cpp:278-284` | 暗色时把 `ads_dark.qss` 追加到全局 qApp 样式表，覆盖浮动窗口 |

### 2.3 现有主题文件结构

除 default（仅 2 个文件，无 `ribbon_default.qss`）外，每个主题 3 个文件（注册在 `src/app/resource.qrc`）：

```
src/app/resources/
├── themes/<id>.json         # 色板（15 语义色 + 26 编辑器色）
├── styles/<id>.qss           # 主程序 QSS
└── styles/ribbon_<id>.qss    # Ribbon QSS
```

现有 12 个主题：暗色 4 个（vscode / klein_blue / indigo / chinese_red），亮色 8 个（default / lime / gold / rose_pink 手写；avocado_green / mocha_brown / hermes_orange / bright_yellow 由脚本生成）。

### 2.4 图标变体规则

`AppIconProvider::resolvePath()` 规则（`src/core_ui/AppIconProvider.cpp:36-48`）：

```
暗色主题 → {name}_light.svg（浅色图标配深色背景）
亮色主题 → {name}_dark.svg（深色图标配亮色背景）
```

`close_dark/light.svg`、`drop_down_arrow_dark/light.svg` 等变体均已存在。QADS 覆盖中引用的图标必须遵循同一规则。

### 2.5 gen_themes.py 现状

- 生成 `themes` 列表中的亮色主题（硬编码 `'isDark': False`），每个主题生成 JSON + QSS + Ribbon QSS 3 个文件。
- 计算 10 个调色变量：`p1-p4`（背景梯度）、`accent_dk`（强调）、`acc_st`（状态栏）、`txt`（正文，硬编码 `#2A2015`，`text_color()` 定义后未被调用）、`sec`（次级文本）、`dis`（禁用）、`a`（主色）。
- 不修改 `resource.qrc`（注册为手动步骤）。
- 对已有主题重跑脚本幂等（3 个文件内容不变）。

## 3. 方案选项及理由

### 方案 A：每主题 `ads_<id>.qss` 伴随文件（设置到 CDockManager 级）

保留现有双加载点机制，把「共享的 ads_dark.qss」改为「每主题独立的 `ads_<id>.qss`」。按是否保留 default.css 基底分两个子变体：

**A1（全自持，不读 default.css）**

```
dock_manager_->setStyleSheet(read("ads_<themeId>.qss"))   // 仅每主题文件
```

- 优点：彻底不依赖 QADS 内置样式，主题完全掌控。
- 缺点：每个主题必须提供**完整** QADS 规格，否则 dock 控件结构样式（分割条、tab 尺寸、边框）缺失；亮色 8 个主题都要从零写完整规格，工作量大、风险高。

**A2（保留 default.css 基底 + 每主题覆盖）**

```
dock_manager_->setStyleSheet(default.css + ads_<themeId>.qss)   // 存在才追加
```

- 优点：
  - default.css 作为结构兜底，某主题暂无 ads 文件时仍以 palette 外观渲染，dock 不裸奔；
  - 亮色主题只需覆盖想改的点（default.css 本就是 palette 亮色），工作量小；
  - 与现有代码差异最小（`if (isDark)` 分支改为按主题 ID 读文件）；
  - 暗色主题不受影响——现有 `ads_dark.qss` 已覆盖 default.css 全部选择器，作为各暗色主题起点，基底被 shadow。
- 缺点：无实质缺点；唯一约束是暗色主题的覆盖文件仍需足够完整（现状已如此）。

**推荐 A2。**

### 方案 B：QADS 段合并回各主题主 QSS + `dock_manager_->setStyleSheet("")` 禁用内置样式

- 把 ads_dark.qss 内容按每主题配色合并回各 `*.qss`，并调用 QADS 官方提供的 `setStyleSheet("")`（`DockManager.h:58-68` 文档明确支持）禁用内置 default.css，让全局主题 QSS 作用到 dock 子树。
- 优点：每主题单一 QSS 文件，自包含。
- 缺点：
  - QADS 规格要复制进**全部 12 个主题**（亮色主题现在完全没有 QADS 段，一旦禁用 default.css 会裸奔）；
  - 任一主题 QADS 段不完整，dock 结构样式即缺失；
  - 12 份重复维护，与「拆分」的初衷背道而驰。

### 方案 C：patch QADS 源码去掉内置样式

- 可行（QADS 源码内置于 `3rdparty/`），但不推荐：
  - 关掉内置样式后，dock 子树回退到全局 QSS，仍需把所有 QADS 样式搬进各主题（同方案 B 的重复问题）；
  - 丢失 `_linux` / `focus_highlighting` 变体分支；
  - fork 第三方库，升级需重新打补丁；
  - 官方 `setStyleSheet("")` 已提供等价入口，无需改源码。

## 4. 决策记录

| 决策点 | 结论 | 理由 |
|--------|------|------|
| 方案选型 | **A2** | 保留 default.css 基底做结构兜底，改动最小、风险最低，满足每主题定制目标 |
| 覆盖文件命名 | `ads_<id>.qss` | 与 `ribbon_<id>.qss` 命名一致，见名知意 |
| 加载点 | 双加载点保留 | MainWindow（CDockManager 级）+ ThemeManager（全局 qApp，浮动窗口）缺一不可 |
| 亮色图标变体 | `_dark.svg` | 与 AppIconProvider 规则一致 |
| 是否 patch QADS | 否 | 方案 C 排除 |
| 脚本生成范围 | 仅亮色 | gen_themes.py 现状只生成亮色主题，暗色 ads 手写 |
| 本次范围 | QSS 层 QADS 定制，**不含 tab 形状/选中配色** | 亮色选中 tab 硬编码纯白、hover 色由 `DockAreaTabBarStyle::paintAllTabs()` 程序化绘制，QSS 无效；tab 每主题化留作独立方案 |

## 5. 实施步骤

核心约束：**先 fork + 改 C++ + 改 qrc，最后删 `ads_dark.qss`**，否则构建断链。

### 任务 1：新建亮色模板 `ads_template.qss`

**文件**：`src/app/resources/styles/ads_template.qss`（新增）

模板结构沿用 `ads_dark.qss`，颜色 token 化，亮色默认值 + 深色半透明 hover（不能用 `rgba(255,255,255,…)`）：

```css
/*
 * QADS Dock 样式模板（亮色）
 * 由 gen_themes.py 替换 token 生成 ads_<id>.qss
 * 每个主题的 QADS Dock 样式，设置到 CDockManager（覆盖内置 default.css）+ 全局 qApp（覆盖浮动窗口）
 */
ads--CDockContainerWidget {
    background: @WINDOW_BG@;
}
ads--CDockContainerWidget > QSplitter {
    padding: 1px 0px 1px 0px;
}
ads--CDockContainerWidget ads--CDockSplitter::handle {
    background: @PANEL_BG@;
}
ads--CDockSplitter {
    background: @PANEL_BG@;
}
ads--CDockAreaWidget {
    background: @WINDOW_BG@;
    border: none;
}
ads--CDockAreaTitleBar {
    background: @PANEL_BG@;
    border-top: 1px solid @WINDOW_BG@;
    border-bottom: 1px solid @WINDOW_BG@;
    padding: 0px;
}
ads--CDockAreaTitleBar QLabel {
    color: @SECONDARY_TEXT@;
    font-size: 12px;
    padding: 2px 6px;
}
ads--CDockAreaTitleBar QToolButton {
    background-color: transparent;
    border: none;
    color: @SECONDARY_TEXT@;
    padding: 2px;
    margin: 0px;
}
ads--CDockAreaTitleBar QToolButton:hover {
    background-color: @HOVER_BG@;
    color: @TEXT@;
}
ads--CDockWidget {
    background: @WINDOW_BG@;
    border-color: @WINDOW_BG@;
    border-style: solid;
    border-width: 1px 0px 0px 0px;
}
ads--CDockWidgetTab {
    background: transparent;
    border: none;
    padding: 0px 4px;
}
ads--CDockWidgetTab[activeTab="true"] {
    background: transparent;
    border: none;
}
ads--CDockWidgetTab:hover {
    background: transparent;
}
ads--CDockWidgetTab QLabel {
    color: @SECONDARY_TEXT@;
    font-size: 12px;
}
ads--CDockWidgetTab[activeTab="true"] QLabel {
    color: @TEXT@;
}
ads--CDockWidgetTab:hover QLabel {
    color: @TEXT@;
}
ads--CTitleBarButton {
    padding: 0px 0px;
}
QScrollArea#dockWidgetScrollArea {
    padding: 0px;
    border: none;
}
#tabCloseButton {
    margin-top: 2px;
    background: transparent;
    border: none;
    border-radius: 3px;
    padding: 2px;
    qproperty-icon: url(:/resources/icons/svg/close_@ICON_VARIANT@.svg);
    qproperty-iconSize: 14px;
}
#tabCloseButton:hover {
    background-color: @HOVER_BG@;
}
#tabCloseButton:pressed {
    background-color: @ACCENT@;
}
#tabsMenuButton::menu-indicator {
    image: none;
}
#tabsMenuButton {
    qproperty-icon: url(:/resources/icons/svg/drop_down_arrow_@ICON_VARIANT@.svg);
    qproperty-iconSize: 16px;
}
#dockAreaCloseButton {
    qproperty-icon: url(:/resources/icons/svg/close_@ICON_VARIANT@.svg);
    qproperty-iconSize: 16px;
}
#detachGroupButton {
    qproperty-icon: url(:/ads/images/detach-button.svg);
    qproperty-iconSize: 16px;
}
```

**token → 语义色映射**（亮色）：

| token | 语义色 | 亮色默认（对齐 default.json） |
|-------|--------|----------|
| `@WINDOW_BG@` | windowBackground | `#F0F0F0` |
| `@PANEL_BG@` | panelBackground | `#F0F0F0` |
| `@HOVER_BG@` | hoverBackground | `#E0E0E0` |
| `@TEXT@` | textColor | `#333333` |
| `@SECONDARY_TEXT@` | secondaryTextColor | `#888888` |
| `@ACCENT@` | accentColor | `#007ACC` |
| `@ICON_VARIANT@` | 图标变体 | `dark` |

### 任务 2：改 `gen_themes.py` 生成第 4 个文件

**文件**：`src/app/resources/scripts/gen_themes.py`（修改）

循环体内新增**两处**代码（脚本已 `import re`）：

**(1) 主 QSS 的 QADS 注释修正——必须插入在 `f.write(qss)`（约 gen_themes.py:224）之前、QSS 颜色替换之后：**

```python
    # 主 QSS 从 vscode.qss 复制的 QADS 注释同步修正（正则通配，vscode 注释改为 ads_vscode 后仍可命中）
    qss = re.sub(r'已迁移至 ads_\w+\.qss',
                 '已迁移至 ads_{}.qss'.format(t['id']), qss)
```

**(2) ads 生成块——追加在 ribbon QSS 生成之后：**

```python
    # ---- QADS QSS（每主题 dock 样式，亮色模板）----
    with open('{}/ads_template.qss'.format(styles_dir), 'r', encoding='utf-8') as f:
        ads = f.read()
    ads = ads.replace('@WINDOW_BG@', p1)
    ads = ads.replace('@PANEL_BG@', p2)
    ads = ads.replace('@HOVER_BG@', p4)
    ads = ads.replace('@TEXT@', txt)
    ads = ads.replace('@SECONDARY_TEXT@', sec)
    ads = ads.replace('@ACCENT@', accent_dk)  # = JSON accentColor，与手写亮色主题的 @ACCENT@ 语义一致
    ads = ads.replace('@ICON_VARIANT@', 'dark')

    fp = '{}/ads_{}.qss'.format(styles_dir, t['id'])
    with open(fp, 'w', encoding='utf-8') as f:
        f.write(ads)
    print('  ADS: {}'.format(fp))
```

**执行**：在仓库根目录运行 `python src/app/resources/scripts/gen_themes.py`，产出 4 个 `ads_*.qss`（avocado_green / mocha_brown / hermes_orange / bright_yellow）。QSS 注释同步改为各主题（正则 `ads_\w+\.qss` 通配，任务 8 改完 vscode.qss 注释后重跑仍正确）。

> **重跑幂等性说明**：提交 `5cfc019` 的 7 个 ribbon 文件是**手改**的（SARibbonPanelLabel 出现双分号 `;;`，如 `background-color: #E1E5D6;;`），而脚本 replace（`gen_themes.py:264-268`）产出单分号 `;`。重跑脚本会把这 4 个生成 ribbon（avocado_green / mocha_brown / hermes_orange / bright_yellow）的 `;;` 规范化回 `;`，颜色值不变（已验证与各主题 JSON 的 `toolbarBackground` 一致）——QSS 中多余空声明无害，属预期差异；3 个手写 ribbon（lime / gold / rose_pink）不受脚本影响，保留 `;;` 原样。

### 任务 3：手写 4 个暗色 ads（fork `ads_dark.qss`）

**文件**：`styles/ads_{vscode,klein_blue,indigo,chinese_red}.qss`（新增）

以 `ads_dark.qss` 为模板，按各主题 JSON 的 `colors` 替换（图标恒为 `light`）：

| 源（ads_dark.qss） | 目标（theme JSON colors） |
|---------------------|---------------------------|
| `#1E1E1E` | `windowBackground` |
| `#252526` | `panelBackground` |
| `#858585` | `secondaryTextColor` |
| `#CCCCCC` | `textColor` |
| `rgba(255, 255, 255, 26)` | `hoverBackground` |
| `rgba(255, 255, 255, 31)` | `hoverBackground` |
| `rgba(255, 255, 255, 51)` | `accentColor` |
| `#FFFFFF`（hover 文字） | `textColor` |

各主题取值：

| 主题 | windowBackground | panelBackground | hoverBackground | secondaryTextColor | textColor | accentColor |
|------|------------------|-----------------|-----------------|---------------------|-----------|-------------|
| vscode | `#1E1E1E` | `#252526` | `#505050` | `#858585` | `#CCCCCC` | `#007ACC` |
| klein_blue | `#0A0A1A` | `#0F0F24` | `#1A1A40` | `#8080A0` | `#D0D0E0` | `#003FD0` |
| indigo | `#0A0015` | `#100820` | `#201540` | `#8060A0` | `#D0C0E0` | `#5A1A9A` |
| chinese_red | `#1A1A1A` | `#241F20` | `#4A3437` | `#9A8585` | `#D4C5C5` | `#C72523` |

**替换顺序约束**：逐条做完整字符串替换（`#1E1E1E`→…、`#252526`→…、`#858585`→…、`#CCCCCC`→…、三个 `rgba(255,255,255,…)`→…、`#FFFFFF`→…）。先替换较长/不冲突的项，避免新值误中未替换的源串（本表各目标值与源值两两不同，顺序无实际冲突）。

> **vscode 说明**：其 windowBackground/panelBackground/secondaryTextColor/textColor 与 ads_dark.qss 源值一致，故 `#1E1E1E/#252526/#858585/#CCCCCC` 四条替换为 no-op；但其余替换仍生效——3 处 `rgba(255,255,255,…)` → `#505050`/`#007ACC`、`#FFFFFF`（hover 文字）→ `#CCCCCC`。统一按通用表逐条替换，不设例外。可直接复制 `ads_dark.qss` 改名 `ads_vscode.qss` 后替换这 4 处。

### 任务 4：手写 4 个亮色 ads（基于模板套色）

**文件**：`styles/ads_{default,lime,gold,rose_pink}.qss`（新增）

用任务 1 的模板做 token 替换，图标 `dark`。取值：

| token | default | lime | gold | rose_pink |
|-------|---------|------|------|-----------|
| `@WINDOW_BG@` | `#F0F0F0` | `#F5FFF5` | `#FFFAF0` | `#FFF5F8` |
| `@PANEL_BG@` | `#F0F0F0` | `#E8F5E8` | `#FFF5E6` | `#FFF0F5` |
| `@HOVER_BG@` | `#E0E0E0` | `#C8E6C8` | `#FFE0B2` | `#FFD6E6` |
| `@TEXT@` | `#333333` | `#1B3A1B` | `#3A2A00` | `#4A2030` |
| `@SECONDARY_TEXT@` | `#888888` | `#558B55` | `#8B7500` | `#B0607A` |
| `@ACCENT@` | `#007ACC` | `#00CC00` | `#D4A800` | `#E890A8` |
| `@ICON_VARIANT@` | `dark` | `dark` | `dark` | `dark` |

### 任务 5：改 `MainWindow::onThemeChanged`

**文件**：`src/app/MainWindow.cpp:1424-1439`（修改）

`if (isDark)` 分支改为按当前主题读文件，函数首行加 `Q_UNUSED(isDark)`：

```cpp
void MainWindow::onThemeChanged(bool isDark) {
  Q_UNUSED(isDark);  // QADS 覆盖已改为按主题读取，不再依赖 isDark
  ...
  // 后设 QADS 样式（default.css 基底 + 每主题覆盖，覆盖 QADS 内置 widget 级 default.css）
  if (dock_manager_) {
    QString adsQss;
    QFile defaultCss(QStringLiteral(":ads/stylesheets/default.css"));
    if (defaultCss.open(QIODevice::ReadOnly | QIODevice::Text)) {
      adsQss = QString::fromUtf8(defaultCss.readAll());
      defaultCss.close();
    }
    QFile themeAds(QStringLiteral(":/resources/styles/ads_%1.qss")
                       .arg(ThemeManager::instance().currentTheme()));
    if (themeAds.open(QIODevice::ReadOnly | QIODevice::Text)) {
      adsQss += QStringLiteral("\n") + QString::fromUtf8(themeAds.readAll());
      themeAds.close();
    }
    dock_manager_->setStyleSheet(adsQss);
    // 其余代码不变（下拉菜单按钮图标、搜索框占位符）
  }
}
```

### 任务 6：改 `ThemeManager::loadQss`

**文件**：`src/core_ui/ThemeManager.cpp:278-284`（修改）

全局 qApp 追加从「仅暗色 ads_dark」改为按主题、存在才追加：

```cpp
  is_dark_ = detectDarkFromQss(qss);

  // 每主题 QADS 覆盖（存在时追加到全局，覆盖浮动窗口）
  QFile adsFile(QStringLiteral(":/resources/styles/ads_%1.qss").arg(themeId));
  if (adsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qss += QStringLiteral("\n") + QString::fromUtf8(adsFile.readAll());
    adsFile.close();
  }

  qApp->setStyleSheet(qss);
  emit themeChanged(is_dark_);
```

### 任务 7：更新 `resource.qrc`

**文件**：`src/app/resource.qrc`（修改）

- 移除 `resources/styles/ads_dark.qss` 条目（第 14 行）
- 注册 12 个 `ads_<id>.qss`（紧随现有 styles 条目区域）：

```xml
        <file>resources/styles/ads_vscode.qss</file>
        <file>resources/styles/ads_klein_blue.qss</file>
        <file>resources/styles/ads_indigo.qss</file>
        <file>resources/styles/ads_chinese_red.qss</file>
        <file>resources/styles/ads_default.qss</file>
        <file>resources/styles/ads_lime.qss</file>
        <file>resources/styles/ads_gold.qss</file>
        <file>resources/styles/ads_rose_pink.qss</file>
        <file>resources/styles/ads_avocado_green.qss</file>
        <file>resources/styles/ads_mocha_brown.qss</file>
        <file>resources/styles/ads_hermes_orange.qss</file>
        <file>resources/styles/ads_bright_yellow.qss</file>
```

### 任务 8：更新 11 个主题 QSS 注释

**文件**：11 个主题 QSS（`default.qss` 除外）

第 91-92 行：

```css
/* ==================== QADS Dock 样式（暗色覆盖）已迁移至 ads_dark.qss ==================== */
/* QADS的样式必须设置到CDockManager自身，否则会被QADS内置的default.css覆盖 */
```

→

```css
/* ==================== QADS Dock 样式已迁移至 ads_<id>.qss ==================== */
/* QADS的样式必须设置到CDockManager自身，否则会被QADS内置的default.css覆盖 */
```

其中 `<id>` 为主题 ID。**注意**：vscode.qss 是脚本的派生源，若日后重跑脚本，4 个生成主题会拷贝 vscode.qss 的注释；任务 2 的正则 `ads_\w+\.qss` 会将其同步为 `ads_<id>.qss`（即使 vscode.qss 注释已在本任务改为 `ads_vscode.qss`，正则仍可命中）。

### 任务 9：删除 `ads_dark.qss`（最后）

**文件**：`src/app/resources/styles/ads_dark.qss`（删除）

确认任务 5/6/7 已完成（MainWindow.cpp:1433、ThemeManager.cpp:279 不再引用、qrc 已移除条目）后再删，避免构建断链。

### 任务 10：更新 `CLAUDE.md` 主题规则

**文件**：`CLAUDE.md`（修改）——「新增主题（JSON 驱动，零 C++ 改动）」一节：

- 「运行后自动生成 JSON + QSS + Ribbon QSS 三个文件」→「**四个**文件（JSON + QSS + Ribbon QSS + **ads QSS**）」
- 手动创建步骤在 ribbon 之后补：
  ```
  4. `src/app/resources/styles/ads_<id>.qss` — QADS dock 样式，基于 `ads_template.qss`（亮色）或 `ads_vscode.qss`（暗色）替换颜色
  ```
- 「零 C++ 改动」修正为：两处 QADS 加载点（MainWindow / ThemeManager）已按主题 ID 推导 `ads_<id>.qss` 路径，新增主题无需改 C++
- 补图标变体约束：亮色 ads 用 `_dark` 图标、暗色用 `_light`
- 补边界：dock 标签(tab) 选中配色由 `DockAreaTabBarStyle` 程序化绘制，不在 ads QSS 范围

### 任务 11：编译验证

```bash
scripts/build_ninja.bat -t debug -m ETestStudio
```

编译通过后手动验证：

1. 逐一切换 12 个主题，dock 区域（容器背景/分割条/标题栏/关闭按钮）配色随主题变化，与各主题主色一致；
2. 浮动窗口（拖出 dock 后）配色与 dock 内一致；**重点检查 lime/gold 等低对比亮色主题**的 titlebar 按钮 hover/pressed 观感（浮动窗口首次获得 QADS 样式）；
3. 亮色主题关闭/下拉图标为深色，暗色为浅色；
4. tab 选中配色不随主题（已知边界，符合预期）。

## 6. 文件清单

| 文件 | 动作 |
|------|------|
| `src/app/resources/styles/ads_template.qss` | 新增（亮色模板，token 化） |
| `src/app/resources/styles/ads_{vscode,klein_blue,indigo,chinese_red}.qss` | 新增（fork ads_dark.qss 改色） |
| `src/app/resources/styles/ads_{default,lime,gold,rose_pink}.qss` | 新增（基于模板套色） |
| `src/app/resources/styles/ads_{avocado_green,mocha_brown,hermes_orange,bright_yellow}.qss` | 新增（脚本生成） |
| `src/app/resources/styles/ads_dark.qss` | 删除（最后一步） |
| `src/app/resources/scripts/gen_themes.py` | 修改（生成第 4 个文件） |
| `src/app/MainWindow.cpp` | 修改（onThemeChanged 按主题读 ads） |
| `src/core_ui/ThemeManager.cpp` | 修改（loadQss 按主题追加 ads） |
| `src/app/resource.qrc` | 修改（12 增 1 删） |
| 11 个主题 QSS 文件（default 除外） | 修改（第 91-92 行注释） |
| `CLAUDE.md` | 修改（主题规则） |

## 7. 风险与验证

- **风险**：亮色主题浮动窗口首次获得 QADS 样式（此前无）。若观感不符，通过调整对应 `ads_<id>.qss` 内容即可，无结构性影响。
- **风险**：某主题新增但漏配 `ads_<id>.qss` 或漏注册 qrc → 走 default.css 兜底，功能正常、观感回落，不会崩溃。
- **已知边界**：dock 标签(tab)的形状/选中配色由 `DockAreaTabBarStyle::paintAllTabs()`（`src/libui/styles/DockAreaTabBarStyle.cpp`）程序化绘制，亮色选中 tab 硬编码纯白、hover 色取自 `ThemeManager` 调色板，**不受 ads QSS 控制**。本次不涉及，tab 每主题化留作独立方案。
- **已知边界**：Linux 构建下 QADS 构造函数加载 `default_linux.css`，会被 `onThemeChanged` 用 `default.css` 覆盖——这是既有行为，本次不恶化。如在意 Linux 观感，可在 C++ 加 `#ifdef Q_OS_LINUX` 读对应变体（本次不做）。
- **回滚**：保留 `ads_dark.qss`，两处 C++ 改回原逻辑即可。
- **验证**：
  1. `scripts/build_ninja.bat -t debug -m ETestStudio` 编译通过；
  2. 逐一切换 12 个主题，检查 dock 区域（标题栏/关闭按钮/分割条/容器背景）与浮动窗口配色与各主题主色一致（**tab 选中配色除外**，见已知边界）；
  3. 亮色主题的关闭/下拉图标为深色、暗色主题为浅色。
