# DeepSeek 引入的性能问题

## BUG — TopologyView 图例现在完全不渲染

`TopologyView.cpp:39-48` — `paintEvent` 检查 `!legend_cache_.isNull()`，但 `renderLegendCache()` **从未被调用**。构造函数没调用，`resizeEvent` 只是清空缓存（`legend_cache_ = {}`），没有任何地方重建它。**图例现在完全不可见。** 这是 DeepSeek 改动引入的回归。

## P0 — 两个全局事件过滤器同时挂在 qApp 上

1. **EyeWidget.cpp:13** — `qApp->installEventFilter(this)` — 应用中的**每一个事件**都会触发，用于追踪鼠标位置做眼球动画
2. **main_window.cpp:871** — `qApp->installEventFilter(this)` — 应用中的**每一个事件**都会触发，用于 Tux 屏保的空闲检测

每一次鼠标移动、按键、滚轮、绘制等事件都会经过**两个**过滤器。EyeWidget 过滤器在每个鼠标事件上做 `qobject_cast` 检查和距离计算，即使 Welcome 标签页不可见也会运行。

## P0 — EditorManager focusChanged 的 parent 链遍历

**EditorManager.cpp:44-57** — `QApplication::focusChanged` 在每次焦点变化时触发。处理器沿 parent 链逐级 `qobject_cast<ads::CDockWidget*>` 查找（O(depth)）。一次 tab 切换会触发多次焦点变化（ADS 内部 widget、工具栏按钮等），所以这段代码会运行多次。

此外，`focusedDockWidgetChanged` **也会**在同一个激活操作中触发，导致 `onDockWidgetActivated` 在每次 tab 切换时被调用**两次**（虽然第二次因 `editorId == current_file_path_` 而提前返回）。

## P1 — PaintedClockWidget 无条件调用 update()

**PaintedClockWidget.cpp:16** — `timer->start(1000)` 配合 `[this]() { update(); }`。每秒无条件调用 `update()`，即使 widget 隐藏或在不可见的标签页上。Qt 会抑制实际绘制，但 `update()` 仍然会调度一个重绘事件。

## P1 — EyeWidget 16ms 定时器永远运行

**EyeWidget.cpp:20** — `anim_timer_->start(16)` 以 60fps 永远运行。`tick()` 方法每 16ms 做一次浮点运算，即使 Welcome 标签页不可见。条件化的 `update()` 有帮助，但定时器回调本身仍然在运行。

## P2 — WelcomeWidget::showEvent 多余的 update()

**WelcomeWidget.cpp:417** — `showEvent()` 中调用 `update()`，但 Qt 在 widget 变为可见时已经调度了重绘。无害但多余。

## P2 — GridTile::showEvent 每次都调用 calcFixedSize()

**GridTile.cpp:156-159** — 每个 GridTile 在每次 show 时都重新计算固定大小。Welcome 页面上约 7 个磁贴，每次切换到 Welcome 时会运行 7 次。

## 汇总

| 严重度 | 问题 | 影响 |
|--------|------|------|
| **BUG** | TopologyView 图例不渲染 | 视觉回归 — 图例不可见 |
| **P0** | 两个全局事件过滤器挂在 qApp | 每个事件被不必要地处理两次 |
| **P0** | focusChanged parent 链遍历 | 每次焦点变化 O(depth) `qobject_cast`，每次 tab 切换运行多次 |
| **P1** | 时钟无条件 update() | 即使隐藏时定时器仍触发 |
| **P1** | EyeWidget 16ms 定时器永远运行 | 即使隐藏时也有 60fps CPU 开销 |
| **P2** | showEvent 多余 update() | 轻微开销 |
| **P2** | GridTile calcFixedSize 在 show 时调用 | 每个磁贴轻微开销 |
