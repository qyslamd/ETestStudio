# 想法清单

## 待实现

- [ ] **全局 IconProvider + ThemeManager**
  统一图标加载和主题管理，带信号机制和缓存
  -> [设计文档](docs/01-规划/全局IconProvider和ThemeManager设计.md)
  -> [任务计划](task_plan.md) 阶段 1.6

- [ ] **评估集成 QtNodes（paceholder/nodeeditor）**
  用途：测试流程可视化编辑（阶段4-5）、ICD 信号映射可视化
  注意：不能替代现有拓扑编辑器（范式不同）
  -> https://github.com/paceholder/nodeeditor
  参考案例：BehaviorTree/Groot 就是用 QtNodes 搭建领域特定节点编辑器的实际例子
  -> https://github.com/BehaviorTree/Groot
  评估时机：做到阶段 4（用例管理层）时再决定是否集成

- [ ] **拓扑编辑器 — 连线增强**
  手动调整连线路径/控制点、连线样式（颜色/粗细）、从端口拖出创建连线

- [ ] **拓扑编辑器 — 连接验证**
  方向不匹配等合法性检查，连线时实时提示

- [ ] **拓扑编辑器 — 自动布局**
  一键整理拓扑图布局算法

- [ ] **拓扑编辑器 — 设备模板**
  保存/加载/管理设备模板（当前只有另存为，没有加载入口）

- [ ] **拓扑编辑器 — 网格吸附**
  元素拖动时吸附到网格对齐

- [ ] **帧协议编辑器改进思路（参考 Protocol Designer）**
  参考：https://github.com/filipskrabak/protocol-designer
  可以借鉴的点：
  - 位图换行算法：bitsPerRow × pixelsPerBit 控制行宽，字段超出自动换行，变长字段支持填充/截断
  - Hover 联动高亮：位图悬停时高亮同字段所有跨行分块，同时联动树节点
  - 右键快捷操作：位图上直接右键 Edit/AddBefore/AddAfter/Delete
  - 分组着色：字段按功能分组（header/payload/checksum），自动分配底色
  - 双视图模式：位图可视化 + 列表拖拽排序并存

- [x] **评估 Taskflow（已评估，不适用）**
  用途评估：测试执行引擎的并行调度
  结论：不适用。引擎是顺序执行 + 控制流 + 交互式调试模型，与 Taskflow 的 DAG 并行范式不匹配。
  保留价值：后期多个独立用例并行执行时可能参考，但 MVP 不需要。
  -> https://github.com/taskflow/taskflow

- [ ] **引入 Step Result 路由机制**
  参考 TestStand 的 Pass/Fail/Error 跳转，在 JSON 用例中增加 `onPass`/`onFail`/`onError` 字段
  -> [NI 分析报告](docs/02-研究/NI_VeriStand_TestStand_分析与对比.md) 7.1 节

- [ ] **引入 Process Model 概念**
  参考 TestStand 的流程模板，在用例管理层增加顺序/循环等执行模式模板
  -> [NI 分析报告](docs/02-研究/NI_VeriStand_TestStand_分析与对比.md) 7.3 节

- [ ] **引入 Callback 钩子机制**
  PreStep/PostStep/onSetup/onCleanup 生命周期管理
  -> [NI 分析报告](docs/02-研究/NI_VeriStand_TestStand_分析与对比.md) 7.3 节

- [ ] **CentralWidget → QStackedWidget 改造**
  主窗口 centralWidget 替换为 QStackedWidget，每页完全独立布局，Ribbon 由页面自行管理。
  - 方案：QStackedWidget 直接 setCentralWidget，每页拥有自定义 chrome
  - Page 0: 设计模式（当前布局 — activity_bar + sidebar + dock + bottom panel）
  - Page 1+: 调试运行、监控/报表、CEF 嵌入式浏览器等
  - Ribbon 策略：IRibbonPage 接口，页面切换时 setup/teardown Ribbon category
  - 切换开销：低频操作（秒级），懒加载 category 缓存可避免重复构造

- [ ] **谷歌20206新图标风格提示词记录**
  - 谷歌2026风格图标，极简几何图形，流畅的彩色渐变（蓝红黄绿），柔和投影，毛玻璃质感，圆角，干净白底，等轴测3D倾斜，现代扁平设计，高清矢量。图标无背景。
  - Google 2026 style icon, minimalist geometric shape, smooth colorful gradient (blue, red, yellow, green), soft shadow, frosted glass texture, rounded corners, clean white background, 3D isometric tilt, modern flat design, high quality, 8k, vector illustration.
  
## 已完成

