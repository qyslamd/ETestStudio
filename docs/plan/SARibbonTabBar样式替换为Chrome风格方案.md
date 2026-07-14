# SARibbonTabBar 样式替换为 Chrome 风格方案

## 背景

项目已有的 `TabBarStyle`（`src/libui/styles/TabBarStyle.h/cpp`）是一个 Chrome 风格的圆角 Tab 自绘样式（`QProxyStyle`），已用于部分 `QTabBar`。但 `SARibbonTabBar`（`SARibbon` 的 Ribbon Tab 栏）的样式完全由 QSS 控制，`TabBarStyle` 无法生效。

**Qt 的 QSS 规则铁律**：只要 QSS 中定义了某个子控件规则（如 `SARibbonTabBar::tab`），`QStyleSheetStyle` 就会接管该子控件的绘制，**不会调用** `QProxyStyle::drawControl(CE_TabBarTabShape, ...)`。因此直接在 `SARibbonTabBar` 上 `setStyle(new TabBarStyle)` 无效。

## 解决思路

清除 SARibbon 的 6 个 QSS 主题文件中关于 `SARibbonTabBar::tab` 的全部子控件规则（保留 `SARibbonTabBar { }` 背景色规则），然后通过 `TabBarStyle::install()` 正常接管 tab 绘制。

## 实现方案

### 1. 新增 CMake 补丁（遵循项目已有模式）

参考 `cmake/saribbon/patch_qwindowkit.cmake` 的模式，新增 `cmake/saribbon/patch_qss_tabstyle.cmake`：

- 扫描 `3rdparty/SARibbon-2.5.7/src/SARibbonBar/resource/` 下全部 6 个 `.qss` 文件
- 对每个文件：删除 `SARibbonTabBar::tab` 子控件相关段落（3~4 个状态规则，各文件不统一）
- 注意 `theme-win7.qss` 使用了**组合选择器** `SARibbonTabBar::tab:selected, SARibbonTabBar::tab:hover { ... }`，需单独处理
- **幂等性检查**：已被补丁过的文件跳过（通过匹配判断 `SARibbonTabBar::tab` 是否还有子控件规则）
- 补丁执行后**验证**：grep 6 个 QSS 文件中 `SARibbonTabBar::tab` 匹配计数均为 0
- 在 `CMakeLists.txt` 中 `include()`，放在其他 SARibbon 补丁之后

### 2. 调整 `TabBarStyle` 的 tab 尺寸策略

当前 `sizeFromContents` 固定返回 `(110, 28)`。不同地方的 QTabBar 需要不同尺寸（底部面板 vs SARibbon 标签栏），改为构造参数传入：

**`TabBarStyle.h`** — 构造器加参数、`install()` 加 `minTabSize`（**无默认值**，每个调用方必须明确传入）：
```cpp
explicit TabBarStyle(int minWidth = 110, int minHeight = 28);

static void install(QTabBar* tabBar, QSize minTabSize);

// 成员
int min_width_;
int min_height_;
```

**`TabBarStyle.cpp`** — `sizeFromContents` 使用成员变量：
```cpp
if (type == CT_TabBarTab) {
  QSize ret(size);
  ret.rheight() = qMax(size.height(), min_height_);
  ret.rwidth() = qMax(size.width(), min_width_);
  return ret;
}
```

**`MainWindow` 调用时传入 `(60, 0)`**（0 高度 = 完全由 SARibbon 布局决定）：

### 3. 调用 `TabBarStyle::install()` 及泄漏防护

注意 **TabBarStyle::install 不能重复创建实例**，否则每次主题切换会泄漏一个 QProxyStyle。

`install()` 方法需增加复用保护：

```cpp
void TabBarStyle::install(QTabBar* tabBar) {
  // 复用已有实例，避免泄漏
  auto* existing = qobject_cast<TabBarStyle*>(tabBar->style());
  if (existing) {
    existing->setDarkTheme(
        etest::core_ui::ThemeManager::instance().isDarkTheme());
    tabBar->update();
    return;
  }
  // 首次安装
  auto* style = new TabBarStyle();
  style->dark_ = etest::core_ui::ThemeManager::instance().isDarkTheme();
  tabBar->setStyle(style);
  QObject::connect(&etest::core_ui::ThemeManager::instance(),
                   &etest::core_ui::ThemeManager::themeChanged, tabBar,
                   [tabBar, style](bool isDark) {
                     style->setDarkTheme(isDark);
                     tabBar->update();
                   });
}
```

