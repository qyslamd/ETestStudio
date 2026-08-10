# AnimationDialog 精简方案

## 问题陈述

1. **Linux 原生对话框标题栏/边框难看**：WSL2 Ubuntu 下 Qt 默认原生窗口装饰
   （标题栏、边框）与自绘 UI 格格不入。改用 AnimationDialog 的**无边框卡片 + 遮罩**
   可统一 Windows/Linux 外观。
2. **飞入/飞出动画花里胡哨**：`actShowAnimation`/`actHideAnimation` 的随机方向
   500ms 移动动画是炫技，拖慢节奏、无确定性。需移除，保留「模态遮罩聚焦」价值。

目标：AnimationDialog 精简为「无边框圆角卡片 + 遮罩 + 卡片阴影」，显示/关闭即时，
不依赖原生窗口装饰，跨平台统一外观。

## 架构回顾

- `AnimationDialog`（`src/app/dialogs/AnimationDialog.cpp`）：QDialog 子类，
  `Qt::FramelessWindowHint` + `WA_TranslucentBackground`；Linux 下退化为父窗口
  子覆盖层并 re-parent 到顶层窗口（保证遮罩覆盖主窗口）。
- `setWidget(QWidget*)`：内容卡片 + `QGraphicsDropShadowEffect` 阴影。
- `showEvent`：Win 设几何为主窗口全屏、Linux 设为主窗口客户区；随后
  `actShowAnimation()`。
- `actShowAnimation`：截图 → 隐藏卡片 → 随机方向 500ms 飞入 → 居中显示卡片。
- `actHideAnimation()`：默认 `actHideAnimation([] { accept(); })`；截图 → 隐藏
  卡片 → 500ms 飞出 → 回调 + 发 `hideAnimationFinished`。
- 调用面（均已确认）：`AboutDialog:157`、`LoginDialog:84/133`、
  `UserManagerDialog:54` 仅调无参 `actHideAnimation()`；`hideAnimationFinished`
  无外部消费；`actShowAnimation` 仅 `showEvent` 内部调用。

## 方案选项及理由

### 动画去留
- **选项 A（选定）**：完全移除飞入/飞出动画，显示/关闭即时。
  理由：最简、无延迟；遮罩聚焦价值已足够，淡入等可后续按需加。
- 选项 B 留浅淡入（150ms 透明度）：不选（用户倾向最简，后续可加）。

### 遮罩与外观
- **选项 A（选定）**：保留无边框 + 半透明 + 遮罩（paintEvent 灰色填充）+ 卡片
  阴影，实现跨平台统一外观（Linux 下无原生标题栏/边框）。

## 决策记录

1. `showEvent`：删除 `actShowAnimation()` 调用，改为直接居中显示卡片：
   ```
   if (widget_) {
     int w = widget_->width();
     int h = widget_->height();
     widget_->move((width() - w) / 2, (height() - h) / 2);
     widget_->show();
   }
   ```
   几何设置（Win 全屏 / Linux 客户区）与 `raise()` 保留。
2. 删除 `actShowAnimation()`（含截图/快照/QVariantAnimation 飞入逻辑）及其
   头文件声明；`showEvent` 不再调用。
3. `actHideAnimation()` 与 `actHideAnimation(func)` 改为即时关闭：
   ```
   void actHideAnimation() { actHideAnimation([] { accept(); }); }
   void actHideAnimation(std::function<void()> func) {
     if (widget_) widget_->hide();
     emit hideAnimationFinished();
     if (func) func();
   }
   ```
   （`hideAnimationFinished` 保留发射，API 稳定；4 个调用方无需改动。）
4. 保留：无边框、半透明、遮罩 paint、卡片阴影、Linux re-parent 逻辑。
5. 不引入淡入（最简）。
6. **改名（用户确认）**：类名 + 文件名 `AnimationDialog` → `OverlayDialog`
   （职责为「无边框遮罩卡片覆盖层」，去掉动画后旧名误导）。影响面：
   类名 + 文件（git mv）+ 5 个子类引用（BaseWizardDialog/Login/About/
   UserManager/StepEdit）+ MainWindow include + CMakeLists + 设计文档引用。
   与精简同一变更完成。
7. **Y1 清理**：删除不再使用的 `cachedPixmap_`/`snapshotOffset_` 成员、
   `#include <QPixmap>`（随成员移除）与仅动画使用的
   `<QPropertyAnimation>`/`<QRandomGenerator>`/`<QVariantAnimation>`。

## 详细设计

### 文件与类

| 文件 | 说明 |
|------|------|
| `git mv src/app/dialogs/AnimationDialog.h/.cpp → OverlayDialog.h/.cpp` + 类改名 | 重命名 |
| 修改 `src/app/dialogs/OverlayDialog.cpp` | showEvent 直接居中、删 actShowAnimation、actHideAnimation 即时化、清死代码与冗余 include |
| 修改 `src/app/dialogs/OverlayDialog.h` | 删 actShowAnimation 声明 + 死成员；类名 OverlayDialog |
| 修改 5 个子类头文件 + MainWindow include | AnimationDialog → OverlayDialog |
| 修改 `src/app/CMakeLists.txt` | 条目改名 |
| 修改设计文档 | 引用更新 |

### 行为影响

- 所有 OverlayDialog 子类（Login/About/UserManager/StepEdit/BaseWizard→向导）
  **无需改动**：显示即时居中、关闭即时，遮罩/无边框/阴影外观不变。
- Linux 下仍无原生标题栏/边框（统一外观的核心诉求不受影响）。
- 向导页切换动画（PageTransitionOverlay）独立，不受影响。

## 验证

1. `scripts/build_ninja.bat -t debug -m ETestStudio` 编译通过。
2. 手动（GUI 需人工确认）：
   - 登录/关于/用户管理/步骤编辑/向导打开：卡片即时居中显示，无飞入动画
   - 遮罩正常（主窗口变灰、卡片聚焦），关闭即时
   - Windows 与 Linux（WSL）外观一致，Linux 无原生标题栏/边框
   - 向导页切换（BaseWizardDialog 的 PageTransitionOverlay）不受影响
