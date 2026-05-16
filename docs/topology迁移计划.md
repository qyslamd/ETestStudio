# topology-demo 迁移计划

## 目的

将 `examples/topology-demo/` 的源代码作为 `etest` 的一部分迁移到 `src/app/topology/`，并在侧边栏添加固定按钮，点击后在中央区域以停靠窗口形式打开拓扑编辑器。

---

## 阶段一：文件迁移

**1.1** 创建 `src/app/topology/` 目录，移动以下 8 组源文件（共 16 个文件）：
- `TopologyDocument.h/.cpp`
- `TopologyJsonSerializer.h/.cpp`
- `topology_items.h/.cpp`
- `TopologyScene.h/.cpp`
- `TopologyView.h/.cpp`
- `TopologyEditorWidget.h/.cpp`
- `PropertyPanelWidget.h/.cpp`
- `DeviceTemplateManager.h/.cpp`

**1.2** 新建 `src/app/topology/CMakeLists.txt`，编译为静态库 `etest_topology`：
```cmake
set(TARGET_NAME etest_topology)
add_library(${TARGET_NAME} STATIC
    TopologyDocument.h TopologyDocument.cpp
    TopologyJsonSerializer.h TopologyJsonSerializer.cpp
    topology_items.h topology_items.cpp
    TopologyScene.h TopologyScene.cpp
    TopologyView.h TopologyView.cpp
    TopologyEditorWidget.h TopologyEditorWidget.cpp
    PropertyPanelWidget.h PropertyPanelWidget.cpp
    DeviceTemplateManager.h DeviceTemplateManager.cpp
)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(${TARGET_NAME} PROPERTIES AUTOMOC ON)
target_link_libraries(${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui)
```

**1.3** `src/app/CMakeLists.txt`：
- 添加 `add_subdirectory(topology)`
- `target_link_libraries` 添加 `etest_topology`

**1.4** 根 `CMakeLists.txt`：移除 `add_subdirectory(examples/topology-demo)`

---

## 阶段二：TopologyEditorWidget 调整

**2.1** `initUi()` 中移除：
- `setWindowTitle(QStringLiteral("拓扑编辑器 - topology-demo"));`
- `resize(1200, 800);`

**2.2** 添加自定义信号，用于在文件操作时更新 dock tab 标题：

`TopologyEditorWidget.h` 添加：
```cpp
Q_SIGNALS:
void editorTitleChanged(const QString& title);
```

`TopologyEditorWidget.cpp` 中替换：
- `onNewFile()` 中 `setWindowTitle(...)` → `emit editorTitleChanged(...)`
- `onOpenFile()` 中 `setWindowTitle(...)` → `emit editorTitleChanged(...)`
- `onSaveAsFile()` 中 `setWindowTitle(...)` → `emit editorTitleChanged(...)`

---

## 阶段三：SidebarWidget 增加固定按钮

**3.1** `SidebarWidget.h`：
- 添加信号 `void openTopologyEditorRequested();`
- 添加成员 `QPushButton* topology_btn_;`

**3.2** `SidebarWidget.cpp` `setupUi()`：
- 在 `layout->addWidget(stack_)` 之后添加底部固定按钮：

```cpp
auto* btnLayout = new QHBoxLayout();
btnLayout->setContentsMargins(4, 4, 4, 4);
topology_btn_ = new QPushButton(QStringLiteral("  硬件拓扑"), this);
topology_btn_->setFixedHeight(36);
topology_btn_->setFlat(true);
topology_btn_->setFocusPolicy(Qt::NoFocus);
topology_btn_->setIcon(QIcon(":/resources/icons/svg/hardware_dark.svg"));
topology_btn_->setIconSize(QSize(20, 20));
topology_btn_->setStyleSheet(QStringLiteral(
    "QPushButton { text-align: left; padding-left: 8px; color: #CCCCCC; background: transparent; }"
    "QPushButton:hover { background: #2a2d2e; }"));
btnLayout->addWidget(topology_btn_);
layout->addLayout(btnLayout);
```

**3.3** 构造函数中连接：
```cpp
connect(topology_btn_, &QPushButton::clicked,
        this, &SidebarWidget::openTopologyEditorRequested);
```

---

## 阶段四：MainWindow 集成

**4.1** `MainWindow.h`：
- 前向声明 `namespace etest::topology { class TopologyEditorWidget; }`
- 添加成员 `ads::CDockWidget* topology_dock_ = nullptr;`

**4.2** `MainWindow.cpp` `initSignals()` 添加连接：

```cpp
connect(sidebar_, &SidebarWidget::openTopologyEditorRequested, this, [this]() {
    if (!topology_dock_) {
        auto* editor = new etest::topology::TopologyEditorWidget();
        topology_dock_ = new ads::CDockWidget(QStringLiteral("硬件拓扑"));
        topology_dock_->setWidget(editor);
        topology_dock_->setFeature(ads::CDockWidget::DockWidgetClosable, true);
        topology_dock_->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
        topology_dock_->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, true);
        dock_manager_->addDockWidget(ads::CenterDockWidgetArea, topology_dock_);

        connect(editor, &etest::topology::TopologyEditorWidget::editorTitleChanged,
                this, [this](const QString& title) {
            if (topology_dock_)
                topology_dock_->setWindowTitle(title);
        });

        connect(topology_dock_, &QObject::destroyed, this, [this]() {
            topology_dock_ = nullptr;
        });
    } else {
        topology_dock_->toggleView(true);
        topology_dock_->raise();
    }
});
```

---

## 涉及文件清单

| 操作 | 文件 |
|------|------|
| 新建目录 | `src/app/topology/` |
| 新建 | `src/app/topology/CMakeLists.txt` |
| 移动+修改 | `src/app/topology/TopologyEditorWidget.h` |
| 移动+修改 | `src/app/topology/TopologyEditorWidget.cpp` |
| 移动 | 其余 7 组共 14 个 `.h/.cpp` 文件 |
| 修改 | `src/app/SidebarWidget.h` |
| 修改 | `src/app/SidebarWidget.cpp` |
| 修改 | `src/app/MainWindow.h` |
| 修改 | `src/app/MainWindow.cpp` |
| 修改 | `src/app/CMakeLists.txt` |
| 修改 | `CMakeLists.txt`（根目录） |
| 删除 | `examples/topology-demo/` 下 16 个 `.h/.cpp` |
| 保留 | `examples/topology-demo/main.cpp`、`readme.md` |
