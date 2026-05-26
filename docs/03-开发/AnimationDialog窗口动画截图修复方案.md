# AnimationDialog 窗口动画修复方案

## 问题

`AnimationDialog`（全屏浮层对话框）在 Windows 上使用 `WA_TranslucentBackground`，Qt 会创建 `WS_EX_LAYERED` 分层窗口，通过 `UpdateLayeredWindowIndirect` API 渲染。动画/拖拽过程中，Qt 计算的脏矩形（dirty region）出现负坐标，导致 API 调用失败：

```
UpdateLayeredWindowIndirect failed for ptDst=(...), size=(...), dirty=(..., -9) (参数错误)
```

## 根因

- **飞入/飞出动画**：`actShowAnimation()`/`actHideAnimation()` 用 `QPropertyAnimation` 移动内部 widget 的 pos，动画启止位置在窗口外（如 `p1 = QPoint(-w, ...)`）。分层窗口在 child widget 移出边界时产生无效脏矩形。
- **拖拽**：`WindowMover` 的 `MouseMove` 将 widget 移到窗口边缘外时，同样产生负坐标脏矩形。

## 方案

用截图 + paintEvent 重绘替代真实 widget 移动，动画驱动力在 paintEvent 中的绘制偏移坐标，不创建/移动任何 child widget，彻底绕过分层窗口的脏矩形问题。同时拖拽时 clamp 坐标到父窗口范围内。

### 涉及文件

| 文件 | 改动类型 |
|------|----------|
| `src/app/dialogs/AnimationDialog.h` | 修改 |
| `src/app/dialogs/AnimationDialog.cpp` | 修改 |
| `src/app/utils/window_mover.cpp` | 修改 |

### 详细改动

#### 1. AnimationDialog.h

新增引入：

```cpp
#include <QPixmap>
class QGraphicsDropShadowEffect;
```

新增成员：

```cpp
  QPixmap cachedPixmap_;
  QPoint snapshotOffset_;
  QGraphicsDropShadowEffect* shadowEffect_ = nullptr;
```

#### 2. AnimationDialog.cpp

**`setWidget()` 修改：**

把阴影效果的指针存下来，方便截图时关/恢复：

```cpp
void AnimationDialog::setWidget(QWidget* widget) {
  // ... 现有逻辑 ...
  shadowEffect_ = new QGraphicsDropShadowEffect(widget_);
  shadowEffect_->setColor(QColor(105, 105, 105, 200));
  shadowEffect_->setBlurRadius(9);
  shadowEffect_->setOffset(0, 0);
  widget_->setGraphicsEffect(shadowEffect_);
}
```

**`actShowAnimation()` 重写：**

```
1. 关阴影：if (shadowEffect_) shadowEffect_->setEnabled(false);
2. cachedPixmap_ = widget_->grab()
3. 恢复阴影：if (shadowEffect_) shadowEffect_->setEnabled(true);
4. widget_->hide()
5. 计算居中位置 center 和随机飞入起始位置 p1（算法保持不变）
6. QVariantAnimation 驱动 snapshotOffset_
   - 类型 QVariantAnimation，setEasingCurve(OutQuint)，500ms
   - startValue = QPoint(p1 - center)    // 飞入起点相对于居中的偏移
   - endValue = QPoint(0, 0)             // 居中
   - 值变化时：snapshotOffset_ = value.toPoint(); update();
   - finished：widget_->move(center); widget_->show(); cachedPixmap_ = {};
7. 动画开始
```

**`actHideAnimation(std::function<void()> func)` 重写：**

```
1. 关阴影：if (shadowEffect_) shadowEffect_->setEnabled(false);
2. cachedPixmap_ = widget_->grab()
3. 恢复阴影：if (shadowEffect_) shadowEffect_->setEnabled(true);
4. widget_->hide()
5. 计算居中位置 center 和随机飞出目标位置 p2（算法保持不变）
6. QVariantAnimation 驱动 snapshotOffset_
   - startValue = QPoint(0, 0)
   - endValue = QPoint(p2 - center)      // 飞入终点相对于居中的偏移
   - 值变化时：snapshotOffset_ = value.toPoint(); update();
   - finished：cachedPixmap_ = {}; emit hideAnimationFinished(); func();
7. 动画开始
```

**`paintEvent()` 修改：**

先画遮罩（已有逻辑），再在截图存在时绘制到居中 + 偏移位置：

```cpp
void AnimationDialog::paintEvent(QPaintEvent* event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QPainterPath path;
  path.addRoundedRect(rect(), round_radius_, round_radius_);
  p.fillPath(path, QColor(205, 205, 205, 170));

  if (!cachedPixmap_.isNull()) {
    int x = (width() - cachedPixmap_.width()) / 2 + snapshotOffset_.x();
    int y = (height() - cachedPixmap_.height()) / 2 + snapshotOffset_.y();
    p.drawPixmap(x, y, cachedPixmap_);
  }
}
```

#### 3. window_mover.cpp

在 `MouseMove` 事件分支，`m_target->move(...)` 前 clamp 坐标：

```cpp
QPoint newPos = mouseEvent->globalPos() - m_dragPosition;
if (auto* parent = m_target->parentWidget()) {
  int maxX = parent->width() - m_target->width();
  int maxY = parent->height() - m_target->height();
  newPos.setX(qBound(0, newPos.x(), maxX));
  newPos.setY(qBound(0, newPos.y(), maxY));
}
m_target->move(newPos);
```

### 影响范围

- **LoginDialog**（AnimationDialog 子类）：登录卡片飞入/飞出动画、拖拽 → 修复
- **AboutDialog**（AnimationDialog 子类）：About 卡片飞入/飞出动画 → 修复（AboutDialog 已 `removeWindowMover()`，无拖拽）
- **UserManagerDialog**（AnimationDialog 子类）：用户管理卡片飞入/飞出 → 修复

### 不需要改动的文件

- `LoginDialog`、`AboutDialog`、`UserManagerDialog` — 它们只是调用父类 `actHideAnimation()`，无需改动
- `resource.qrc`、`QSS` 等 — 无样式变化

### 设计决策

| 决策 | 结论 |
|------|------|
| 截图方式 | 先关阴影 → grab → 恢复阴影，确保干净截图 |
| 阴影处理 | `QGraphicsDropShadowEffect::setEnabled(false)` 临时关闭 |
| 动画帧率 | 默认 60fps，无需限帧 |
| 动画流程 | grab 完立刻 hide widget |
| widget 最终位置 | 固定居中 |
| 防重入 | 不需要（LoginDialog 一次性使用） |

### 验证方法

1. `scripts/build_ninja.bat` 编译成功
2. `scripts/run_app.bat` 启动
3. 点击登录按钮 → 浮层出现，飞入动画无 error log
4. 拖拽登录卡片在遮罩范围内移动 → 无 error log，不能拖到遮罩外
5. 登录成功 → 浮层飞出关闭，无 error log
6. 关于对话框打开/关闭 → 飞入飞出无 error log
