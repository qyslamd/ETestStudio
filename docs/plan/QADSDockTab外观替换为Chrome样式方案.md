# QADS Dock Tab 外观替换为 Chrome 样式方案

## 一、背景

目前项目中已有 `TabBarStyle`（`src/libui/styles/TabBarStyle.h`）实现了 Chrome 风格的圆角 Tab 自绘样式，但该样式仅适用于 `QTabBar`，因为其 hook 的是 `CE_TabBarTabShape` / `CE_TabBarTabLabel` 这两个 `QTabBar` 独有的绘制路径。

Qt-Advanced-Docking-System（QADS）的 Tab 机制不同：
- `CDockAreaTabBar` 继承 `QScrollArea` **而不是** `QTabBar`
- 每个 Tab 是一个独立的 `CDockWidgetTab` : `QFrame` 实例，自带 layout（图标 + 标题 + 关闭按钮）
- 绘制完全依赖 QSS 样式表，**没有** `paintEvent` 重写

因此 `TabBarStyle` 对 QADS 无效，需要另寻方案。

## 二、原理分析

### 2.1 QADS Tab 结构

```
CDockAreaTabBar : QScrollArea
  └─ TabsContainerWidget : QWidget
       └─ QBoxLayout(L⇀R)
            ├─ CDockWidgetTab : QFrame    ← 每个 tab 是独立 QWidget
            │    ├─ QLabel (图标, 可选)
            │    ├─ CElidingLabel (标题)
            │    └─ QAbstractButton (关闭按钮)
            ├─ CDockWidgetTab : QFrame
            └─ stretch
```

### 2.2 现有扩展点

QADS 提供了 `CDockComponentsFactory` 工厂类，专门用于替换默认组件：

```cpp
class CDockComponentsFactory {
public:
    virtual CDockWidgetTab*   createDockWidgetTab(CDockWidget* DockWidget) const;
    virtual CDockAreaTabBar*  createDockAreaTabBar(CDockAreaWidget* DockArea) const;
    virtual CDockAreaTitleBar* createDockAreaTitleBar(CDockAreaWidget* DockArea) const;
};
```

应用启动时调用 `CDockComponentsFactory::setFactory(new MyFactory())` 即可全局替换。

### 2.3 `CDockWidgetTab` 的绘制机制

`CDockWidgetTab` 继承自 `QFrame`，没有重写 `paintEvent`，因此：
- 背景/边框由 **QSS** 控制（通过 `updateStyle()` → `internal::repolishStyle()` 刷新）
- 子控件（标题、图标、关闭按钮）各自独立绘制
- `activeTab` 属性暴露为 `Q_PROPERTY`，可在 QSS 中用 `[activeTab="true"]` 选择

> **注意**：`updateStyle()` 在 `DockWidgetTab.h:162` 中声明为 `void updateStyle();`，**不是虚函数**，无法 override。

## 三、源码审查后修正的方案

经过对 QADS 源码的完整审查，确认方案可行，但有若干关键修正。以下为修正后的方案。

### 3.1 总览

| 步骤 | 文件 | 说明 |
|------|------|------|
| 1 | `src/libui/styles/DockWidgetTabStyle.h` | 新类，继承 `ads::CDockWidgetTab`，`paintEvent` 留空（形状由容器统一绘制） |
| 2 | `src/libui/styles/DockWidgetTabStyle.cpp` | 仅保留构造 + `themeChanged` 连接 |
| 3 | `src/libui/styles/DockAreaTabBarStyle.h` | 新类，继承 `ads::CDockAreaTabBar`，eventFilter 拦截 `tabsContainerWidget` 的 paint 事件 |
| 4 | `src/libui/styles/DockAreaTabBarStyle.cpp` | `paintAllTabs` 统一绘制所有 tab 形状，复用 `TabBarStyle` 色值/路径 |
| 5 | `src/libui/styles/EtestComponentsFactory.h` | 新类，继承 `ads::CDockComponentsFactory` |
| 6 | `src/libui/styles/EtestComponentsFactory.cpp` | 重写 `createDockWidgetTab()` + `createDockAreaTabBar()` |
| 7 | 修改 `main.cpp` | 提前初始化 `ThemeManager` + 注册自定义 Factory |
| 8 | 修改 `src/core_ui/ThemeManager.cpp` | `loadQss()` 末尾 `emit themeChanged` |
| 9 | 修改 `src/app/resources/styles/ads_dark.qss` | 删 tab 背景/边框，保留子控件文字颜色 |

### 3.2 新增：`DockWidgetTabStyle`

继承 `ads::CDockWidgetTab`，头文件：

```cpp
// src/libui/styles/DockWidgetTabStyle.h
class DockWidgetTabStyle : public ads::CDockWidgetTab {
    Q_OBJECT
public:
    explicit DockWidgetTabStyle(ads::CDockWidget* DockWidget,
                                QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    // tab 位置枚举（映射自 QStyleOptionTab::Position）
    enum class TabPosition { Beginning, Middle, End, OnlyOne };
    // 选中邻位枚举（映射自 QStyleOptionTab::SelectedPosition）
    enum class SelectedPosition { NotAdjacent, NextIsSelected, PreviousIsSelected };

    [[nodiscard]] int tabIndexInBar() const;
    [[nodiscard]] TabPosition tabPosition(int index, int count) const;
    [[nodiscard]] SelectedPosition selectedPosition(int index, int count) const;

    void drawTabShape(QPainter* painter) const;
    QPainterPath selectedTabPath(const QRectF& r, TabPosition pos) const;
    QPainterPath hoveredTabPath(const QRectF& r, TabPosition pos,
                                SelectedPosition sel) const;
    QLineF dividingLine(const QRectF& r, TabPosition pos) const;

    // 色值辅助（从 TabBarStyle 移植）
    QBrush selectedBrush(const QRect& tab_rect) const;
    QColor hoveredColor() const;
    QColor dividerColor() const;
    QColor borderColor() const;

    void onThemeChanged();

    static constexpr qreal kTopMargin = 0.0;
    static constexpr qreal kHRatio = 1.0 / 5.0;
    bool dark_ = true;
};
```

