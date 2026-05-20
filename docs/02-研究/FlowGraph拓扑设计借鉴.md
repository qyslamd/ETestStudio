# FlowGraph 拓扑编辑功能设计借鉴

> 分析 [FlowGraph](D:\workspace\self\works\workspace\flow-graph\FlowGraph) 项目中可借鉴到 `src/topology/` 模块的 7 个设计点。

---

## 1. NicePathMaker 智能路径路由

### 现状

拓扑模块的 `ConnectionItem::updatePath()` 仅有一种贝塞尔曲线实现，不会避障，连线可能穿过其他方块。

### 借鉴设计

FlowGraph 的 `NicePathMaker` 支持 3 种连线风格，并根据源/目标相对位置自动选择绕行路径。

```cpp
// FlowGraph 中 NicePathMaker 的核心接口
class NicePathMaker {
public:
    // 三种连线风格
    enum LineStyle { Line, Polyline, Curve };

    QPainterPath create(const LinkItem* startItem, const LinkItem* endItem);

    // 静态工具方法
    static QPainterPath makeLine(const QPointF& start, const QPointF& end);
    static QPainterPath makeCurve(const QPointF& start, const QPointF& end);

private:
    // 绕行逻辑：根据源/目标矩形位置选择上绕还是下绕
    qreal calcFisrtTurningLen(const LinkItem* start, const LinkItem* end);
    void calcPathA2B(QPainterPath& path, ..., const QRectF& srcRect, const QRectF& destRect);
    void calcPathAndB(QPainterPath& path, ..., const QRectF& srcRect, const QRectF& destRect);
};
```

### 关键算法：避障折线

当目标在源左侧时，FlowGraph 会计算绕行路径绕过两个块：

```cpp
// 源在右，目标在左 —— 需要绕行
if (x1 > x2) {
    if (doubleEquals(y1, y2)) {
        // 水平对齐时从上方绕行
        path.moveTo(endPos);
        path.lineTo(x2 - Turning, y2);
        path.lineTo(x2 - Turning, min(top1, top2) - Turning);  // 绕到上方
        path.lineTo(x1 + Turning, min(top1, top2) - Turning);
        path.lineTo(x1 + Turning, y1);
        path.lineTo(startPos);
    } else {
        // 错位时判断往哪个方向绕更近
        if (abs(_ytop - y1) < abs(_ybottom - y1))
            // 往上绕
        else
            // 往下绕
    }
}
```

### 引入建议

在拓扑模块中新增 `TopologyPathRouter` 类，将当前 `ConnectionItem::updatePath()` 中的路径计算逻辑抽取出来，并增加：

1. **路径风格枚举**：`Line | Polyline | Curve`
2. **避障参数**：从 `TopologyDocument` 获取所有 UUT/Device 的包围盒
3. **箭头绘制**：保留现有方向箭头，并入路由类

```cpp
// 建议的新接口
class TopologyPathRouter {
public:
    enum Style { Bezier, Polyline, Straight };
    QPainterPath route(const QPointF& src, const QPointF& dst,
                       const QVector<QRectF>& obstacles,
                       Style style = Bezier);
};
```

---

## 2. IRunnableItem 可扩展 Item 接口

### 现状

`UutItem` 和 `DeviceItem` 各自独立实现，hover/select/shape 等逻辑重复。

```cpp
// 当前：两个类各自实现相同的接口
class UutItem : public QGraphicsItem { /* 独立实现 hover、select、paint */ };
class DeviceItem : public QGraphicsItem { /* 重复实现 hover、select、paint */ };
```

### 借鉴设计

FlowGraph 通过 `IRunnableItem` 基类统一管理公共行为：

