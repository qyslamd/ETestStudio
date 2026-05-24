# 全局 IconProvider + ThemeManager 设计

## 背景与问题

### 主题系统现状

当前主题管理分散在三个地方：

1. **`src/core/common/ThemeState.h/.cpp`** — 一个文件作用域的全局 bool `g_dark_mode`，默认 `true`（暗色主题）。提供 `isDarkTheme()` / `setDarkTheme(bool)`，**无信号机制**，widget 只能轮询。
2. **`MainWindow::applyTheme()`** — 唯一的主题切换编排点（`main_window.cpp:194-226`），负责：读取配置 → 更新 ThemeState → 重载 ActivityBar 图标 → 加载 QSS → 同步 SettingsDialog。**QSS 加载逻辑硬编码在 MainWindow 中**。
3. **`ConfigManager::configChanged` 监听** — `MainWindow::initSignals()` 中监听 `CONFIG_APPEARANCE_THEME` 配置变化并调用 `applyTheme()`。

### 图标系统现状

当前存在 **三种图标加载模式**，各自为政：

1. **内联 `isDarkTheme()` 三目运算** — 出现在 `SearchWidget`、`GitWidget`、`BottomContainerWidget`、`TopologyEditorWidget`。图标在构造时加载，**主题切换后永不更新**。
2. **`FileTypeIconProvider::loadDualThemeIcon()`** — 唯一的图标加载辅助方法，但它是私有的，仅供 FileTypeIconProvider 自身使用。
3. **`QStyle::standardIcon()`** — SARibbon 功能区使用平台原生图标，**无主题感知**。

### 核心问题

- **主题切换覆盖不完整**：只有 `ActivityBarWidget::reloadIcons()` 会响应主题切换，其他 widget 在构造后就不再检查主题状态。
- **无集中图标缓存**：每次 `isDarkTheme()` 三目运算都创建新的 `QIcon` 对象，重复加载磁盘文件。
- **无统一 API**：每个 widget 自己拼接 `_dark.svg` / `_light.svg` 路径字符串，重复代码多。
- **主题配置与 UI 紧耦合**：QSS 加载逻辑在 MainWindow 类中，无法被其他窗口复用。

---

## 目标

创建两个全局类，统一管理主题与图标：

1. **ThemeManager（主题管理器）** — 主题状态的单一信源，通过 Qt 信号机制通知所有 widget 主题变化，接管 QSS 加载。
2. **IconProvider（图标提供者）** — 统一的图标加载入口，根据当前主题自动选择图标变体，带缓存。

---

## 方案设计

> 库定位：不单独建库，ThemeManager 和 IconProvider 都放在 `src/app/` 中，作为应用层通用组件。
> 后续如有 example 需要复用，通过添加 `src/app/resource.qrc` 和对应源文件的方式接入。

### 1. ThemeManager

#### 类声明

```cpp
// src/app/ThemeManager.h
namespace etest::app {

class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager& instance();
    ~ThemeManager() override;

    // 禁用拷贝/移动
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    // 查询
    bool isDarkTheme() const;           // 替代 core::common::isDarkTheme()
    QString currentTheme() const;       // 返回 "default" 或 "vscode"

    // 设置主题（写配置、发信号）
    void setTheme(const QString& themeId);

signals:
    // 主题变化信号，widget 连接此信号刷新图标/颜色
    // isDark=true 表示切换到暗色主题
    void themeChanged(bool isDark);

private:
    ThemeManager(QObject* parent = nullptr);
    void connectConfigManager();

    QString current_theme_ = QStringLiteral("default");
};

}  // namespace etest::app
```

#### 行为设计

**构造时**：
1. 从 `ConfigManager` 读取当前配置的 `appearance/theme` 值
2. 调用 `core::common::setDarkTheme()` 同步遗留状态标志
3. 连接 `ConfigManager::configChanged` 以响应外部配置变更

**`setTheme(themeId)`**：
1. guard：如果 `themeId == current_theme_`，直接返回（防止 re-entry）
2. 更新 `current_theme_`
3. 调用 `core::common::setDarkTheme(themeId == "vscode")` 同步遗留状态
4. 写 `ConfigManager::set("appearance/theme", themeId)` 持久化
5. emit `themeChanged(isDark)`

**Re-entry 防护**：`setTheme()` 写 ConfigManager 会触发 `configChanged`，进而再次调用 `setTheme()`。第一步的 guard `if (themeId == current_theme_) return;` 打断这个循环。