cpp 实现要点：

1. **`paintEvent` —— 完全不调 `QFrame::paintEvent`** 【关键修正】

   > **⚠️ 已弃用**：以下为 per-tab 方案的实现，因 `QPainter` 硬裁剪导致视觉缺陷（§3.2.6），**推荐改用容器级统一绘制方案（§3.2.7）**。在容器级方案中，`DockWidgetTabStyle::paintEvent` 应留空（不绘制形状），形状由 `DockAreaTabBarStyle::paintAllTabs` 统一绘制。

   ```cpp
   // per-tab 方案（已弃用）
   void DockWidgetTabStyle::paintEvent(QPaintEvent* event) {
       QPainter painter(this);
       painter.setRenderHint(QPainter::Antialiasing);
       drawTabShape(&painter);
       // 不调 QFrame::paintEvent！
       // 原因：QSS 的 background / border 会覆盖我们绘制的形状
       // 子控件（CElidingLabel、关闭按钮）各自独立收 paintEvent，不受影响
   }
   ```

   - `QFrame` 的 frameShape 默认是 `NoFrame`，跳过 `paintEvent` 不会有副作用
   - ~~QADS 内置 `default.css` 的 `ads--CDockWidgetTab { background }` **自动失效**~~（**错误**：QSS background 不被 empty paintEvent 阻止，详见 §3.5 和 §八 #17）
   - 子控件文字颜色仍由 QSS 的 `ads--CDockWidgetTab QLabel { color }` 控制（子控件独立绘制）

2. **主题切换 & 重绘触发 -- 不 override `updateStyle()`** 【关键修正】

   `updateStyle()` 在 QADS 中**不是虚函数**（`DockWidgetTab.h:162`），无法 override。
   QADS 在 `setActiveTab()` 时调用 `updateStyle()` -> `internal::repolishStyle()` 刷新子控件 QSS。

   我们的自绘 `paintEvent` 依赖 C++ 成员变量（`dark_`、`isActiveTab()`），不依赖 QSS。
   重绘触发通过以下两条路径覆盖：

   - **构造函数中连接 `ThemeManager::themeChanged` 信号** -> `onThemeChanged()` -> 更新 `dark_` + `update()`
   - **`setActiveTab()` 调用 `updateStyle()` 时**，QADS 内部 repolish 会触发 widget 的 `styleChange` 事件，
     Qt 默认在 `styleChange` 后调用 `update()`，因此 `paintEvent` 会被间接触发

   ```cpp
   DockWidgetTabStyle::DockWidgetTabStyle(ads::CDockWidget* DockWidget, QWidget* parent)
       : ads::CDockWidgetTab(DockWidget) {
       dark_ = etest::core_ui::ThemeManager::instance().isDarkTheme();
       connect(&etest::core_ui::ThemeManager::instance(),
               &etest::core_ui::ThemeManager::themeChanged,
               this, [this](bool) { onThemeChanged(); });
   }

   void DockWidgetTabStyle::onThemeChanged() {
       dark_ = etest::core_ui::ThemeManager::instance().isDarkTheme();
       update();
   }
   ```

3. **Tab 位置 & 选中态判定**【关键修正】

   通过公有 API 遍历确定当前 tab 在 tab bar 中的位置，映射到 `TabBarStyle` 所需的 `QStyleOptionTab::position` 和 `selectedPosition`：

   ```
   CDockWidgetTab
     ├── dockAreaWidget() → CDockAreaWidget*          (公有)
     │     └── titleBar() → CDockAreaTitleBar*          (公有)
     │           └── tabBar() → CDockAreaTabBar*         (公有)
     │                 └── tab(int) → CDockWidgetTab*    (公有, 遍历)
     └── isActiveTab() → bool                           (公有)
   ```

   ```cpp
   int DockWidgetTabStyle::tabIndexInBar() const {
       auto* area = dockAreaWidget();
       if (!area) return -1;
       auto* tabBar = area->titleBar()->tabBar();
       int count = tabBar->count();
       for (int i = 0; i < count; ++i) {
           if (tabBar->tab(i) == this) return i;
       }
       return -1;
   }
   ```

   **`position` 映射**（对应 `QStyleOptionTab::position`）：

   | TabBarStyle 位置 | QADS 判定 |
   |---|---|
   | `Beginning` | `index == 0 && count > 1` |
   | `Middle` | `0 < index < count - 1` |
   | `End` | `index == count - 1 && count > 1` |
   | `OnlyOneTab` | `count == 1` |

   **`selectedPosition` 映射**（对应 `QStyleOptionTab::selectedPosition`）：

   | TabBarStyle 选中邻位 | QADS 判定 |
   |---|---|
   | `NotAdjacent` | 前后 tab 均未选中 |
   | `NextIsSelected` | `index < count-1 && tabBar->tab(index+1)->isActiveTab()` |
   | `PreviousIsSelected` | `index > 0 && tabBar->tab(index-1)->isActiveTab()` |

   O(n) 遍历对 tab 数量（通常 < 20）无性能影响。