```cpp
// FlowGraph 的设计
class IRunnableItem : public QGraphicsObject, public QRunnable {
    Q_PROPERTY(QSizeF size READ size WRITE setSize NOTIFY sizeChanged)
public:
    explicit IRunnableItem(const QSizeF& size, int inputCount, int outputCount);

    // 模板方法：子类只需实现 paintUnder()
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) final;
    virtual void paintUnder(QPainter*, const QStyleOptionGraphicsItem*) = 0;

    // 生命周期钩子
    virtual void doAfterItemAddToScene() {}
    virtual void doAfterSizeChanged() {}

    // 通用功能
    void setInputCount(int n);
    void setOutputCount(int n);
    void setSize(const QSizeF& size);

    // 通用交互
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void mousePressEvent(QGraphicsSceneMouseEvent*) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;

    // resize 手柄
    void drawResizeSign(QPainter*, const QStyleOptionGraphicsItem*, QWidget*);
};
```

### 引入建议

新增 `TopologyBlockItem` 基类，将 UutItem 和 DeviceItem 的共同逻辑上提：

```cpp
// 建议的新基类
class TopologyBlockItem : public QGraphicsItem {
public:
    TopologyBlockItem(int index, TopologyDocument* doc);

    // 子类只需实现这两个方法
    virtual void paintContent(QPainter* painter, const QRectF& rect) = 0;
    virtual qreal contentHeight() const = 0;

    // 基类统一实现
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) final;
    QPainterPath shape() const final;
    bool contains(const QPointF& point) const final;
    QVariant itemChange(GraphicsItemChange, const QVariant&) final;

    // 通用交互
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) final;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) final;

protected:
    int index_;
    TopologyDocument* doc_;
    bool hovered_ = false;

    // 默认常量
    static constexpr qreal kCornerRadius = 8.0;
    static constexpr qreal kShadowOffset = 4.0;
};
```

这样 UutItem 和 DeviceItem 只需实现 `paintContent()` 和 `contentHeight()`，缩减约 40% 的重复代码。

---

## 3. 端口连接约束增强

### 现状

当前 `TopologyDocument::canConnect()` 检查较宽松，缺少用户操作层面的约束。

```cpp
// 当前实现（TopologyDocument.cpp）
bool TopologyDocument::canConnect(...) const {
    // 主要检查类型匹配，缺少拓扑规则检查
}
```

### 借鉴设计

FlowGraph 在 `LinkItem::mouseReleaseEvent()` 中有三层约束检查：

```cpp
// FlowGraph 的连接约束
void LinkItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    // 1. 自连接阻止
    if (link_item->parentItem() == this->parentItem()) {
        ok = false;
    }

    // 2. 同类型阻止（Input 不能连 Input）
    if (link_item->io_type() == this->io_type()) {
        ok = false;
    }
}

// 3. 输入端单连线（在 Scene 层面检查）
void FlowGraphScene::onLinkPathEndLink(bool ok, LinkItem* item) {
    if (targetItem->data(0).value<LinkPath*>()) {
        ok = false;  // Input 端口已有连线，拒绝
    }
}
```

### 引入建议

在 `TopologyEditorWidget::onSelectionChanged` 或 `TopologyScene::finishConnectionDrag` 中增加约束：

```cpp
// 建议在 TopologyScene::finishConnectionDrag 中增加的约束
void TopologyScene::finishConnectionDrag(QPointF scenePos) {
    // ... 现有逻辑 ...

    if (srcPort && devPort) {
        // 约束 1：不允许自连接（同一产品的端口不能连自己的设备）
        // —— 当前拓扑的 product 和 device 是分离的，此项天然满足

        // 约束 2：不允许 Input 连 Input（或 Output 连 Output）
        const auto& srcDir = prod->ports[srcPort->portIndex()].direction;
        const auto& dstDir = dev->ports[devPort->portIndex()].direction;
        if (srcDir == TopologyPort::Input && dstDir == TopologyPort::Input)
            return;
        if (srcDir == TopologyPort::Output && dstDir == TopologyPort::Output)
            return;

        // 约束 3：输入端单连线
        if (hasExistingConnection(dev, devPort->portIndex()))
            return;

        // 约束 4：功能类型匹配（A429 只能连 A429）
        if (srcPortType != dstPortType)
            return;

        // 通过所有检查后才创建连线
        doc_->undoStack()->push(new AddConnectionCommand(doc_, conn));
    }
}
```

