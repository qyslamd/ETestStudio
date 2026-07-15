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

## 三、源码审查后修正的方案

经过对 QADS 源码的完整审查，确认方案可行，但有若干关键修正。以下为修正后的方案。

### 3.1 总览

| 步骤 | 文件 | 说明 |
|------|------|------|
| 1 | `src/libui/styles/DockWidgetTabStyle.h` | 新类，继承 `ads::CDockWidgetTab`，重写 `paintEvent` + `updateStyle` |
| 2 | `src/libui/styles/DockWidgetTabStyle.cpp` | 绘制实现，复用 `TabBarStyle` 色值逻辑 |
| 3 | `src/libui/styles/EtestComponentsFactory.h` | 新类，继承 `ads::CDockComponentsFactory` |
| 4 | `src/libui/styles/EtestComponentsFactory.cpp` | 仅重写 `createDockWidgetTab()` |
| 5 | 修改 `main.cpp` 或应用初始化处 | 注册自定义 Factory |
| 6 | 修改 `src/app/resources/styles/ads_dark.qss` | 删 tab 背景/边框，保留子控件文字颜色 |

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
    void updateStyle() override;
private:
    [[nodiscard]] int tabIndexInBar() const;
    void drawTabShape(QPainter* painter) const;
    // 色值辅助（从 TabBarStyle 移植）
    QBrush selectedBrush() const;
    QColor hoveredColor() const;
    QColor textColor(bool selected) const;
    QPainterPath tabPath(const QRect& r, bool isFirst, bool isLast) const;
    bool dark_ = true;
};
```

cpp 实现要点：

1. **`paintEvent` —— 完全不调 `QFrame::paintEvent`** 【关键修正】

   ```cpp
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
   - QADS 内置 `default.css` 的 `ads--CDockWidgetTab { background }` **自动失效**（因为走不到 QFrame 的绘制路径）
   - 子控件文字颜色仍由 QSS 的 `ads--CDockWidgetTab QLabel { color }` 控制（子控件独立绘制）

2. **`updateStyle()` 必须 override** 【关键修正】

   QADS 在 `setActiveTab()` 时调用 `updateStyle()`：

   ```cpp
   void CDockWidgetTab::updateStyle() {
       internal::repolishStyle(this, internal::RepolishDirectChildren);
   }
   ```

   默认实现只做了样式重算，但我们的 `paintEvent` 依赖的是 C++ 成员变量（`dark_`、`isActiveTab()`），不是 QSS。因此：

   ```cpp
   void DockWidgetTabStyle::updateStyle() override {
       ads::CDockWidgetTab::updateStyle();  // repolish 子控件（更新文字颜色）
       update();                            // 触发我们的 paintEvent
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
           auto path = selectedTabPath(idx);
           painter->setBrush(selectedBrush(rect()));
           painter->setPen(QPen(borderColor(), 1));
           painter->drawPolygon(path.toFillPolygon());
           if (dark_) {
               // 绘制蓝色外发光边框（渐变）
               ...
           }
       } else if (hovered) {
           auto path = hoveredTabPath(idx);
           painter->setPen(Qt::NoPen);
           painter->setBrush(hoveredColor());
           painter->drawPolygon(path.toFillPolygon());
       } else if (idx > 0) {
           auto line = dividingLine(idx);
           painter->setPen(QPen(dividerColor(), 1));
           painter->drawLine(line);
       }
   }
   ```

5. **色值复用**：
   - 主题色逻辑从 `TabBarStyle` 直接复制（`selectedBrush`、`hoveredColor`、`borderColor` 等）
   - 通过 `ThemeManager::instance().isDarkTheme()` 判断深浅
   - 在构造函数中连接 `ThemeManager::themeChanged` 信号调用 `update()`

### 3.3 新增：`EtestComponentsFactory`

```cpp
// src/libui/styles/DockComponentsFactory.h
class EtestComponentsFactory : public ads::CDockComponentsFactory {
public:
    ads::CDockWidgetTab* createDockWidgetTab(ads::CDockWidget* DockWidget) const override;
    // 其余两个工厂方法走默认
};
```

```cpp
// src/libui/styles/DockComponentsFactory.cpp
ads::CDockWidgetTab* EtestComponentsFactory::createDockWidgetTab(
    ads::CDockWidget* DockWidget) const
{
    return new DockWidgetTabStyle(DockWidget);
}
```

### 3.4 注册 Factory

在 `main.cpp` 或 `ETestStudio` 应用初始化的最早阶段：

```cpp
#include "DockComponentsFactory.h"  // 我们的，不是 QADS 的

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // 必须在任何 DockWidget 创建之前
    ads::CDockComponentsFactory::setFactory(new EtestComponentsFactory());
    // ...
}
```

### 3.5 QSS 清理

**只改 `ads_dark.qss`，不动 `default.css`。**

由于 `paintEvent` 完全不调 `QFrame::paintEvent`，QADS 内置 `default.css` 中 `ads--CDockWidgetTab` 的 `background` / `border` 规则**自动失效**，无需处理。

需要修改 `src/app/resources/styles/ads_dark.qss`：

