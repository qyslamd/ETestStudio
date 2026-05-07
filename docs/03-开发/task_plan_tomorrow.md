# 设置页面实现（v2 — 独立弹窗）

## Context
主窗口6区布局中，活动栏应有3个功能按钮（资源管理器/全局搜索/设置），当前只有资源管理器和硬件树有实际页面，设置页面完全缺失。

**设计变更 v1 → v2**：vs 嵌入 SidebarWidget 的侧边栏页，改为**独立模态/非模态弹窗**（类似 VS Code 当前版本的 Settings 以独立 Tab 打开，但我们这里简化为 QDialog）。活动栏齿轮按钮直接打开此弹窗，不再切换侧边栏。

## 方案：独立 Dialog — 左侧分类树 + 右侧设置面板

```
 ┌─────────────────────────────────────────────────────────┐
 │  设置                                           [ × ]  │ <- 窗口标题栏
 ├──────────────┬──────────────────────────────────────────┤
 │  ■ 常用      │  编辑器                                  │ <- 分组标题
 │    编辑器    │                                          │
 │    终端      │  字体大小        [  11  ] ▲▼            │
 │    外观      │  显示行号        ☑                       │
 │              │  自动缩进        ☑                       │
 │              │  Tab宽度         [  4  ] ▲▼            │
 │              │  空格替代Tab     ☑                       │
 │              │                                          │
 │              │  终端                                    │
 │              │                                          │
 │              │  Shell           [cmd.exe      ] ▼      │
 │              │  字体大小        [  11  ] ▲▼            │
 │              │  滚动缓冲        [10000  ] ▲▼          │
 │              │                                          │
 │              │  外观                                    │
 │              │                                          │
 │              │  窗口布局        [恢复默认]              │
 │              │  工具栏可见      ☑                       │
 │              │                                          │
 └──────────────┴──────────────────────────────────────────┘
```

### 布局
- 整体：QDialog，固定初始大小 ~700x500，居中显示
- 内容水平分割：左侧 QTreeWidget 分类树（宽 180px）+ 右侧 QScrollArea
- 右侧：QScrollArea > QStackedWidget，每个分类对应一个 QWidget（QVBoxLayout + 行控件）
- 控件类型：QSpinBox（数值）、QCheckBox（开关）、QComboBox（选择）、QPushButton（操作按钮）

### 分类与配置项映射

| 分类 | 配置项 | ConfigKey | 控件 |
|------|--------|-----------|------|
| **常用** | 显示所有配置项的精简视图 | — | 点击后跳转到对应分类 |
| **编辑器** | 字体大小 | `editor/font_size` | QSpinBox(8-72) |
| | 显示行号 | `editor/show_line_number` | QCheckBox |
| | 自动缩进 | `editor/auto_indent` | QCheckBox |
| | Tab宽度 | `editor/tab_width` | QSpinBox(2-8) |
| | 空格替代Tab | `editor/spaces_for_tab` | QCheckBox |
| **终端** | Shell | `terminal/shell` | QComboBox(cmd.exe/powershell.exe/bash) |
| | 字体大小 | `terminal/font_size` | QSpinBox(8-24) |
| | 滚动缓冲行数 | `terminal/scrollback` | QSpinBox(100-100000) |
| **外观** | 工具栏可见 | `toolbar/visible` | QCheckBox |
| | 恢复窗口布局 | — | QPushButton(恢复默认) |

## 实现步骤

### Step 1: 创建设置齿轮SVG图标（已完成）
- 新建 `src/app/resources/icons/svg/settings_dark.svg`
- 新建 `src/app/resources/icons/svg/settings_light.svg`
- 在 `src/app/resource.qrc` 中注册（已完成）

### Step 2: 修改 SettingsWidget — `src/app/SettingsWidget.h/.cpp`
- 基类由 QWidget 改为 QDialog
- 构造函数增加 `parent` 参数
- `initUi()`：补充窗口属性设置
  - `setWindowTitle("设置")`
  - `setModal(false)`（非模态，不阻塞主窗口）
  - `resize(700, 500)`
  - 窗口标志去掉 Qt::WindowContextHelpButtonHint
- 删除原来 inline 的 section header styleSheet（已有 QSS 接管）
- 内容布局不变：左侧 QTreeWidget + 右侧 QScrollArea