**QSS 加载策略**：ThemeManager **不持有 widget 指针**，也不直接加载 QSS。它只负责状态管理和信号通知。QSS 加载由 MainWindow（或其他窗口）在响应 `themeChanged` 信号的 slot 中完成。这样 ThemeManager 保持通用性，不依赖特定的窗口结构。

#### 信号流

```
用户切换主题 → ConfigManager::set("appearance/theme", "vscode")
                    │
                    ▼
ConfigManager::configChanged("appearance/theme")
                    │
        ┌───────────┘
        ▼
ThemeManager::setTheme("vscode")  [guard: 不同则继续]
    │   ├── current_theme_ = "vscode"
    │   ├── core::common::setDarkTheme(true)
    │   ├── ConfigManager::set(...)  [不会递归：guard 已拦截]
    │   └── emit themeChanged(true)
    │             │
    │     ┌───────┼───────────┐
    │     ▼       ▼           ▼
    │  ActivityBar  MainWindow  其他 widget(未来)
    │  reloadIcons  onThemeChanged
    │               ├── load QSS
    │               ├── ads_dark QSS
    │               └── 同步 settings_dialog
    ▼
complete
```

#### 向后兼容

- `core::common::isDarkTheme()` 继续正常工作（ThemeManager 内部同步该标志）
- `topology-demo` 继续使用 `setDarkTheme(false)`，不依赖 ThemeManager
- 所有现有的 `isDarkTheme()` 调用点无需修改

---

### 2. IconProvider

#### 类声明

```cpp
// src/app/IconProvider.h
namespace etest::app {

class IconProvider {
public:
    static IconProvider& instance();

    // 禁用拷贝/移动
    IconProvider(const IconProvider&) = delete;
    IconProvider& operator=(const IconProvider&) = delete;

    // 图标加载（自动主题适配）
    // e.g. icon("search") → search_light.svg（暗色主题）或 search_dark.svg（亮色主题）
    QIcon icon(const QString& name) const;

    // 清空缓存（连接 ThemeManager::themeChanged 自动调用）
    void clearCache();

private:
    IconProvider();
    ~IconProvider() = default;

    QString resolvePath(const QString& baseName) const;

    mutable QCache<QString, QIcon> cache_;
};

}  // namespace etest::app
```

#### 路径解析规则

SVG 图标按命名约定配对：`{name}_dark.svg`（亮色背景可读）和 `{name}_light.svg`（暗色背景可读）。

```
resolvePath("search"):
  isDark=true  → 尝试 :/resources/icons/svg/search_light.svg  → 存在则返回
  isDark=false → 尝试 :/resources/icons/svg/search_dark.svg   → 存在则返回
  都不存在     → 尝试 :/resources/icons/svg/search.svg         → 单状态图标回退
```

注意语义：`isDarkTheme() == true`（暗色背景）使用 `_light` 变体（在暗色背景上清晰的浅色图标）。

#### 缓存策略

- 使用 `QCache<QString, QIcon>`，设最大容量 200 条目（42 对图标绰绰有余）
- 懒加载：首次 `icon(name)` 调用时解析路径、加载文件、缓存
- 后续相同 name 直接从缓存返回
- 空图标也缓存（防止反复命中不存在的文件）
- `clearCache()` 通过 `Qt::QueuedConnection` 连接 `ThemeManager::themeChanged`

#### IconProvider 与 ThemeManager 的连接

在 IconProvider 构造中连接：

```cpp
IconProvider::IconProvider() : cache_(200) {
    QObject::connect(
        &ThemeManager::instance(), &ThemeManager::themeChanged,
        [this]() { clearCache(); },
        Qt::QueuedConnection);
}
```

使用 `QueuedConnection` 确保：先清空缓存 → widget 响应 themeChanged 时重新通过 IconProvider 加载图标 → 缓存自动填充新主题的图标。

---

## 迁移计划

### Phase 0：创建新文件（无行为变更）

创建 `ThemeManager.h/.cpp` 和 `IconProvider.h/.cpp`，加入 CMakeLists.txt，确认编译通过。此时它们功能完整但不接入 MainWindow。

### Phase 1：FileTypeIconProvider 融合 + IconProvider 接入 ThemeManager

FileTypeIconProvider 的 `loadDualThemeIcon()` 内部委托给 `IconProvider::icon()`：

```cpp
// FileTypeIconProvider::loadDualThemeIcon
QIcon FileTypeIconProvider::loadDualThemeIcon(const QString& baseName) const {
    return IconProvider::instance().icon(baseName);
}
```

