# 设置页面实现

## Context
主窗口6区布局中，活动栏应有3个功能按钮（资源管理器/全局搜索/设置），当前只有资源管理器和硬件树有实际页面，设置页面完全缺失。需要在侧边栏添加设置页面，让用户通过UI修改ConfigManager中的配置项。

## 方案：VS Code风格设置页 — 左侧分类树 + 右侧设置面板

```
┌──────────────┬─────────────────────────────────────────┐
│  资源管理器   │  设置                                    │ <- title_label_
├──────────────┼─────────────────────────────────────────┤
│  ■ 常用      │  编辑器                                  │ <- 分类名标题
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
└──────────────┴─────────────────────────────────────────┘
```

### 布局
- 左侧：QTreeWidget分类树（常用/编辑器/终端/外观），宽度固定200px
- 右侧：QScrollArea内放QFormLayout，每个配置项一行（QLabel描述 + 控件）
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

### Step 1: 创建设置齿轮SVG图标
- 新建 `src/app/resources/icons/svg/settings_dark.svg`（白色描边齿轮，24x24 viewBox）
- 新建 `src/app/resources/icons/svg/settings_light.svg`（黑色描边齿轮，同上）
- 在 `src/app/resource.qrc` 中注册

### Step 2: 新建 SettingsWidget — `src/app/SettingsWidget.h/.cpp`
- QWidget子类，水平布局：左侧QTreeWidget + 右侧QScrollArea
- `initUi()`：创建分类树和设置面板
- `initSignals()`：树节点切换→右侧面板切换、控件值变化→ConfigManager::set()
- 每个分类对应一个QWidget（QFormLayout），放入QStackedWidget
- "常用"分类直接显示编辑器+终端+外观的所有配置项（合并视图）
- 控件值变化时调用 `ConfigManager::instance().set(key, value)`
- ConfigManager::configChanged信号触发时更新控件显示（双向绑定）

### Step 3: 更新 ActivityBarWidget — `src/app/ActivityBarWidget.cpp`
- 在setupUi()末尾追加设置按钮（index 6）
- 图标：settings_dark.svg / settings_light.svg
- Tooltip："设置"

### Step 4: 更新 SidebarWidget — `src/app/SidebarWidget.h/.cpp`
- 新增 `SettingsWidget* settings_page_` 成员
- setupUi()中创建SettingsWidget并添加到stack_（index 6）
- view_titles_ 追加 "设置"

### Step 5: 更新 CMake — `src/app/CMakeLists.txt`
- 添加 SettingsWidget.h / SettingsWidget.cpp 到 SOURCES/HEADERS

### Step 6: 添加 SettingsWidget QSS — `src/app/resources/styles/vscode.qss`
- SettingsWidget QTreeWidget：左侧分类树窄宽度样式
- SettingsWidget QScrollArea：右侧面板背景
- 设置项标题QLabel样式（分组标题加粗）

## 修改文件清单
1. `src/app/resources/icons/svg/settings_dark.svg` — 新建
2. `src/app/resources/icons/svg/settings_light.svg` — 新建
3. `src/app/resource.qrc` — 注册新SVG
4. `src/app/SettingsWidget.h` — 新建
5. `src/app/SettingsWidget.cpp` — 新建
6. `src/app/ActivityBarWidget.cpp` — 添加设置按钮
7. `src/app/SidebarWidget.h` — 添加settings_page_成员
8. `src/app/SidebarWidget.cpp` — 添加设置页面到stack
9. `src/app/CMakeLists.txt` — 添加SettingsWidget源文件
10. `src/app/resources/styles/vscode.qss` — 添加设置页样式

## 验证
1. 编译通过
2. 活动栏底部显示齿轮设置图标
3. 点击齿轮图标，侧边栏切换到设置页面
4. 左侧分类树显示：常用/编辑器/终端/外观
5. 切换分类，右侧面板显示对应配置项
6. 修改配置项值，ConfigManager中值更新
7. 重启程序，配置值已持久化
8. 再次点击齿轮图标，侧边栏隐藏（toggle行为）