### Step 3: 更新 ActivityBarWidget — `src/app/ActivityBarWidget.cpp`
- 设置按钮从底部按钮区移除（按钮不再由 ActivityBarWidget 管理）
- 改为由 **MainWindow** 监听活动栏的 activityBarClicked 信号，识别到 settings index 后自行打开 SettingsDialog
- 或者：ActivityBarWidget 在 setupUi() 末尾给底部追加设置按钮（保留已实现的代码），但是连接改为 emit 一个独立信号 `settingsTriggered()`
- **推荐方案**：保留 Step 1-2 已做的 ActivityBarWidget 改动（底部设置按钮），新增信号 `void settingsTriggered();` 声明，按钮点击时 emit 该信号而非使用通用 `activityClicked`。MainWindow 连接此信号。

### Step 4: 撤销 SidebarWidget 变更
- `src/app/SidebarWidget.h`：删除 `SettingsWidget` 前置声明和 `settings_page_` 成员
- `src/app/SidebarWidget.cpp`：删除 `#include "SettingsWidget.h"`、页6设置页面添加代码、`view_titles_` 中 "设置" 条目

### Step 5: 更新 MainWindow — `src/app/MainWindow.cpp`
- 添加 `#include "SettingsWidget.h"`
- 在 initSignals() 中连接活动栏的设置信号
  ```cpp
  connect(activity_bar_, &ActivityBarWidget::settingsTriggered, this, [this]() {
    if (!settings_dialog_) {
      settings_dialog_ = new SettingsWidget(this);
    }
    settings_dialog_->show();
    settings_dialog_->raise();
    settings_dialog_->activateWindow();
  });
  ```
- 新增 `SettingsWidget* settings_dialog_ = nullptr;` 成员变量（MainWindow.h）

### Step 6: 更新 MainWindow.h
- 新增前置声明 `class SettingsWidget;`
- 新增成员 `SettingsWidget* settings_dialog_ = nullptr;`

### Step 7: 更新 CMake — `src/app/CMakeLists.txt`
- 已有 SettingsWidget.h / SettingsWidget.cpp 条目（无需再改）

### Step 8: 更新 QSS — `src/app/resources/styles/vscode.qss`
- SettingsWidget 样式可保留，但改为针对 SettingsDialog（QDialog 子类）
- 新增 SettingsDialog 的窗口背景样式

## 修改文件清单
1. `src/app/resources/icons/svg/settings_dark.svg` — ✅ 已完成
2. `src/app/resources/icons/svg/settings_light.svg` — ✅ 已完成
3. `src/app/resource.qrc` — ✅ 已完成
4. `src/app/SettingsWidget.h` — 基类 QWidget → QDialog，增加 settingsTriggered 等
5. `src/app/SettingsWidget.cpp` — initUi() 补充窗口属性，其余内容布局不变
6. `src/app/ActivityBarWidget.h` — 新增 `settingsTriggered()` 信号声明
7. `src/app/ActivityBarWidget.cpp` — 设置按钮连接改为 emit settingsTriggered()
8. `src/app/MainWindow.h` — 新增 SettingsWidget 前置声明 + settings_dialog_ 成员
9. `src/app/MainWindow.cpp` — 连接 activity_bar_ 的设置信号打开对话框
10. `src/app/SidebarWidget.h` — 还原：删除 SettingsWidget 前置声明和 settings_page_
11. `src/app/SidebarWidget.cpp` — 还原：删除 SettingsWidget include 和页6
12. `src/app/resources/styles/vscode.qss` — 微调 SettingsDialog 样式
13. `src/app/CMakeLists.txt` — ✅ 已有 SettingsWidget 条目

## 验证
1. 编译通过
2. 活动栏底部显示齿轮设置图标
3. 点击齿轮图标，弹出独立设置窗口
4. 窗口标题为"设置"，大小约 700x500
5. 左侧分类树显示：常用/编辑器/终端/外观
6. 切换分类，右侧面板显示对应配置项
7. 修改配置项值，ConfigManager 中值更新
8. 重启程序，配置值已持久化
9. 窗口可以非模态操作，不影响主窗口交互
10. 再次点击齿轮图标，设置窗口置前