4. **`drawTabShape` —— 完全复刻 `TabBarStyle` 的梯形路径**

   完整移植 `TabBarStyle::getSelectedShape()` / `getHoveredShape()` / `getDividingLine()` 的 `QPainterPath` 几何计算。唯一变化：将 `QStyleOptionTab::rect` 替换为 `this->rect()`，将 `QStyleOptionTab::position` / `selectedPosition` 替换为上述映射结果。

   **⚠️ 裁剪约束**：`TabBarStyle` 的梯形路径包含超出 tab rect 的控制点（如 `r.left() - per` / `r.right() + per`），用于相邻 tab 间的视觉重叠。在 QADS 中每个 `CDockWidgetTab` 是独立 `QFrame` 子 widget，`QPainter` 自动裁剪到 `this->rect()`，超出部分不可见。因此需将所有控制点 clamp 到 `[0, width()]` 范围内：

   ```cpp
   // Before (TabBarStyle):
   p1 = QPointF(r.left() - per, r.bottom());
   p8 = QPointF(r.right() + per, p1.y());

   // After (DockWidgetTabStyle):
   p1 = QPointF(qMax(r.left() - per, 0.0), r.bottom());
   p8 = QPointF(qMin(r.right() + per, double(width())), p1.y());
   ```

   这样保留了梯形角度，但相邻 tab 之间不再有视觉重叠。原 `TabBarStyle` 中用于重叠延伸的 `per` 像素在边缘处被截平。

   ```cpp
   void DockWidgetTabStyle::drawTabShape(QPainter* painter) const {
       int idx = tabIndexInBar();
       bool active = isActiveTab();
       bool hovered = underMouse();

       if (active) {
           // 同 TabBarStyle::drawTabBarTabShape 的 State_Selected 分支
           auto path = selectedTabPath(rect(), tabPosition(idx, count));
           painter->setBrush(selectedBrush(rect()));
           painter->setPen(QPen(borderColor(), 1));
           painter->drawPolygon(path.toFillPolygon());
           if (dark_) {
               // 蓝色渐变描边（沿路径 pen stroke，非外发光）
               // 注意：borderColor() 在 dark 模式下返回 alpha=0 全透明，
               //       选中 tab 的视觉边框完全由此渐变描边提供
               QLinearGradient grad(rect().left(), 0, rect().right(), 0);
               grad.setColorAt(0.0,  QColor(0x0A, 0x3A, 0x5C));
               grad.setColorAt(0.15, QColor(0x40, 0xB0, 0xEE));
               grad.setColorAt(0.5,  QColor(0x90, 0xDD, 0xFF));
               grad.setColorAt(0.85, QColor(0x40, 0xB0, 0xEE));
               grad.setColorAt(1.0,  QColor(0x0A, 0x3A, 0x5C));
               painter->setPen(QPen(QBrush(grad), 1));
               painter->setBrush(Qt::NoBrush);
               painter->drawPath(path);
           }
       } else if (hovered) {
           auto path = hoveredTabPath(rect(), tabPosition(idx, count),
                                      selectedPosition(idx, count));
           painter->setPen(Qt::NoPen);
           painter->setBrush(hoveredColor());
           painter->drawPolygon(path.toFillPolygon());
       } else if (idx > 0) {
           auto line = dividingLine(rect(), tabPosition(idx, count));
           painter->setPen(QPen(dividerColor(), 1));
           painter->drawLine(line);
       }
   }
   ```

5. **色值复用**：
   - 主题色逻辑从 `TabBarStyle` 直接复制（`selectedBrush`、`hoveredColor`、`dividerColor`、`borderColor`）
   - 通过 `ThemeManager::instance().isDarkTheme()` 判断深浅
   - 在构造函数中连接 `ThemeManager::themeChanged` 信号调用 `update()`
   - **注意**：`borderColor()` 在 dark 模式下返回 `QColor(0, 0, 0, 0)`（全透明），选中 tab 的边框视觉完全由蓝色渐变描边提供；`textColor()` 不需要移植（文字颜色由 QSS 控制）

### 3.2.6 per-tab 方案的视觉缺陷

上述 §3.2.1–§3.2.5 的 per-tab 自绘方案存在根本性的视觉缺陷，无法达到与 `TabBarStyle` 一致的效果。

**原因**：`TabBarStyle` 能画出 Chrome 风格的关键在于 `QTabBar` 在**一次 `paintEvent` 中统一绘制所有 tab**，painter 可以访问整个 tab bar 的坐标空间。梯形路径的 `p1 = QPointF(r.left() - per, ...)` 和 `p8 = QPointF(r.right() + per, ...)` 向相邻 tab 的区域"借用"了 `per` 像素，形成底部圆弧无缝衔接的"熔合"效果。

QADS 中每个 `CDockWidgetTab` 是**独立 QWidget**，`QPainter` 硬裁剪到 `this->rect()`，无法越界绘制。§3.2.4 的 `qBound` clamp 只是让代码不崩的妥协，但视觉上：