---

## 4. 端口可视化风格多样化

### 现状

端口只有圆形 + 短线的单一风格，用颜色区分方向。

### 借鉴设计

FlowGraph 的 `LinkItem` 支持 4 种绘制风格，其中 `Triangle` 风格通过形状直观表达端口方向：

```cpp
// FlowGraph 的三角形端口绘制
case Triangle: {
    auto rect = option->rect;
    if (io_type() == IO_Type::Output) {
        // 输出端口：三角形顶点朝右
        QPainterPath path(rect.topLeft());
        path.lineTo(rect.right(), rect.center().y());   // 尖朝右
        path.lineTo(rect.bottomLeft());
        painter->fillPath(path, pen_.brush());
    } else {
        // 输入端口：三角形顶点朝左
        QPainterPath path(rect.topRight());
        path.lineTo(rect.bottomRight());
        path.lineTo(rect.left(), rect.center().y());     // 尖朝左
        painter->fillPath(path, pen_.brush());
    }
}
```

### 引入建议

在 `PortItem::paint()` 和 `DevicePortItem::paint()` 中增加可选的三角形绘制模式：

```cpp
// 建议在 PortItem::paint() 中增加
enum PortStyle { Circle, Triangle };

void PortItem::paint(QPainter* painter, ...) {
    if (portStyle_ == Triangle) {
        // 方向朝外
        QPainterPath path;
        if (port.direction == TopologyPort::Input) {
            // 输入：箭头指向块内部
            path.moveTo(0, -kRadius);
            path.lineTo(-kLineLength, 0);
            path.lineTo(0, kRadius);
        } else {
            // 输出：箭头指向块外部
            path.moveTo(0, -kRadius);
            path.lineTo(kLineLength, 0);
            path.lineTo(0, kRadius);
        }
        painter->fillPath(path, color);
    } else {
        // 现有圆形绘制逻辑
    }
}
```

也可以考虑在 `TopologyEditorWidget` 工具栏增加风格切换按钮。

---

## 5. 拖放创建 Item

### 现状

拓扑模块仅通过工具栏按钮点击来添加 UUT/Device，不支持从面板拖入。

### 借鉴设计

FlowGraph 通过 `QGraphicsScene` 的 drag/drop 事件处理拖放：

```cpp
// FlowGraph Scene 的拖放支持
void FlowGraphScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event) {
    auto mimeData = event->mimeData();
    if (mimeData->hasFormat("application/fg-dragdata")) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsScene::dragEnterEvent(event);
}

void FlowGraphScene::dropEvent(QGraphicsSceneDragDropEvent* event) {
    QByteArray itemData = event->mimeData()->data("application/fg-dragdata");
    QDataStream dataStream(&itemData, QIODevice::ReadOnly);

    int id; dataStream >> id;
    QString name; dataStream >> name;

    // 根据 id 创建对应类型的 Item
    addRunnableItem(new EndItem, event->scenePos());
    event->acceptProposedAction();
}
```

外部工具箱通过设置 MIME 数据发起拖放：

```cpp
// 在 ControlToolbox 中发起拖放
void ControlToolbox::startDrag() {
    QMimeData* mimeData = new QMimeData;
    QByteArray itemData;
    QDataStream dataStream(&itemData, QIODevice::WriteOnly);
    dataStream << controlInfo.id << controlInfo.name;
    mimeData->setData("application/fg-dragdata", itemData);

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction);
}
```

### 引入建议

在 `TopologyScene` 中增加 drop 事件处理，配合左侧面板的拖放：