| 选择器 | 操作 | 原因 |
|--------|------|------|
| `ads--CDockWidgetTab` 的 `background` | 删除 | 背景由 `paintEvent` 绘制 |
| `ads--CDockWidgetTab` 的 `border-color/border-style/border-width` | 删除 | 边框由 `paintEvent` 绘制 |
| `ads--CDockWidgetTab[activeTab="true"]` 的 `background` | 删除 | 选态背景由 `paintEvent` 绘制 |
| `ads--CDockWidgetTab[activeTab="true"]` 的 `border-top`（蓝色高亮线） | 删除 | 高亮线由 `paintEvent` 绘制 |
| `ads--CDockWidgetTab:hover` 的 `background` | 删除 | hover 背景由 `paintEvent` 绘制 |
| `ads--CDockWidgetTab QLabel` 的 `color` | **保留** | 文字颜色仍由 QSS 控制 |
| `ads--CDockWidgetTab[activeTab="true"] QLabel` 的 `color` | **保留** | 选态文字颜色 |
| `ads--CDockWidgetTab:hover QLabel` 的 `color` | **保留** | hover 文字颜色 |

## 四、`TabBarStyle` 代码复用清单

| `TabBarStyle` 成员 | 复用方式 |
|-------------------|----------|
| `selectedBrush()` | 直接复制 |
| `hoveredColor()` | 直接复制 |
| `dividerColor()` | 直接复制 |
| `borderColor()` | 直接复制 |
| `textColor(bool selected)` | 直接复制 |
| `getSelectedShape()` | 完整移植，`QStyleOptionTab::rect` → `this->rect()`，`position` → tabIndex 映射 |
| `getHoveredShape()` | 同上 |
| `getDividingLine()` | 完整移植，`QStyleOptionTab::rect` → `this->rect()`，`position` → tabIndex 映射 |
| `sizeFromContents()` | 不需要（尺寸由 QADS 内部管理） |
| `drawTabBarTabShape()` | 完整移植到 `drawTabShape()` |
| `drawTabBarTabLabel()` | 不需要（子控件 CElidingLabel 负责文字绘制） |

## 五、未覆盖场景与边界情况

1. **拖拽中的 tab**：拖拽时 tab 移出 layout，`dockAreaWidget()` 返回 nullptr，此时 `paintEvent` 退化为不绘制自定义形状，子控件仍正常绘制
2. **选中位置判定边界**：`tabBar->tab(index±1)->isActiveTab()` 在 tab 刚插入/移除时可能存在竞态，但 QADS 通过 signal/slot 顺序化保证了 `updateTabs()` → `setActiveTab()` 的顺序，不会出现不一致
3. **关闭按钮样式**：关闭按钮保持 QADS 默认（QSS 通过 `#tabCloseButton` 控制），不涉及修改
4. **深色/浅色切换**：`paintEvent` 通过 `ThemeManager` 获取当前主题，切换时 `update()` 重绘
5. **`default.css` 的 `ads--CDockWidgetTab` 样式**：由于 `paintEvent` 跳过 `QFrame::paintEvent`，这些规则（background、border）自动失效，无需手动清理
6. **`FocusHighlighting` 模式**：QADS 的 `focused` 属性仍然生效（`repolishStyle` 刷新子控件聚焦样式），不影响自定义背景绘制

## 六、实施顺序

1. 编写 `DockWidgetTabStyle.h/.cpp`（自定义 `paintEvent` + `updateStyle` override）
2. 编写 `EtestComponentsFactory.h/.cpp`（自定义 Factory）
3. 在 `main.cpp` 注册 Factory（必须在任何 `CDockWidget` 创建之前）
4. 清理 `ads_dark.qss` 中 tab 背景/边框规则，保留子控件文字颜色
5. 编译验证

## 七、不采用方案及其理由

### 7.1 直接修改 QADS 源码

在 `CDockWidgetTab` 中直接加 `paintEvent` 重写。

**不采用**：每次升级 QADS 都要重新改，维护成本高。

### 7.2 eventFilter 拦截 Paint 事件

对每个 `CDockWidgetTab` 实例 `installEventFilter`。

**不采用**：
- 需要追踪所有 tab 的创建和销毁，生命周期管理复杂
- Factory 方案已经很干净，没有必要用 hack 方式

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
| 3 | default.css 背景自动失效 | ✅ | 跳过 QFrame::paintEvent 即绕过 |
| 4 | updateStyle override | ✅ | 需同时调基类 + update() |
| 5 | 公有 API 链可访问性 | ✅ | dockAreaWidget → titleBar → tabBar → tab 全公有 |
| 6 | isActiveTab() 调用安全 | ✅ | 纯 getter，无递归风险 |
| 7 | dockAreaWidget() nullptr 场景 | ⚠️ | 拖拽时通常不 nullptr，防御性检查仍推荐 |
| 8 | Factory 注册时机 | ✅ | main.cpp 开头来得及 |
| 9 | QSS 优先级 | ✅ | ads_dark.qss 附加在后，覆盖 default.css |
| 10 | 梯形重叠被裁剪 | **❌ 已处理** | 约束控制点至 [0, width()] 范围 |
| 11 | CElidingLabel 文字绘制 | ✅ | 位置差异可接受 |
| 12 | 竞态条件 | ✅ | Qt 事件模型保证一致性 |

### 8.1 必须处理的修正

- **#10 路径几何裁剪**：已在本方案 §3.2.4 中写入约束处理

### 8.2 总结

方案整体可行，未发现架构性障碍。建议实现前先用一个最小原型验证约束后的梯形视觉效果可接受。