- 相邻 tab 底部圆弧无法衔接，变成各自独立的"孤岛"
- 边缘处的斜角被截平，梯形角度变形
- Chrome 标志性的 tab 间"熔合"效果完全丢失

### 3.2.7 容器级统一绘制方案（推荐）

要达到与 `TabBarStyle` 完全一致的视觉效果，需将绘制层级从"每个 tab 自绘"提升到"容器统一绘制"。

**核心思路**：在 `tabsContainerWidget`（所有 `CDockWidgetTab` 的父 widget）的 `paintEvent` 中，用**统一 painter** 遍历所有子 tab 并绘制全部梯形路径。该 painter 的坐标空间覆盖全部 tab，不受单个 tab rect 裁剪限制，梯形路径可自由延伸到相邻 tab 区域。

**结构变化**：

```
CDockAreaTabBar : QScrollArea（自定义子类 DockAreaTabBarStyle）
  └─ viewport
       └─ tabsContainerWidget : QWidget  ← 在此层统一绘制所有 tab 形状
            └─ QBoxLayout
                 ├─ CDockWidgetTab : QFrame  ← paintEvent 不再绘制形状
                 ├─ CDockWidgetTab : QFrame  ← 只保留子控件（label、close button）
                 └─ stretch
```

**实现要点**：

1. **自定义 `CDockAreaTabBar` 子类**（如 `DockAreaTabBarStyle`）

   通过 Factory 的 `createDockAreaTabBar()` 返回自定义子类。构造函数中通过 `findChild<QWidget*>("tabsContainerWidget")` 获取容器 widget，对其 `installEventFilter(this)` 拦截 paint 事件：

   ```cpp
   class DockAreaTabBarStyle : public ads::CDockAreaTabBar {
       Q_OBJECT
   public:
       explicit DockAreaTabBarStyle(ads::CDockAreaWidget* parent);
   protected:
       bool eventFilter(QObject* watched, QEvent* event) override;
   private:
       // tab 位置枚举（映射自 QStyleOptionTab::Position）
       enum class TabPosition { Beginning, Middle, End, OnlyOne };
       enum class SelectedPosition { NotAdjacent, NextIsSelected, PreviousIsSelected };

       void paintAllTabs(QPainter* painter);
       [[nodiscard]] TabPosition mapPosition(int index, int count) const;
       [[nodiscard]] SelectedPosition mapSelectedPosition(int index, int count) const;

       // 路径计算（从 TabBarStyle 移植，参数改为 QRectF + TabPosition）
       QPainterPath selectedTabPath(const QRectF& r, TabPosition pos) const;
       QPainterPath hoveredTabPath(const QRectF& r, TabPosition pos,
                                   SelectedPosition sel) const;
       QLineF dividingLine(const QRectF& r, TabPosition pos) const;

       // 色值辅助（从 TabBarStyle 移植）
       QBrush selectedBrush(const QRect& tab_rect) const;
       QColor hoveredColor() const;
       QColor dividerColor() const;
       QColor borderColor() const;

       void onThemeChanged();

       QWidget* tabsContainer_ = nullptr;
       static constexpr qreal kTopMargin = 0.0;
       static constexpr qreal kHRatio = 1.0 / 5.0;
       bool dark_ = true;
   };
   ```

   ```cpp
   DockAreaTabBarStyle::DockAreaTabBarStyle(ads::CDockAreaWidget* parent)
       : ads::CDockAreaTabBar(parent) {
       tabsContainer_ = findChild<QWidget*>("tabsContainerWidget");
       if (tabsContainer_) {
           tabsContainer_->installEventFilter(this);
       }
       // 主题切换
       dark_ = etest::core_ui::ThemeManager::instance().isDarkTheme();
       connect(&etest::core_ui::ThemeManager::instance(),
               &etest::core_ui::ThemeManager::themeChanged,
               this, [this](bool dark) {
                   dark_ = dark;
                   if (tabsContainer_) tabsContainer_->update();
               });
   }

   bool DockAreaTabBarStyle::eventFilter(QObject* watched, QEvent* event) {
       if (watched == tabsContainer_ && event->type() == QEvent::Paint) {
           QPainter painter(tabsContainer_);
           painter.setRenderHint(QPainter::Antialiasing);
           paintAllTabs(&painter);
           return true;  // 吃掉事件，阻止 tabsContainerWidget 默认 paintEvent
                        // 子控件（CDockWidgetTab）有独立 paintEvent，不受影响
       }
       return ads::CDockAreaTabBar::eventFilter(watched, event);
   }
   ```