```cpp
// 建议在 TopologyScene 中增加
const char kTopologyMimeType[] = "application/x-topology-device";

void TopologyScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event) {
    if (event->mimeData()->hasFormat(QLatin1String(kTopologyMimeType))) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsScene::dragEnterEvent(event);
}

void TopologyScene::dropEvent(QGraphicsSceneDragDropEvent* event) {
    if (event->mimeData()->hasFormat(QLatin1String(kTopologyMimeType))) {
        QByteArray data = event->mimeData()->data(QLatin1String(kTopologyMimeType));
        QString deviceType = QString::fromUtf8(data);
        emit deviceDropped(deviceType, event->scenePos());
        event->acceptProposedAction();
        return;
    }
    QGraphicsScene::dropEvent(event);
}
```

然后 `TopologyEditorWidget` 连接 `deviceDropped` 信号来创建设备。

---

## 6. 连线风格右键切换

### 现状

连线右键菜单只有"删除连线"，不能改变连线的呈现风格。

### 借鉴设计

FlowGraph 的 LinkPath 右键菜单支持运行时切换 3 种连线风格：

```cpp
// FlowGraph 的连线风格切换实现
void LinkPath::initMenu() {
    context_menu_.addAction("删除连线", this, &LinkPath::onDeleteAction);

    auto action = context_menu_.addAction("连线类型");
    action->setMenu(&sub_menu_line_style_);

    auto ac1 = sub_menu_line_style_.addAction("直线", [this] { setLineStyle(Line); });
    auto ac2 = sub_menu_line_style_.addAction("折线", [this] { setLineStyle(Polyline); });
    auto ac3 = sub_menu_line_style_.addAction("曲线", [this] { setLineStyle(Curve); });

    QActionGroup* group = new QActionGroup(&context_menu_);
    group->setExclusive(true);
    group->addAction(ac1);
    group->addAction(ac2);
    group->addAction(ac3);
    ac3->setChecked(true);  // 默认曲线
}

void LinkPath::setLineStyle(LineStyle style) {
    style_ = style;
    auto sourceItem = data(SrcLnkItemRole).value<LinkItem*>();
    auto targetItem = data(DstLnkItemRole).value<LinkItem*>();
    setPath(NicePathMaker(style_).create(sourceItem, targetItem));
    update();
}
```

### 引入建议

在 `ConnectionItem` 中增加风格属性和右键菜单：

```cpp
// 建议在 ConnectionItem 中增加
class ConnectionItem : public QGraphicsPathItem {
public:
    enum Style { Bezier, Polyline, Straight };

    void setStyle(Style s) {
        style_ = s;
        updatePath();
        update();
    }

    Style style() const { return style_; }

private:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override {
        QMenu menu;
        menu.addAction("删除连线", ...);

        auto* styleMenu = menu.addMenu("连线样式");
        auto* bezierAct = styleMenu->addAction("曲线", [this] { setStyle(Bezier); });
        auto* polyAct  = styleMenu->addAction("折线", [this] { setStyle(Polyline); });
        auto* lineAct  = styleMenu->addAction("直线", [this] { setStyle(Straight); });

        // 当前风格置为选中状态
        QActionGroup group(styleMenu);
        group.setExclusive(true);
        // ...
        menu.exec(event->screenPos());
    }

    Style style_ = Bezier;
};
```

此外，还可以根据功能类型自动选择默认风格（例如 A429 = 曲线，DISCRETE = 折线）。

---

## 7. Item Resize 手柄

### 现状

`UutItem` 和 `DeviceItem` 的大小根据端口数量自动计算，不支持用户调整。

### 借鉴设计

FlowGraph 的 `IRunnableItem` 实现了 8 个方向的 resize 手柄：