然后在 `MainWindow::onThemeChanged()` 末尾调用（QSS 已加载之后）：

```cpp
TabBarStyle::install(ribbonBar()->ribbonTabBar(), QSize(60, 0));
```

传入 `minHeight=0` 表示不设最低高度限制，tab 高度完全由 SARibbonBar 布局决定。

## 文件改动清单

### 新增文件

| 文件 | 说明 |
|------|------|
| `cmake/saribbon/patch_qss_tabstyle.cmake` | CMake 补丁：清除 6 个 QSS 中的 `::tab` 子控件规则 |

### 修改文件

| 文件 | 改动 |
|------|------|
| `CMakeLists.txt` | 在 SARibbon `add_subdirectory` 前 `include(cmake/saribbon/patch_qss_tabstyle.cmake)` |
| `src/libui/styles/TabBarStyle.h` | 构造器加 `minWidth`/`minHeight` 参数；`install()` 加 `QSize minTabSize` 参数；新增 `min_width_`/`min_height_` 成员 |
| `src/libui/styles/TabBarStyle.cpp` | ① `sizeFromContents` 高/宽改为 `qMax(height, min_height_)` + `qMax(width, min_width_)`；② `install()` 增加 `qobject_cast` 复用保护 + 转发 `minTabSize` |
| `src/app/widgets/BottomContainerWidget.cpp` | 改为 `TabBarStyle::install(tabBar, QSize(110, 28))` — 显式传入原默认值 |
| `src/test_program/TestProgramEditorWidget.cpp` | 同上 |
| `src/app/MainWindow.cpp` | 在 `onThemeChanged()` 末尾调用 `TabBarStyle::install(ribbonBar()->ribbonTabBar(), QSize(60, 0))` |

### CMake 补丁影响的 QSS 文件

CMake 补丁运行时会修改以下 6 个文件（在 `3rdparty/SARibbon-2.5.7/src/SARibbonBar/resource/` 下）：

| 文件 | 需删除的子控件规则数量 | 说明 |
|------|----------------------|------|
| `theme-win7.qss` | 4 个 | 含 `:selected, :hover` 组合选择器，需单独匹配 |
| `theme-office2013.qss` | 3 个 | 无 `::tab:!selected` |
| `theme-office2016-blue.qss` | 3 个 | 同上 |
| `theme-office2021-blue.qss` | 3 个 | 同上 |
| `theme-dark.qss` | 4 个 | 含 `::tab:!selected` |
| `theme-dark2.qss` | 4 个 | 含 `::tab:!selected` |

**注意**：各 QSS 文件的 `::tab` 规则数量不统一：
- 亮色主题（office2013/2016/2021）只有 3 段，缺少 `::tab:!selected`
- 暗色主题（dark/dark2/win7）有完整的 4 段
- `theme-win7.qss` 第 182 行使用了组合选择器 `SARibbonTabBar::tab:selected, SARibbonTabBar::tab:hover`，补丁脚本必须按完整字符串匹配

每个文件保留 `SARibbonTabBar { background-color: transparent; }` 这一行（tab bar 整体背景色）。

## 补丁幂等性方案

参考 `patch_qwindowkit.cmake` 的做法，通过检查文件内容是否仍包含 `SARibbonTabBar::tab` 子控件规则来判断是否已打过补丁：

```cmake
if("${_qss_content}" MATCHES "SARibbonTabBar::tab \\{")
    # 包含子控件规则 → 需要补丁
else()
    # 已补丁过 → 跳过
endif()
```

**注意 `theme-win7.qss` 的特殊性**：该文件第 182 行使用了组合选择器 `SARibbonTabBar::tab:selected, SARibbonTabBar::tab:hover`，补丁脚本除匹配常规的单条 `::tab` 规则外，还需检查是否存在此组合规则并一并删除。

## 验证

1. 执行 clean build：`scripts/build_ninja.bat -t debug -m ETestStudio`
2. 验证 SARibbon 选项卡已变为 Chrome 圆角风格（选中 tab 有渐变背景 + 蓝色呼吸边框）
3. 切换主题（暗色 ↔ 亮色），确认 TabBarStyle 跟随主题变化
4. 验证其他 SARibbon 组件不受影响（按钮、面板、Gallery 等 QSS 样式完整保留）