2. **`paintAllTabs` -- 遍历所有子 tab，用统一坐标空间绘制**

   ```cpp
   void DockAreaTabBarStyle::paintAllTabs(QPainter* painter) {
       auto* tabBar = this;  // CDockAreaTabBar 自身
       int count = tabBar->count();
       for (int i = 0; i < count; ++i) {
           auto* tab = tabBar->tab(i);
           if (!tab) continue;

           // tab 的 rect 需转换到 tabsContainerWidget 坐标空间
           QRectF r = tab->geometry();  // 已是 tabsContainerWidget 子 widget 的坐标

           bool active = tab->isActiveTab();
           bool hovered = tab->underMouse();

           // 复用 TabBarStyle 的路径计算，传入 r 和 position/selectedPosition
           // 关键：路径控制点可以超出 r，painter 不裁剪！
           auto pos = mapPosition(i, count);
           auto sel = mapSelectedPosition(i, count);

           if (active) {
               auto path = selectedTabPath(r, pos);
               painter->setBrush(selectedBrush(r.toRect()));
               painter->setPen(QPen(borderColor(), 1));
               painter->drawPolygon(path.toFillPolygon());
               if (dark_) {
                   // 蓝色渐变描边
                   ...
               }
           } else if (hovered) {
               auto path = hoveredTabPath(r, pos, sel);
               painter->setPen(Qt::NoPen);
               painter->setBrush(hoveredColor());
               painter->drawPolygon(path.toFillPolygon());
           } else if (i > 0) {
               auto line = dividingLine(r, pos);
               painter->setPen(QPen(dividerColor(), 1));
               painter->drawLine(line);
           }
       }
   }
   ```

   **关键优势**：`r` 是 tab 在 `tabsContainerWidget` 坐标空间中的 geometry，路径控制点 `r.left() - per` / `r.right() + per` 可以自由延伸到相邻 tab 区域，与 `TabBarStyle` 的行为完全一致。**无需 `qBound` 裁剪**。

3. **`CDockWidgetTab` 的 `paintEvent` 不再绘制形状**

   per-tab 方案中的 `DockWidgetTabStyle` 仍需保留（用于 Factory 注入），但 `paintEvent` 简化为**不绘制任何形状**（形状已由容器统一绘制）：

   ```cpp
   void DockWidgetTabStyle::paintEvent(QPaintEvent* event) {
       // 不调 QFrame::paintEvent，也不绘制形状
       // 形状由 DockAreaTabBarStyle::paintAllTabs 统一绘制
       // 子控件（CElidingLabel、关闭按钮）各自独立收 paintEvent，不受影响
   }
   ```

   或者更简单地，`DockWidgetTabStyle` 可以完全不要，Factory 的 `createDockWidgetTab()` 走默认实现。但保留它便于将来按需扩展 per-tab 的特殊绘制逻辑。

4. **重绘协调**

   当 tab 的 active/hover 状态变化时，需触发容器重绘。两个路径：
   - **hover**：QADS 的 `CDockWidgetTab` 在 `enterEvent` / `leaveEvent` 中已有处理，但不会通知容器。可通过 `DockAreaTabBarStyle` 对每个子 tab `installEventFilter`，或重写 `CDockWidgetTab::event()` 拦截 `Enter`/`Leave` 事件后调用 `tabsContainer_->update()`
   - **active**：`setActiveTab()` 调用 `updateStyle()` 后，QADS 内部 repolish 触发 `styleChange`，Qt 默认调 `update()`。但需确保容器也 `update()`。可在 `DockAreaTabBarStyle` 中重写 `eventFilter` 拦截子 tab 的 `StyleChange` 事件

   简化方案：`DockAreaTabBarStyle` 对所有子 tab 安装 eventFilter，拦截 `Paint` / `Enter` / `Leave` / `StyleChange` 事件，统一触发 `tabsContainer_->update()`。

5. **坐标空间**

   `tabsContainerWidget` 的 `paintEvent` 中，painter 坐标原点是容器左上角。子 tab 的 `geometry()` 返回的就是相对于容器的坐标，可直接用于路径计算，无需额外坐标转换。

   但需注意 `CDockAreaTabBar` 是 `QScrollArea`，`tabsContainerWidget` 可能比 viewport 宽（tab 被滚动）。painter 会自动处理滚动偏移，路径绘制不受影响。

### 3.2.8 两方案对比

| 维度 | per-tab 自绘（§3.2.1–3.2.5） | 容器级统一绘制（§3.2.7） |
|------|-------------------------------|--------------------------|
| 视觉效果 | ❌ tab 间无法熔合，梯形截平 | ✅ 与 `TabBarStyle` 完全一致 |
| 裁剪约束 | 需 `qBound` clamp，角度变形 | 不需裁剪，路径自由延伸 |
| 侵入性 | 低（只 override tab 的 paintEvent） | 中（需自定义 tab bar 子类 + eventFilter） |
| 重绘协调 | 简单（tab 自管） | 需协调容器与子 tab 的重绘时机 |
| 代码复用 | 直接复用 `TabBarStyle` 色值/路径 | 同样复用，但路径参数从 `QStyleOptionTab` 改为手动构造 |
| hover 检测 | `underMouse()` 直接可用 | 需遍历子 tab 的 `underMouse()` |
| 推荐度 | ⚠️ 妥协方案 | ✅ 推荐方案 |

### 3.3 新增：`EtestComponentsFactory`

```cpp
// src/libui/styles/EtestComponentsFactory.h
#include "DockComponentsFactory.h"  // QADS 头文件

class EtestComponentsFactory : public ads::CDockComponentsFactory {
public:
    ads::CDockWidgetTab* createDockWidgetTab(
        ads::CDockWidget* DockWidget) const override;
    ads::CDockAreaTabBar* createDockAreaTabBar(
        ads::CDockAreaWidget* DockArea) const override;
};
```

```cpp
// src/libui/styles/EtestComponentsFactory.cpp
#include "EtestComponentsFactory.h"
#include "DockAreaTabBarStyle.h"
#include "DockWidgetTabStyle.h"

ads::CDockWidgetTab* EtestComponentsFactory::createDockWidgetTab(
    ads::CDockWidget* DockWidget) const
{
    return new DockWidgetTabStyle(DockWidget);
}

ads::CDockAreaTabBar* EtestComponentsFactory::createDockAreaTabBar(
    ads::CDockAreaWidget* DockArea) const
{
    return new DockAreaTabBarStyle(DockArea);
}
```

