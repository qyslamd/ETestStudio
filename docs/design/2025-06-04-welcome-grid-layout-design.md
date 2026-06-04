# Welcome 页面磁贴布局改造设计

## 背景

将 WelcomeWidget 从传统 VBoxLayout 改造为基于 drag-and-drop 网格的磁贴布局。从
`qt-preview-demo/todo/test_grid_layout` 移植网格布局引擎。

## 布局方案（选项 A）

```
┌───────────────────┬──────────────┬──────────────┐
│  Logo + 版本 (2x1)│  新建项目    │  打开项目     │
│                   │   (1x1)      │   (1x1)       │
├───────────────────┴──────────────┴──────────────┤
│              EyeWidget (4x1)                     │
├─────────────┬─────────────┬──────────────────────┤
│  最近项目 A  │  最近项目 B  │   时钟 (2x2)        │
│   (1x1)     │   (1x1)     │                      │
├─────────────┴─────────────┤                      │
│  最近项目 C  │  最近项目 D  │                      │
│   (1x1)     │   (1x1)     │                      │
├───────────────────────────┴──────────────────────┤
│             每日提示 (4x1)                        │
└──────────────────────────────────────────────────┘
```

## 网格参数

- 列数：4
- 基础磁贴尺寸：130 x 130
- 间距：10px
- 外边距：24px
- 圆角：8px

## 移植的源文件

所有 grid 组件位于 `src/app/grid/`：

| 文件                          | 来源                               |
| ----------------------------- | ---------------------------------- |
| `grid_global_def.hpp`         | 类型、常量、工具函数               |
| `test_gridly_layout.h/.cpp`  | 自定义 QLayout                     |
| `test_grid_label.h/.cpp`     | 磁贴容器（无拖放简化版）           |
| `layout_calculator_base.*`   | 布局计算器基类 + 策略模式           |
| `layout_calculator_v1.*`     | 自动排列算法                       |
| `test_grid_utils.h/.cpp`     | 工具函数（圆角图片等）              |
| `test_grid_animator.h/.cpp`  | 磁贴位移动画                        |

## WelcomeWidget 改造

### 替换内容
- 移除 `center_widget_`（居中容器）
- 移除 `recent_scroll_` / `recent_container_`（滚动区域）
- 替换为主 `TestGridlyLayout`
- 每个区域变成 `TestGridLabel` + 内部 `setContentWidget()`

### 磁贴映射

| 原组件              | 类型 | 点击事件                      |
| ------------------- | ---- | ----------------------------- |
| Logo + 版本号       | 2x1  | 无                            |
| 新建项目按钮        | 1x1  | `newProjectRequested()`       |
| 打开项目按钮        | 1x1  | `openProjectRequested()`      |
| EyeWidget           | 4x1  | 无（互动跟随鼠标）            |
| 最近项目 ×N         | 1x1  | `projectOpenRequested(path)`  |
| 时钟（新增）        | 2x2  | 无（装饰）                    |
| 每日提示            | 4x1  | 点击切换下一条                |

### 信号连接
- 磁贴 `clicked()` → WelcomeWidget 原有信号
- 最近项目动态增删 → `layout_->addWidget()` / `removeWidget()`
- 点击每日提示 → `showRandomTip()`

## 样式约定

- 所有 `TestGridLabel` 样式通过 `objectName` + QSS 控制
- 不在 C++ 中调用 `setStyleSheet()`
- QSS 选择器命名：`WelcomeTile*`, `WelcomeTileClock`, `WelcomeTileTip` 等

## 新建文件

- `src/app/grid/grid_global_def.hpp`
- `src/app/grid/test_gridly_layout.h`
- `src/app/grid/test_gridly_layout.cpp`
- `src/app/grid/test_grid_label.h`
- `src/app/grid/test_grid_label.cpp`
- `src/app/grid/layout_calculator_base.h`
- `src/app/grid/layout_calculator_base.cpp`
- `src/app/grid/layout_calculator_v1.h`
- `src/app/grid/layout_calculator_v1.cpp`
- `src/app/grid/test_grid_utils.h`
- `src/app/grid/test_grid_utils.cpp`
- `src/app/grid/test_grid_animator.h`
- `src/app/grid/test_grid_animator.cpp`

## 修改的文件

- `src/app/WelcomeWidget.h` — 替换成员变量
- `src/app/WelcomeWidget.cpp` — 重写 initUi()
- `src/app/CMakeLists.txt` — 添加新源文件
- `src/app/resources/styles/default.qss` — 添加磁贴样式
- `src/app/resources/styles/vscode.qss` — 添加磁贴样式（暗色）