- 移除 `#include "core/common/ThemeState.h"` 依赖（不需要了，IconProvider 内部通过 ThemeManager 获知主题）
- `FileTypeIconProvider` 自身增加 `reload()` 方法，重建 `extension_icons_` map，连接 `ThemeManager::themeChanged` 以实现文件浏览器图标实时刷新
- IconProvider 在 `resolvePath()` 中通过 `ThemeManager::instance().isDarkTheme()` 获取主题状态，无需外部传入

### Phase 2：MainWindow 接入

**main_window.h**：
- 移除 `applyTheme()` 声明
- 添加 `onThemeChanged(bool)` slot

**main_window.cpp**：
- `initUi()` 中：`setDarkTheme(theme == "vscode")` → `ThemeManager::instance().setTheme(theme)`
- `initSignals()` 中：连接 `themeChanged` → `onThemeChanged`（加载 QSS、处理 ADS/settings）
- ConfigManager 的 configChanged 监听移至 ThemeManager 内部
- 移除原来的 `applyTheme()` 方法实现

### Phase 3：ActivityBarWidget 迁移

**ActivityBarWidget.h**：
- 移除 `IconPair` 结构体
- `QVector<IconPair> icon_pairs_` → `QStringList icon_names_`

**ActivityBarWidget.cpp**：
- `setupUi()` 中：`QIcon(dark ? d.light : d.dark)` → `IconProvider::icon("project")`
- 构造函数连接 `ThemeManager::themeChanged` → `reloadIcons()`
- `reloadIcons()` 中：`for each button` → `btn->setIcon(IconProvider::icon(name))`

---

## 后续增量计划（Phase 4+，不在本次实施）

以下 widget 后续可增量接入，无需一次性完成：

| Widget | 当前模式 | 接入方式 |
|--------|---------|---------|
| SearchWidget | 内联三目运算 | 连接 themeChanged，刷新图标 |
| GitWidget | 内联三目运算 | 连接 themeChanged，刷新图标 |
| BottomContainerWidget | 内联三目运算 | 连接 themeChanged，刷新图标 |
| ImageViewerWidget | 内联三目运算（背景色） | 连接 themeChanged，更新 brush |
| TopologyEditorWidget | 内联三目运算（topoIcon） | 替换为 IconProvider + themeChanged |
| IcdBitLayoutView | 内联三目运算（背景色） | 连接 themeChanged，更新 brush |
| SARibbon 功能区 | QStyle::standardIcon | 替换为 IconProvider（需确认是否适配） |

---

## 文件变更清单

### 新增文件

```
src/app/ThemeManager.h          — ThemeManager 单例声明
src/app/ThemeManager.cpp        — ThemeManager 实现
src/app/IconProvider.h          — IconProvider 单例声明
src/app/IconProvider.cpp        — IconProvider 实现
```

### 修改文件

```
src/app/CMakeLists.txt              — 添加 ThemeManager / IconProvider 源文件
src/app/main_window.h               — 移除 applyTheme()，添加 onThemeChanged()
src/app/main_window.cpp             — 主题逻辑迁移到 ThemeManager
src/app/FileTypeIconProvider.h/.cpp — loadDualThemeIcon 委托 IconProvider::icon()
src/app/ActivityBarWidget.h         — IconPair → QStringList
src/app/ActivityBarWidget.cpp       — 路径对 → IconProvider API
```

---

## 未变更的部分

- `src/core/common/ThemeState.h/.cpp` — 保留，ThemeManager 内部同步其状态
- `src/core/config/ConfigDefs.h` — ThemeManager 已通过 ConfigManager 读取配置，无需新增配置项
- `examples/topology-demo/` — 继续使用 `setDarkTheme(false)` 传统 API
- `src/topology/TopologyTheme.cpp` — 继续使用 `isDarkTheme()` 传统 API

---

## 验证方法

1. `scripts/build_ninja.bat` 编译通过（97/97 targets）
2. 启动 etest_demo，确认：
   - ActivityBar 图标显示正确（暗色主题用 `_light` 变体，亮色主题用 `_dark` 变体）
   - 设置对话框切换主题，ActivityBar 图标实时更新
   - MainWindow QSS 正确加载（vscode.qss/ads_dark.qss）
   - `core::common::isDarkTheme()` 与 ThemeManager 状态一致
   - 主题切换无崩溃、无旧图标残留
3. `scripts/run_topology-demo.bat` 编译运行正常