### 3.4 注册 Factory & ThemeManager 初始化时序

在 `main.cpp` 应用初始化的最早阶段：

```cpp
#include "libui/styles/EtestComponentsFactory.h"
#include "core_ui/ThemeManager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 1. 提前初始化 ThemeManager（加载 QSS，确保在 QADS widget 创建前完成）
    //    必须在 setFactory 之前调用：
    //    ThemeManager 构造函数内调 qApp->setStyleSheet()，会重置所有已创建 widget 的 QPalette。
    //    若 tabbar 已存在时再加载 QSS，其 palette 会被冲掉。
    etest::core_ui::ThemeManager::instance();

    // 2. 注册自定义 Factory（必须在任何 CDockWidget 创建之前）
    ads::CDockComponentsFactory::setFactory(new EtestComponentsFactory());

    // ...
}
```

**⚠️ ThemeManager `loadQss()` 修正**：

`ThemeManager::loadQss()` 末尾需新增 `emit themeChanged(is_dark_)`。

原因：`loadQss()` 内调 `qApp->setStyleSheet()` 会重置所有 widget 的 QPalette。`setTheme()` 中已有 `emit themeChanged`，但构造函数中的首次 `loadQss()` 调用没有 emit。添加后，所有连接 `themeChanged` 的 slot（包括 `DockWidgetTabStyle::onThemeChanged`）能在 QSS 加载完毕后重新设置 palette / 触发重绘。

### 3.5 QSS 清理

**只改 `ads_dark.qss`，不动 `default.css`。**

> **⚠️ 关键修正**：empty `paintEvent` **不能**阻止 QSS background 绘制。`QStyleSheetStyle` 对 QSS 匹配的 widget 设置 `WA_StyledBackground`，Qt 在 `paintEvent` 之前自动绘制 QSS 背景。因此 `default.css` 和 `ads_dark.qss` 中 `ads--CDockWidgetTab` 的 `background` 规则**不会自动失效**，必须显式处理。
>
> 同时注意 `default.css` 设置在 `CDockManager` 级别（widget stylesheet），优先级**高于** `qApp->setStyleSheet()`。即使 `ads_dark.qss` 删除了 `background` 规则，`default.css` 的 `background: palette(window)` 仍会生效。

需要修改 `src/app/resources/styles/ads_dark.qss`：

| 选择器 | 操作 | 原因 |
|--------|------|------|
| `ads--CDockWidgetTab` 的 `background` | **改为 `transparent`**（不能删除，否则 `default.css` 的 `palette(window)` 补位） | 阻止 QSS 背景覆盖容器绘制的梯形形状 |
| `ads--CDockWidgetTab` 的 `border-color/border-style/border-width` | **改为 `none`** | 边框由容器 `paintAllTabs` 绘制 |
| `ads--CDockWidgetTab[activeTab="true"]` 的 `background` | **改为 `transparent`** | 选态背景由容器绘制 |
| `ads--CDockWidgetTab[activeTab="true"]` 的 `border-top` | **删除** | 高亮线由容器绘制 |
| `ads--CDockWidgetTab:hover` 的 `background` | **改为 `transparent`** | hover 背景由容器绘制 |
| `ads--CDockWidgetTab QLabel` 的 `color` | **保留** | 文字颜色仍由 QSS 控制 |
| `ads--CDockWidgetTab[activeTab="true"] QLabel` 的 `color` | **保留** | 选态文字颜色 |
| `ads--CDockWidgetTab:hover QLabel` 的 `color` | **保留** | hover 文字颜色 |

此外，在 `DockWidgetTabStyle` 构造函数中需同时设置：
```cpp
setAttribute(Qt::WA_StyledBackground, false);
setAttribute(Qt::WA_NoSystemBackground, true);
```
与 QSS `background: transparent` 双保险，确保子 tab 不绘制任何背景，容器的梯形形状直接可见。

## 四、`TabBarStyle` 代码复用清单

| `TabBarStyle` 成员 | 复用方式 |
|-------------------|----------|
| `selectedBrush(const QRect&)` | 直接复制 |
| `hoveredColor()` | 直接复制 |
| `dividerColor()` | 直接复制 |
| `borderColor()` | 直接复制 |
| `textColor(bool selected)` | 不需要（文字颜色由 QSS 控制） |
| `getSelectedShape()` | 完整移植，`QStyleOptionTab::rect` → `QRectF`（tab geometry），`position` → `TabPosition` 映射 |
| `getHoveredShape()` | 同上 |
| `getDividingLine()` | 完整移植，`QStyleOptionTab::rect` → `QRectF`（tab geometry），`position` → `TabPosition` 映射 |
| `sizeFromContents()` | 不需要（尺寸由 QADS 内部管理） |
| `drawTabBarTabShape()` | 完整移植到 `drawTabShape()` |
| `drawTabBarTabLabel()` | 不需要（子控件 CElidingLabel 负责文字绘制） |

## 五、未覆盖场景与边界情况