```cpp
// FlowGraph 的 resize 设计
class IRunnableItem : public QGraphicsObject {
    enum class ResizePos {
        None, TopLeft, TopMid, TopRight,
        RightMid, BottomRight, BottomMid, BottomLeft, LeftMid
    } resize_mode_;

    QPointF resize_start_pos_;

    void checkResizeMode(const QPointF& pos) {
        // 检查鼠标是否落在某个手柄区域
        for (int i = TopLeft; i <= LeftMid; ++i) {
            auto pos_enum = static_cast<ResizePos>(i);
            if (calcResizeSignRect(pos_enum).contains(pos)) {
                resize_mode_ = pos_enum;
                return;
            }
        }
        resize_mode_ = ResizePos::None;
    }

    void changeCursorWhenMouseHover(const QPointF& pos) {
        // 根据手柄位置改变光标形状
        switch (resize_mode_) {
        case TopLeft: case BottomRight:
            setCursor(Qt::SizeFDiagCursor); break;
        case TopMid: case BottomMid:
            setCursor(Qt::SizeVerCursor); break;
        // ...
        }
    }

    QRectF calcResizeSignRect(ResizePos position) const {
        // 计算 8 个手柄的小方块区域
        const qreal size = 6.0;
        switch (position) {
        case TopLeft:      return QRectF(0, 0, size, size);
        case TopRight:     return QRectF(size_.width() - size, 0, size, size);
        case BottomRight:  return QRectF(size_.width() - size, size_.height() - size, size, size);
        case BottomLeft:   return QRectF(0, size_.height() - size, size, size);
        case TopMid:       return QRectF(size_.width() / 2 - size / 2, 0, size, size);
        // ...
        }
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
        if (resize_mode_ != ResizePos::None) {
            // 根据拖拽方向计算新的大小
            QPointF delta = event->pos() - resize_start_pos_;
            QSizeF newSize = size_;
            switch (resize_mode_) {
            case RightMid: newSize.setWidth(size_.width() + delta.x()); break;
            case BottomMid: newSize.setHeight(size_.height() + delta.y()); break;
            // ...
            }
            setSize(newSize);
            resize_start_pos_ = event->pos();
        }
    }
};
```

### 引入建议

当前拓扑模块的块大小是固定的，如果未来需要支持用户自定义块大小，可以在 `TopologyBlockItem`（建议的基类）中增加：

```cpp
// 建议在 TopologyBlockItem 中增加
class TopologyBlockItem : public QGraphicsItem {
    void setUserSize(const QSizeF& size) {
        user_size_ = size;
        prepareGeometryChange();
        update();
    }

protected:
    // 如果用户设置了自定义大小，使用用户大小；否则自动计算
    qreal effectiveContentHeight() const {
        return user_size_.isValid() ? user_size_.height() : contentHeight();
    }

    QSizeF user_size_;
    enum ResizeMode { None, Right, Bottom, Corner } resize_mode_;
};
```

**注意**：此项优先级较低，在块内容动态变化（端口增删）时会增加复杂度。建议仅在需要用户自定义布局时引入。

---

## 总结优先级

| # | 功能 | 复杂度 | 效果 | 建议阶段 |
|---|------|--------|------|----------|
| 1 | NicePathMaker 智能路由 | 中 | 连线质量大幅提升 | 阶段 1 |
| 2 | TopologyBlockItem 公共基类 | 中 | 消除重复代码，便于维护 | 阶段 1 |
| 3 | 端口连接约束增强 | 低 | 防止误操作 | 阶段 1 |
| 4 | 端口风格多样化 | 低 | 视觉提升 | 阶段 2 |
| 5 | 拖放创建 Item | 中 | 交互体验提升 | 阶段 2 |
| 6 | 连线风格右键切换 | 低 | 用户体验增强 | 阶段 2 |
| 7 | Resize 手柄 | 高 | 灵活性提升，但复杂度高 | 阶段 3 |

---

*参考项目*：`D:\workspace\self\works\workspace\flow-graph\FlowGraph`
*目标项目*：`src/topology/`
*编制日期*：2026-05-20