- [x] **NI VeriStand + TestStand 拆解分析**
  完成 NI 两大产品的模块拆解和与 IATP 的逐层对比报告，验证了 IATP 架构决策（UUID 信号/ICD 独立/Lua MVP），识别了 5 个可借鉴的设计模式
  -> [分析报告](docs/02-研究/NI_VeriStand_TestStand_分析与对比.md)

- [x] **UI 布局重构**
  全 QADS → QSplitter 混合布局，活动栏/侧边栏/底部面板独立为普通 QWidget
  -> [设计文档](docs/01-规划/UI布局重构方案.md)
  (commit 3b526e1)

- [x] **SARibbon 主窗口改造**
  QMainWindow → SARibbonMainWindow，Ribbon 功能区替代传统菜单栏/工具栏
  (commit 7e15c75)

- [x] **core → etest_core 重命名**
  统一 CMake 目标命名规范，涉及 12 个 CMakeLists.txt
  (commit 466f6ce)

- [x] **topology-demo 浅色主题 + SVG 图标修复**
   添加 resource.qrc、AUTORCC、setDarkTheme(false)
   (commit 7e15c75)

## 跨平台坑点记录

### 函数局部静态单例 + QSqlDatabase 的析构顺序问题

**现象**：`WisdomDatabase::instance()` 使用函数局部 `static WisdomDatabase db`，程序退出时析构函数访问 `QSqlDatabase::contains()` 导致 SIGSEGV。
**仅在 GCC（Linux）上复现**，MSVC（Windows）上安全。

**原因**：
- 函数局部静态变量在首次调用时构造，在程序退出时以**构造的逆序**析构（§[basic.start.term]）
- QSqlDatabase 内部维护一个全局连接注册表（也是静态存储期），其析构时机由 Qt 内部决定
- GCC 和 MSVC 对**不同编译单元**（Qt 库 vs 用户代码）中静态变量的析构顺序有不同的实现策略
- GCC 倾向于先析构用户代码中的静态对象，此时 QSqlDatabase 的注册表可能已被析构 → `QSqlDatabase::contains()` 访问已销毁内存 → SIGSEGV
- MSVC 的析构顺序恰好相反，因此从未暴露

**教训**：**凡是持有 QSqlDatabase 连接的单例，不应依赖析构函数来清理连接。** 因为无法保证你的析构函数在 QSqlDatabase 内部静态数据销毁之前执行。

**修复方案**：改为堆分配单例，永不析构。这与 `QCoreApplication` 的典型模式一致——让 Qt 在 `QCoreApplication` 析构时自行清理 SQL 连接注册表。

```cpp
WisdomDatabase& WisdomDatabase::instance() {
  static WisdomDatabase* db = new WisdomDatabase();
  return *db;
}
```

**适用场景**：任何在全局/静态生命周期中使用 Qt 资源（`QSqlDatabase`、`QNetworkAccessManager`、`QThread`、`QTimer` 等）的单例或静态对象。

**一般性建议**：
1. 函数局部静态变量适用于**纯数据**或**不依赖 Qt 内部静态数据的对象**
2. 如果对象依赖 Qt 类（特别是涉及全局注册表的类如 `QSqlDatabase`），优先考虑堆分配（`new` + 永不 `delete`）
3. 或者将初始化/清理托管给 `QCoreApplication` 的生命周期（如 `aboutToQuit` 信号中手动清理）
4. 跨平台代码**必须**在 GCC 和 MSVC 下都测试退出路径——析构顺序是未定义行为，标准不保证一致性

## SARibbon的固定收起按钮
```txt

3rdparty\SARibbon-2.5.7\src\SARibbonBar\SARibbonBar.cpp


/**
 * @brief 显示隐藏ribbon的按钮
 * @param isShow
 */
void SARibbonBar::showMinimumModeButton(bool isShow)
{
    SA_D(d);
    if (isShow && !(d->mMinimumCategoryButtonAction)) {
        activeRightButtonGroup();

        d->mMinimumCategoryButtonAction = new QAction(this);
        d->mMinimumCategoryButtonAction->setIcon(style()->standardIcon(
            isMinimumMode() ? QStyle::SP_TitleBarUnshadeButton : QStyle::SP_TitleBarShadeButton, nullptr));
        connect(d->mMinimumCategoryButtonAction, &QAction::triggered, this, [ this, d ]() {
            this->setMinimumMode(!isMinimumMode());
            d->mMinimumCategoryButtonAction->setIcon(style()->standardIcon(
                isMinimumMode() ? QStyle::SP_TitleBarUnshadeButton : QStyle::SP_TitleBarShadeButton, nullptr));
        });
        if (d->mRightButtonGroup) {
            d->mRightButtonGroup->addAction(d->mMinimumCategoryButtonAction);
        }
    }

    d->mMinimumCategoryButtonAction->setVisible(isShow);
}

```