1. **拖拽中的 tab**：拖拽时 tab 通常仍在 dock area 内，`dockAreaWidget()` 一般不返回 nullptr。但防御性检查仍推荐：若返回 nullptr，`paintEvent` 退化为不绘制自定义形状，子控件仍正常绘制
2. **`CDockAreaTabBar` 背景色问题（未解决）**：`CDockAreaTabBar` 是 `QScrollArea`，其 viewport 在深色主题下默认白/灰背景。尝试过 QSS `background: transparent`、QPalette + `autoFillBackground`、`paintEvent` override 三种方案，均因 `qApp->setStyleSheet()` 重置 palette 或 QSS 对 QScrollArea viewport 无效而失败。此问题暂未解决，需后续单独处理
3. **选中位置判定边界**：`tabBar->tab(index±1)->isActiveTab()` 在 tab 刚插入/移除时可能存在竞态，但 QADS 通过 signal/slot 顺序化保证了 `updateTabs()` -> `setActiveTab()` 的顺序，不会出现不一致
4. **关闭按钮样式**：关闭按钮保持 QADS 默认（QSS 通过 `#tabCloseButton` 控制），不涉及修改
5. **深色/浅色切换**：`paintEvent` 通过 `ThemeManager` 获取当前主题，切换时 `update()` 重绘
6. **`default.css` 的 `ads--CDockWidgetTab` 样式**：`default.css` 的 `background: palette(window)` 优先级高于 `qApp` 级 QSS，且 empty `paintEvent` 不能阻止 QSS background 绘制。必须通过 QSS 显式覆盖为 `transparent` + `setAttribute(WA_StyledBackground, false)` 双保险处理（详见 §3.5）
7. **`FocusHighlighting` 模式**：QADS 的 `focused` 属性仍然生效（`repolishStyle` 刷新子控件聚焦样式），不影响自定义背景绘制

## 六、实施顺序

1. 修改 `ThemeManager.cpp`：`loadQss()` 末尾添加 `emit themeChanged(is_dark_)`
2. 编写 `DockWidgetTabStyle.h/.cpp`（`paintEvent` 留空，仅保留 `themeChanged` 连接）
3. 编写 `DockAreaTabBarStyle.h/.cpp`（自定义 `CDockAreaTabBar` 子类，eventFilter 拦截 `tabsContainerWidget` paint 事件，`paintAllTabs` 统一绘制）
4. 编写 `EtestComponentsFactory.h/.cpp`（重写 `createDockWidgetTab()` + `createDockAreaTabBar()`）
5. 修改 `main.cpp`：`ThemeManager::instance()` 提前初始化 + 注册 Factory（必须在任何 `CDockWidget` 创建之前）
6. 清理 `ads_dark.qss` 中 tab 背景/边框规则，保留子控件文字颜色
7. 编译验证

## 七、不采用方案及其理由

### 7.1 直接修改 QADS 源码

在 `CDockWidgetTab` 中直接加 `paintEvent` 重写。

**不采用**：每次升级 QADS 都要重新改，维护成本高。

### 7.2 eventFilter 拦截每个 `CDockWidgetTab` 的 Paint 事件

对每个 `CDockWidgetTab` 实例 `installEventFilter`。

**不采用**：
- 需要追踪所有 tab 的创建和销毁，生命周期管理复杂
- Factory 方案已经很干净，没有必要用 hack 方式

> **注**：§3.2.7 容器级方案**部分采用**了 eventFilter 思路，但拦截对象从单个 `CDockWidgetTab` 改为 `tabsContainerWidget`（容器级），避免了逐 tab 追踪的生命周期问题。同时 §3.2.7 点 4 中对子 tab 的 eventFilter 仅用于 hover/active 重绘协调，不用于 paint 拦截。

### 7.3 纯 QSS 实现

用 QSS 的 `border-image`、`border-radius` 等属性模拟圆角梯形。

**不采用**：QSS 无法绘制非对称的梯形形状和不规则渐变，效果达不到 Chrome 风格。

### 7.4 继承 `CDockAreaTabBar` 并在其中绘制所有 tab

代替每个 `CDockWidgetTab` 各自绘制。

**不采用**：与 QADS 的设计不符，需要完全重写 tab bar 布局逻辑，侵入性过大。

## 八、审查结论

对方案文档进行逐条源码审查后，结论如下：

| # | 审查项 | 结论 | 说明 |
|---|--------|------|------|
| 1 | 子控件能否独立绘制 | ✅ | 每个子控件有独立 paintEvent |
| 2 | frameShape 默认值 | ✅ | 默认为 NoFrame |
| 3 | default.css 背景自动失效 | **❌ 错误** | empty `paintEvent` 不能阻止 QSS background 绘制，详见 #17 |
| 4 | updateStyle 是否可 override | **❌ 非虚函数** | `DockWidgetTab.h:162` 无 virtual，改用 `themeChanged` 信号触发重绘 |
| 5 | 公有 API 链可访问性 | ✅ | dockAreaWidget -> titleBar -> tabBar -> tab 全公有 |
| 6 | isActiveTab() 调用安全 | ✅ | 纯 getter，无递归风险 |
| 7 | dockAreaWidget() nullptr 场景 | ⚠️ | 拖拽时通常不 nullptr，防御性检查仍推荐 |
| 8 | Factory 注册时机 | ✅ | main.cpp 开头来得及 |
| 9 | QSS 优先级 | ⚠️ | `ads_dark.qss` 设到 `qApp`，但 `default.css` 设到 `CDockManager`，后者优先级更高，需显式覆盖 |
| 10 | 梯形重叠被裁剪 | **❌ per-tab 方案已弃用** | per-tab 的 `qBound` clamp 导致视觉缺陷，改用容器级统一绘制（§3.2.7） |
| 11 | CElidingLabel 文字绘制 | ✅ | 位置差异可接受 |
| 12 | 竞态条件 | ✅ | Qt 事件模型保证一致性 |
| 13 | ThemeManager 初始化时序 | **⚠️ 需处理** | `ThemeManager::instance()` 必须在 `setFactory()` 之前调用 |
| 14 | loadQss() emit themeChanged | **⚠️ 需处理** | 构造函数首次加载 QSS 后需 emit，否则 slot 不被通知 |
| 15 | CDockAreaTabBar 背景色 | **❌ 未解决** | QScrollArea viewport 深色主题下白/灰背景，QSS/QPalette/paintEvent 均未生效 |
| 16 | 容器级统一绘制可行性 | **⚠️ 需附加条件** | eventFilter 可行，但子 tab 的 QSS background 会覆盖容器绘制（#17），必须同时处理 |
| 17 | empty paintEvent 能否阻止 QSS background | **❌ 不能** | QSS 匹配的 widget 会被 `QStyleSheetStyle` 设置 `WA_StyledBackground`，Qt 在 `paintEvent` 之前自动绘制 QSS 背景。empty `paintEvent` 无法阻止 |
| 18 | default.css 优先级 | **⚠️ 需显式覆盖** | `default.css` 设置在 `CDockManager` 级别（widget stylesheet），优先级高于 `qApp->setStyleSheet()`。即使 `ads_dark.qss` 删了 `background`，`default.css` 的 `background: palette(window)` 仍生效 |
| 19 | hover 重绘协调 | **⚠️ 需 eventFilter** | `CDockWidgetTab` 内部处理 `enterEvent`/`leaveEvent` 但不通知父容器。需对子 tab 安装 eventFilter 拦截 `Enter`/`Leave` 事件后触发 `tabsContainer_->update()` |

### 8.1 必须处理的修正

- **#3/#17 empty paintEvent 不能阻止 QSS background**：`QStyleSheetStyle` 对 QSS 匹配的 widget 设置 `WA_StyledBackground`，Qt 在 `paintEvent` 之前自动绘制 QSS 背景。容器级方案（§3.2.7）中子 tab 的 QSS 纯色背景矩形会**覆盖**容器绘制的梯形形状。必须同时执行以下操作之一：
  - **方案 A（推荐）**：在 `DockWidgetTabStyle` 构造函数中 `setAttribute(Qt::WA_StyledBackground, false)` + `setAttribute(Qt::WA_NoSystemBackground, true)`，阻止 QSS 背景绘制
  - **方案 B**：`ads_dark.qss` 中将 `ads--CDockWidgetTab` 的 `background` 显式改为 `transparent`（覆盖 `default.css` 的 `palette(window)`）
  - 两者可同时使用以确保万无一失
- **#4 updateStyle 非虚函数**：删掉 override，改用 `themeChanged` 信号 -> `update()` 触发重绘
- **#9/#18 default.css 优先级**：`default.css` 设在 `CDockManager` 级别，优先级高于 `qApp->setStyleSheet()`。`ads_dark.qss` 必须显式覆盖 `default.css` 的 `background` 规则，不能简单删除
- **#10 路径几何裁剪**：per-tab 方案的 `qBound` clamp 导致视觉缺陷，**改用容器级统一绘制方案（§3.2.7）**，painter 不受单 tab rect 裁剪
- **#13 ThemeManager 初始化时序**：`main.cpp` 中 `ThemeManager::instance()` 必须在 `setFactory()` 之前
- **#14 loadQss emit**：`ThemeManager::loadQss()` 末尾添加 `emit themeChanged(is_dark_)`
- **#16 容器级方案附加条件**：eventFilter 拦截 `tabsContainerWidget` 的 paint 事件可行，但必须同时处理 #17/#18 的 QSS background 覆盖问题
- **#19 hover 重绘协调**：对子 tab 安装 eventFilter 拦截 `Enter`/`Leave` 事件，触发 `tabsContainer_->update()`

### 8.2 未解决问题

- **#15 CDockAreaTabBar 背景色**：QScrollArea 的 viewport 背景在深色主题下无法通过 QSS/QPalette/paintEvent 有效覆盖。`qApp->setStyleSheet()` 会重置所有 widget 的 QPalette，导致 C++ 设置的 palette 失效。此问题需后续单独研究解决方案。

### 8.3 总结

方案整体可行，`updateStyle` 非虚和 ThemeManager 时序问题已修正。per-tab 自绘方案（§3.2.1–3.2.5）因 `QPainter` 硬裁剪无法实现 Chrome 风格的 tab 间熔合效果，**推荐改用容器级统一绘制方案（§3.2.7）**：在 `tabsContainerWidget` 层用统一 painter 遍历所有子 tab 绘制，路径控制点可自由延伸到相邻 tab 区域，与 `TabBarStyle` 行为完全一致。

**容器级方案的实施前提**：必须同时解决子 tab 的 QSS background 覆盖问题（#17/#18）--否则容器绘制的梯形被子 tab 的纯色矩形遮盖。推荐 `setAttribute(WA_StyledBackground, false)` + QSS `background: transparent` 双保险。

`CDockAreaTabBar` 背景色问题（#15）是唯一未解决的阻塞项，但不影响 tab 自绘形状的正确性，仅影响 tab 间空隙的背景色。建议实现前先用一个最小原型验证容器级绘制的视觉效果。
